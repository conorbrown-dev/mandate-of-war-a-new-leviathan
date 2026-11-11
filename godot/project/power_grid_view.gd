extends Node3D
## Presents the native power grid: stations, wooden poles, steel pylons, their
## conductors and the sparks of a cut line. The simulation owns every rule
## (who holds what, what is cut, what a repair costs); this only draws what the
## viewer's side knows. A station or line is drawn once it has been seen, and
## then shows the holder and condition it had when it was last seen. A side
## always knows about its own lines.

const MODELS_PATH := "res://assets/source_3d/power/power_models.json"
const KIND_NAMES := {"station": 0, "junction": 1, "load": 2}
const LINE_KIND_NAMES := {"wood": 0, "steel": 1}
const STATION := 0
const LOAD := 2
const STEEL := 1
# Orders, as the native bridge numbers them.
const ORDER_CAPTURE_STATION := 1
const ORDER_CAPTURE_LINE := 2
const ORDER_SEVER_LINE := 3
const ORDER_REPAIR_LINE := 4
# Lines keep clear of a station's buildings: the first pylon stands this far out.
const STATION_CLEARANCE := 110.0
const WIRE_SEGMENTS := 8
const SPARK_INTERVAL_SECONDS := 2.4
const NEUTRAL_COLOR := Color("#d9d3c4")
const WIRE_COLOR := Color("#1d2326")
const DOWN_WIRE_COLOR := Color("#8a6a3c")

var extension: Object
var viewer := 0
var height_at: Callable
var can_see: Callable
var friendly_color := Color(0.12, 0.62, 1.0)
var hostile_color := Color(0.92, 0.20, 0.18)

var nodes: Dictionary = {}
var lines: Dictionary = {}
var active := false
# Set when something the viewer would be told about happens; main shows it.
var pending_notice := ""
var spark_bursts := 0

var _models: Dictionary = {}
var _meshes: Dictionary = {}
var _spark_clock := 0.0
var _native_notice_seen := ""


func setup(native: Object, viewer_faction: int, terrain_height: Callable, sight: Callable) -> void:
	extension = native
	viewer = viewer_faction
	height_at = terrain_height
	can_see = sight
	var file := FileAccess.open(MODELS_PATH, FileAccess.READ)
	var parsed: Variant = JSON.parse_string(file.get_as_text()) if file != null else null
	_models = parsed if parsed is Dictionary else {}


func clear() -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	nodes.clear()
	lines.clear()
	active = false
	pending_notice = ""
	_native_notice_seen = ""
	spark_bursts = 0


## Reads a scenario's grid, hands it to the simulation and prepares the views.
func load_grid(path: String) -> bool:
	clear()
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return false
	var content: Variant = JSON.parse_string(file.get_as_text())
	if not content is Dictionary:
		push_warning("Power grid data is malformed")
		return false
	for definition: Dictionary in content.get("nodes", []):
		var id := int(definition.get("id", 0))
		var kind := int(KIND_NAMES.get(String(definition.get("kind", "junction")), 1))
		var coordinates: Array = definition.get("position", [0, 0])
		var position := Vector2(float(coordinates[0]), float(coordinates[1]))
		var owner := int(definition.get("owner", -1))
		if not bool(extension.call("power_add_node", id, kind, position.x, position.y, owner,
				float(definition.get("output_mw", 0.0)), float(definition.get("demand_mw", 0.0)))):
			push_error("Power grid node %d was refused" % id)
			continue
		var known := _starts_known(definition, owner)
		nodes[id] = {
			"id": id, "kind": kind, "name": String(definition.get("name", "POWER STATION")),
			"position": position, "owner": owner, "known_owner": owner if known else -1,
			"discovered": known, "output_mw": float(definition.get("output_mw", 0.0)),
			"powered": false, "view": null,
		}
	for definition: Dictionary in content.get("lines", []):
		var id := int(definition.get("id", 0))
		var from := int(definition.get("from", 0))
		var to := int(definition.get("to", 0))
		var kind := int(LINE_KIND_NAMES.get(String(definition.get("kind", "wood")), 0))
		var owner := int(definition.get("owner", -1))
		var via := PackedFloat32Array()
		for point: Array in definition.get("via", []):
			via.append(float(point[0]))
			via.append(float(point[1]))
		if not nodes.has(from) or not nodes.has(to) or \
				not bool(extension.call("power_add_line", id, from, to, kind, owner, via)):
			push_error("Power grid line %d was refused" % id)
			continue
		var path_points := PackedVector2Array([nodes[from].position])
		for index in range(0, via.size(), 2):
			path_points.append(Vector2(via[index], via[index + 1]))
		path_points.append(nodes[to].position)
		var known := _starts_known(definition, owner)
		lines[id] = {
			"id": id, "from": from, "to": to, "kind": kind, "path": path_points,
			"reason": String(definition.get("reason", "")),
			"owner": owner, "known_owner": owner if known else -1,
			"intact": true, "known_intact": true, "discovered": known,
			"cut": Vector2.ZERO, "known_cut": Vector2.ZERO,
			"repairing": false, "repair_remaining_ms": 0.0, "repair_cost": 0.0, "repair_seconds": 0.0,
			"work": 0, "length": 0.0, "supports": _support_points(path_points, kind, from, to),
			"view": null, "sparks": null, "label": null,
		}
	active = not nodes.is_empty()
	for node: Dictionary in nodes.values():
		_rebuild_node_view(node)
	for line: Dictionary in lines.values():
		_rebuild_line_view(line)
	sync()
	return active


func _starts_known(definition: Dictionary, owner: int) -> bool:
	if owner == viewer:
		return true
	for side in definition.get("known_to", []):
		if int(side) == viewer:
			return true
	return false


## Pulls the grid's real state and updates what the viewer knows of it.
func sync() -> void:
	if not active or extension == null:
		return
	var state: Dictionary = extension.call("power_grid_state", viewer)
	var notice := String(state.get("notice", ""))
	if notice != _native_notice_seen:
		_native_notice_seen = notice
		if not notice.is_empty():
			pending_notice = notice
	for entry: Dictionary in state.get("nodes", []):
		var node: Dictionary = nodes.get(int(entry.id), {})
		if node.is_empty():
			continue
		node.owner = int(entry.owner)
		node.powered = bool(entry.powered)
		if int(node.kind) != STATION:
			continue
		var position: Vector2 = node.position
		# Losing a station is known at once; anything else needs eyes on it.
		var told := int(node.owner) == viewer or int(node.known_owner) == viewer
		if not told and not bool(can_see.call(position.x, position.y)):
			continue
		if not bool(node.discovered) or int(node.known_owner) != int(node.owner):
			if int(node.known_owner) == viewer and int(node.owner) != viewer:
				pending_notice = "POWER STATION LOST // %s" % node.name
			elif int(node.owner) == viewer and bool(node.discovered):
				pending_notice = "POWER STATION SECURED // %s" % node.name
			elif not bool(node.discovered):
				pending_notice = "POWER STATION LOCATED // %s" % node.name
			node.discovered = true
			node.known_owner = int(node.owner)
			_rebuild_node_view(node)
	for entry: Dictionary in state.get("lines", []):
		var line: Dictionary = lines.get(int(entry.id), {})
		if line.is_empty():
			continue
		line.owner = int(entry.owner)
		line.intact = bool(entry.intact)
		line.cut = Vector2(float(entry.cut_x), float(entry.cut_y))
		line.repair_cost = float(entry.repair_cost)
		line.repair_seconds = float(entry.repair_seconds)
		line.length = float(entry.length)
		line.work = int(entry.work)
		var told := int(line.owner) == viewer or int(line.known_owner) == viewer
		if not told and not _line_in_sight(line):
			continue
		line.repairing = bool(entry.repairing)
		line.repair_remaining_ms = float(entry.repair_remaining_ms)
		var changed := not bool(line.discovered) or int(line.known_owner) != int(line.owner) or \
			bool(line.known_intact) != bool(line.intact)
		if changed:
			var was_ours := int(line.known_owner) == viewer
			if bool(line.known_intact) and not bool(line.intact):
				line.known_cut = line.cut
				if was_ours:
					pending_notice = "POWER LINE CUT // ENGINEER MUST REPAIR IT"
			elif not bool(line.known_intact) and bool(line.intact) and int(line.owner) == viewer:
				pending_notice = "POWER LINE REPAIRED"
			elif was_ours and int(line.owner) != viewer:
				pending_notice = "POWER LINE CAPTURED BY THE ENEMY"
			elif bool(line.discovered) and int(line.owner) == viewer:
				pending_notice = "POWER LINE SECURED"
			var newly_down := bool(line.known_intact) and not bool(line.intact)
			line.discovered = true
			line.known_owner = int(line.owner)
			line.known_intact = bool(line.intact)
			_rebuild_line_view(line)
			if newly_down:
				_burst_sparks(line)
		_update_line_label(line)


func _line_in_sight(line: Dictionary) -> bool:
	for point: Vector2 in line.supports:
		if bool(can_see.call(point.x, point.y)):
			return true
	return false


func _process(delta: float) -> void:
	if not active:
		return
	_spark_clock += delta
	if _spark_clock < SPARK_INTERVAL_SECONDS:
		return
	_spark_clock = 0.0
	# A dead line keeps arcing at the break until someone starts repairing it.
	for line: Dictionary in lines.values():
		if bool(line.discovered) and not bool(line.known_intact) and not bool(line.repairing):
			_burst_sparks(line)


func take_notice() -> String:
	var notice := pending_notice
	pending_notice = ""
	return notice


func owner_color(owner: int) -> Color:
	return friendly_color if owner == viewer else hostile_color if owner >= 0 else NEUTRAL_COLOR


func allegiance(owner: int) -> String:
	return "FRIENDLY" if owner == viewer else "HOSTILE" if owner >= 0 else "CIVILIAN"


## The known station nearest a map point, within `tolerance` metres.
func station_near(world: Vector2, tolerance: float) -> Dictionary:
	var best: Dictionary = {}
	var best_distance := tolerance
	for node: Dictionary in nodes.values():
		if int(node.kind) != STATION or not bool(node.discovered):
			continue
		var distance := world.distance_to(node.position)
		if distance <= best_distance:
			best_distance = distance
			best = node
	return best


## The known line nearest a map point, within `tolerance` metres.
func line_near(world: Vector2, tolerance: float) -> Dictionary:
	var best: Dictionary = {}
	var best_distance := tolerance
	for line: Dictionary in lines.values():
		if not bool(line.discovered):
			continue
		var distance := distance_to_line(line, world)
		if distance <= best_distance:
			best_distance = distance
			best = line
	return best


func distance_to_line(line: Dictionary, world: Vector2) -> float:
	var path_points: PackedVector2Array = line.path
	var best := INF
	for index in range(1, path_points.size()):
		var nearest := Geometry2D.get_closest_point_to_segment(world, path_points[index - 1], path_points[index])
		best = minf(best, world.distance_to(nearest))
	return best


## What the HUD shows for the station or line under the pointer.
func hover_context(world: Vector2, tolerance: float) -> Dictionary:
	var station := station_near(world, maxf(tolerance, 90.0))
	if not station.is_empty():
		var owner := int(station.known_owner)
		return {
			"key": "power_station:%d:%d" % [int(station.id), owner],
			"title": "%s // %s" % [allegiance(owner), station.name],
			"detail": "OUTPUT %.0f MW  //  2 THEN RIGHT-CLICK TO CAPTURE" % float(station.output_mw),
			"accent": owner_color(owner),
		}
	var line := line_near(world, tolerance)
	if line.is_empty():
		return {}
	var owner := int(line.known_owner)
	var kind := "STEEL TRANSMISSION LINE" if int(line.kind) == STEEL else "WOODEN POWER LINE"
	var detail := "2 THEN RIGHT-CLICK TO CAPTURE  //  3 TO CUT"
	if not bool(line.known_intact):
		detail = "LINE DOWN  //  REPAIR %.0f MATERIALS, %.0f S  //  2 THEN RIGHT-CLICK: THE ENGINEER GOES TO THE BREAK" % [
			float(line.repair_cost), float(line.repair_seconds)]
		if bool(line.repairing):
			detail = "REPAIR UNDER WAY  //  %.0f S" % (float(line.repair_remaining_ms) / 1000.0)
	elif owner == viewer:
		detail = "CARRYING POWER  //  3 THEN RIGHT-CLICK TO CUT"
	return {
		"key": "power_line:%d:%d:%s:%d" % [int(line.id), owner, str(line.known_intact), int(float(line.repair_remaining_ms) / 1000.0)],
		"title": "%s // %s" % [allegiance(owner), kind],
		"detail": detail,
		"accent": owner_color(owner),
	}


## Sends `unit` to work on whatever known station or line is at `world`.
## mode 1 captures (or, on a line that is down, repairs); mode 2 cuts.
## Returns {"handled": bool, "message": String}; unhandled means nothing of the
## grid is there and the click belongs to something else.
func order_at(world: Vector2, tolerance: float, mode: int, unit: int, engineer: int) -> Dictionary:
	var station := station_near(world, maxf(tolerance, 90.0))
	if mode == 1 and not station.is_empty():
		var refusal := String(extension.call("power_order", unit, viewer, ORDER_CAPTURE_STATION, int(station.id)))
		return {"handled": true, "message": "CAPTURING POWER STATION" if refusal.is_empty() else refusal}
	var line := line_near(world, maxf(tolerance, 30.0))
	if line.is_empty():
		return {"handled": false, "message": ""}
	var order := ORDER_SEVER_LINE if mode == 2 else ORDER_CAPTURE_LINE
	var sent := unit
	var accepted := "CUTTING POWER LINE" if mode == 2 else "CAPTURING POWER LINE"
	if mode == 1 and not bool(line.known_intact):
		order = ORDER_REPAIR_LINE
		sent = engineer
		accepted = "REPAIRING POWER LINE // %.0f MATERIALS" % float(line.repair_cost)
	var refusal := String(extension.call("power_order", sent, viewer, order, int(line.id)))
	return {"handled": true, "message": accepted if refusal.is_empty() else refusal}


# --- views -----------------------------------------------------------------

func _ground(point: Vector2) -> float:
	# Pylons that cross the strait stand on footings at the waterline.
	return maxf(float(height_at.call(point.x, point.y)), 0.0)


# A station's pad sits level at the highest ground under it; the model's deep
# foundation covers the fall of the slope.
func _station_ground(position: Vector2) -> float:
	var footprint: Array = _models.get("power_station", {}).get("footprint", [150.0, 96.0])
	var reach := Vector2(float(footprint[0]), float(footprint[1])).length() * 0.5
	var height := _ground(position)
	for step in range(8):
		var offset := Vector2.from_angle(float(step) * TAU / 8.0) * reach
		height = maxf(height, _ground(position + offset))
	return height


func _model(kind: int) -> Dictionary:
	return _models.get("steel_pylon" if kind == STEEL else "wooden_power_pole", {})


func _mesh(model_name: String) -> Mesh:
	if _meshes.has(model_name):
		return _meshes[model_name]
	var mesh: Mesh = null
	var scene: Variant = load(String(_models.get(model_name, {}).get("scene", "")))
	if scene is PackedScene:
		var instance: Node = scene.instantiate()
		var found := instance.find_children("*", "MeshInstance3D", true, false)
		if not found.is_empty():
			mesh = (found[0] as MeshInstance3D).mesh
		instance.free()
	_meshes[model_name] = mesh
	return mesh


# Where a line's poles or pylons stand: evenly along each leg of its route, a
# model's span apart, keeping clear of a station at either end.
func _support_points(path_points: PackedVector2Array, kind: int, from: int, to: int) -> Array[Vector2]:
	var span := float(_model(kind).get("span", 70.0 if kind != STEEL else 350.0))
	var points: Array[Vector2] = []
	for index in range(1, path_points.size()):
		var start := path_points[index - 1]
		var finish := path_points[index]
		var steps := maxi(1, int(ceilf(start.distance_to(finish) / span)))
		for step in range(0 if index == 1 else 1, steps + 1):
			points.append(start.lerp(finish, float(step) / float(steps)))
	var first_node: Dictionary = nodes[from]
	var last_node: Dictionary = nodes[to]
	var kept: Array[Vector2] = []
	for point in points:
		if int(first_node.kind) == STATION and point.distance_to(first_node.position) < STATION_CLEARANCE:
			continue
		if int(last_node.kind) == STATION and point.distance_to(last_node.position) < STATION_CLEARANCE:
			continue
		kept.append(point)
	return kept if kept.size() >= 2 else points


func _support_transform(supports: Array[Vector2], index: int) -> Transform3D:
	var point := supports[index]
	var ahead := supports[mini(index + 1, supports.size() - 1)] - supports[maxi(index - 1, 0)]
	# The model's conductors run along its Z axis.
	return Transform3D(Basis(Vector3.UP, atan2(ahead.x, ahead.y)), Vector3(point.x, _ground(point), point.y))


func _station_yaw(node: Dictionary) -> float:
	# The switchyard is on the model's +X side: turn it toward the first line out.
	for line: Dictionary in lines.values():
		var other := -1
		if int(line.from) == int(node.id):
			other = 1
		elif int(line.to) == int(node.id):
			other = (line.path as PackedVector2Array).size() - 2
		if other < 0:
			continue
		var toward: Vector2 = (line.path as PackedVector2Array)[other] - (node.position as Vector2)
		return atan2(-toward.y, toward.x)
	return 0.0


func station_terminal(node: Dictionary) -> Vector3:
	var position: Vector2 = node.position
	var terminal: Array = _models.get("power_station", {}).get("terminal", [60.0, 13.0, 0.0])
	var local := Vector3(float(terminal[0]), float(terminal[1]), float(terminal[2]))
	return Vector3(position.x, _station_ground(position), position.y) + Basis(Vector3.UP, _station_yaw(node)) * local


func _rebuild_node_view(node: Dictionary) -> void:
	if node.view != null and is_instance_valid(node.view):
		remove_child(node.view)
		node.view.queue_free()
	node.view = null
	if int(node.kind) != STATION or not bool(node.discovered):
		return
	var position: Vector2 = node.position
	var root := Node3D.new()
	root.name = "PowerStation_%d" % int(node.id)
	root.position = Vector3(position.x, _station_ground(position), position.y)
	root.rotation.y = _station_yaw(node)
	var mesh := _mesh("power_station")
	if mesh != null:
		var body := MeshInstance3D.new()
		body.name = "Model"
		body.mesh = mesh
		root.add_child(body)
	# Who holds it: a lit band round the top of each stack, and a label.
	var beacon_material := StandardMaterial3D.new()
	beacon_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	beacon_material.albedo_color = owner_color(int(node.known_owner))
	for x in [-58.0, -40.0]:
		var beacon := MeshInstance3D.new()
		beacon.name = "OwnerBeacon"
		var band := CylinderMesh.new()
		band.top_radius = 2.6
		band.bottom_radius = 2.6
		band.height = 2.0
		beacon.mesh = band
		beacon.material_override = beacon_material
		beacon.position = Vector3(x, 71.4, 8.0)
		root.add_child(beacon)
	var label := Label3D.new()
	label.name = "Label"
	label.text = "%s\n%.0f MW" % [node.name, float(node.output_mw)]
	label.font_size = 28
	label.pixel_size = 0.16
	label.outline_size = 3
	label.modulate = owner_color(int(node.known_owner))
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.position = Vector3(0.0, 84.0, 0.0)
	root.add_child(label)
	add_child(root)
	node.view = root


func _rebuild_line_view(line: Dictionary) -> void:
	if line.view != null and is_instance_valid(line.view):
		remove_child(line.view)
		line.view.queue_free()
	line.view = null
	line.sparks = null
	line.label = null
	if not bool(line.discovered):
		return
	var kind := int(line.kind)
	var model := _model(kind)
	var supports: Array[Vector2] = line.supports
	var root := Node3D.new()
	root.name = "PowerLine_%d" % int(line.id)
	root.set_meta("support_count", supports.size())
	root.set_meta("kind", kind)
	root.set_meta("intact", bool(line.known_intact))
	var transforms: Array[Transform3D] = []
	for index in range(supports.size()):
		transforms.append(_support_transform(supports, index))
	var mesh := _mesh("steel_pylon" if kind == STEEL else "wooden_power_pole")
	if mesh != null:
		var batch := MultiMesh.new()
		batch.transform_format = MultiMesh.TRANSFORM_3D
		batch.mesh = mesh
		batch.instance_count = transforms.size()
		for index in range(transforms.size()):
			batch.set_instance_transform(index, transforms[index])
		var supports_view := MultiMeshInstance3D.new()
		supports_view.name = "Supports"
		supports_view.multimesh = batch
		root.add_child(supports_view)
	# Conductors. A dead line is drawn broken: no wire across the cut span, and
	# every other length missing elsewhere so it reads as dead from far away.
	var attach: Array = model.get("attach", [[0.0, 10.0, 0.0]])
	var sag_ratio := 0.03 if kind == STEEL else 0.022
	var cut_span := -1
	if not bool(line.known_intact):
		var nearest := INF
		for index in range(1, supports.size()):
			var middle := (supports[index - 1] + supports[index]) * 0.5
			var distance := middle.distance_to(line.known_cut)
			if distance < nearest:
				nearest = distance
				cut_span = index
	var vertices := PackedVector3Array()
	for index in range(1, transforms.size()):
		if index == cut_span:
			continue
		for point: Array in attach:
			var local := Vector3(float(point[0]), float(point[1]), float(point[2]))
			_append_wire(vertices, transforms[index - 1] * local, transforms[index] * local, sag_ratio, not bool(line.known_intact))
	for end in [int(line.from), int(line.to)]:
		var node: Dictionary = nodes[end]
		if int(node.kind) != STATION or transforms.is_empty():
			continue
		var nearest_support := transforms[0] if end == int(line.from) else transforms[transforms.size() - 1]
		var terminal := station_terminal(node)
		for point: Array in attach:
			_append_wire(vertices, terminal, nearest_support * Vector3(float(point[0]), float(point[1]), float(point[2])),
				sag_ratio, not bool(line.known_intact))
	if not vertices.is_empty():
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = vertices
		var wire_mesh := ArrayMesh.new()
		wire_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_LINES, arrays)
		var material := StandardMaterial3D.new()
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		var owner := int(line.known_owner)
		material.albedo_color = DOWN_WIRE_COLOR if not bool(line.known_intact) else \
			WIRE_COLOR if owner < 0 else owner_color(owner).darkened(0.25)
		var wires := MeshInstance3D.new()
		wires.name = "Conductors"
		wires.mesh = wire_mesh
		wires.material_override = material
		wires.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		root.add_child(wires)
		root.set_meta("wire_vertices", vertices.size())
	if not bool(line.known_intact):
		var cut: Vector2 = line.known_cut
		var break_point := Vector3(cut.x, _ground(cut) + float(model.get("height", 11.0)) * 0.8, cut.y)
		var sparks := _make_sparks(kind)
		sparks.position = break_point
		root.add_child(sparks)
		line.sparks = sparks
		var label := Label3D.new()
		label.name = "Status"
		label.font_size = 26
		label.pixel_size = 0.07
		label.outline_size = 3
		label.modulate = Color("#ffb547")
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.position = break_point + Vector3(0.0, 9.0, 0.0)
		root.add_child(label)
		line.label = label
	add_child(root)
	line.view = root
	_update_line_label(line)


func _append_wire(vertices: PackedVector3Array, start: Vector3, finish: Vector3, sag_ratio: float, dashed: bool) -> void:
	var sag := start.distance_to(finish) * sag_ratio
	var previous := start
	for step in range(1, WIRE_SEGMENTS + 1):
		var along := float(step) / float(WIRE_SEGMENTS)
		var point := start.lerp(finish, along)
		point.y -= 4.0 * sag * along * (1.0 - along)
		if not dashed or step % 2 == 1:
			vertices.append(previous)
			vertices.append(point)
		previous = point


func _update_line_label(line: Dictionary) -> void:
	var label: Label3D = line.label
	if label == null or not is_instance_valid(label):
		return
	label.text = "REPAIRING %.0f S" % (float(line.repair_remaining_ms) / 1000.0) if bool(line.repairing) else "LINE DOWN"


# A burst of hot sparks thrown from the broken conductor.
func _make_sparks(kind: int) -> CPUParticles3D:
	var steel := kind == STEEL
	var sparks := CPUParticles3D.new()
	sparks.name = "Sparks"
	sparks.amount = 90 if steel else 56
	sparks.lifetime = 1.2
	sparks.one_shot = true
	sparks.explosiveness = 0.9
	sparks.randomness = 0.6
	sparks.lifetime_randomness = 0.5
	sparks.emitting = false
	sparks.local_coords = false
	sparks.direction = Vector3(0.0, 1.0, 0.0)
	sparks.spread = 85.0
	sparks.initial_velocity_min = 12.0 if steel else 7.0
	sparks.initial_velocity_max = 36.0 if steel else 20.0
	sparks.gravity = Vector3(0.0, -24.0, 0.0)
	sparks.damping_min = 0.4
	sparks.damping_max = 1.6
	sparks.scale_amount_min = 1.2 if steel else 0.6
	sparks.scale_amount_max = 3.0 if steel else 1.5
	var ramp := Gradient.new()
	ramp.set_color(0, Color(1.0, 1.0, 0.92, 1.0))
	ramp.add_point(0.35, Color(1.0, 0.82, 0.28, 1.0))
	ramp.add_point(0.75, Color(1.0, 0.42, 0.08, 0.85))
	ramp.set_color(ramp.get_point_count() - 1, Color(0.9, 0.2, 0.02, 0.0))
	sparks.color_ramp = ramp
	var quad := QuadMesh.new()
	quad.size = Vector2(0.5, 0.5)
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.vertex_color_use_as_albedo = true
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	material.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	material.billboard_keep_scale = true
	quad.material = material
	sparks.mesh = quad
	return sparks


func _burst_sparks(line: Dictionary) -> void:
	var sparks: CPUParticles3D = line.sparks
	if sparks == null or not is_instance_valid(sparks):
		return
	sparks.restart()
	sparks.emitting = true
	spark_bursts += 1
