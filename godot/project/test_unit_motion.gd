extends SceneTree
## How units are drawn moving: each model's nose points along its travel, the
## drawn position and heading are blended between 50 ms simulation ticks
## instead of jumping, and vehicles sit on the slope beneath them.

const ELITE_MBT := 0
const Registry = preload("res://visual_definition_registry.gd")
const VisualRoot = preload("res://unit_visual_root.gd")
# Swept-wing aircraft are as wide as they are long, so their long axis is
# ambiguous; their noses were checked by eye.
const SQUARE_PLANFORMS := ["visual.elite.fighter.prototype"]
# No ground vehicle in the game is longer than this; a donor pack that lays
# several vehicles side by side is.
const LONGEST_VEHICLE_M := 14.0

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _bounds(node: Node, transform: Transform3D, merged: Array) -> void:
	var local := transform
	if node is Node3D:
		local = transform * (node as Node3D).transform
	if node is MeshInstance3D and (node as MeshInstance3D).mesh != null:
		var box: AABB = local * (node as MeshInstance3D).mesh.get_aabb()
		merged[0] = box if merged[0] == null else (merged[0] as AABB).merge(box)
	for child in node.get_children():
		_bounds(child, local, merged)


# Every unit is drawn as one vehicle, nose forward. The game draws forward
# along -Z and a vehicle is longer than it is wide, so once the unit's visual
# is built its long axis must lie along Z, not X. Measured on the model as the
# game builds it, after any pack isolation, rotation and scale.
func _test_model_facing() -> void:
	var registry = Registry.new()
	registry.load_definitions()
	var definitions = JSON.parse_string(FileAccess.get_file_as_string("res://visuals/visual_definitions.json"))
	var measured := 0
	for definition in definitions.visual_definitions:
		var visual_id := String(definition.visual_id)
		if visual_id.contains(".nature."):
			continue
		var unit = VisualRoot.new()
		root.add_child(unit)
		var loaded: bool = unit.configure(registry, visual_id, Color.WHITE)
		check(loaded, "%s loads its model" % visual_id)
		var merged := [null]
		_bounds(unit.get_node("ModelRoot"), Transform3D.IDENTITY, merged)
		unit.free()
		if merged[0] == null:
			continue
		var size: Vector3 = (merged[0] as AABB).size
		measured += 1
		var naval := visual_id.contains("destroyer") or visual_id.contains("carrier")
		if not naval and not visual_id.contains("fighter") and not visual_id.contains("vtol"):
			check(maxf(size.x, size.z) <= LONGEST_VEHICLE_M, "%s is one vehicle, not a pack (%.1f m long)" % [visual_id, maxf(size.x, size.z)])
		if definition.has("isolate"):
			var expected := float(definition.isolate.length_m) * float(definition.scale[0])
			check(absf(maxf(size.x, size.z) - expected) <= expected * 0.05, "%s is drawn at its real length (%.1f m, expected %.1f m)" % [visual_id, maxf(size.x, size.z), expected])
		if not SQUARE_PLANFORMS.has(visual_id):
			check(size.z >= size.x * 1.15, "%s faces sideways: %.1f m along X but %.1f m along forward Z" % [visual_id, size.x, size.z])
	check(measured >= 15, "unit models were measured (%d)" % measured)


func _drawn(view: Node, entity_id: int) -> Node3D:
	return view.prototype_visual_views.get(entity_id) as Node3D


func _test_motion(view: Node) -> void:
	var extension: Object = view.extension
	var engineer: Array = extension.call("get_unit_position", int(view._player_engineer_id()))
	# The level starting pad: flat ground, so only steering moves the hull.
	var start := Vector2(float(engineer[0]) + 60.0, float(engineer[1]) - 60.0)
	var tank := int(extension.call("create_unit_with_type", start.x, start.y, ELITE_MBT, 0))
	check(tank > 0, "a tank spawns on the starting pad")
	if tank <= 0:
		return
	view._sync_new_entities()
	view._sync_unit_transforms()
	# Destination a quarter turn off the tank's initial heading (+Z).
	var destination := start + Vector2(160.0, 0.0)
	extension.call("issue_move_commands", PackedInt32Array([tank]), 0, destination.x, destination.y, 10.0)
	var positions: Array[Vector2] = []
	var headings: Array[float] = []
	# Frames of 10 ms: five per simulation tick, as at 100 fps.
	for _frame in range(600):
		extension.call("update_simulation", 10.0)
		view._sync_unit_transforms()
		var drawn := _drawn(view, tank)
		positions.append(Vector2(drawn.global_position.x, drawn.global_position.z))
		var forward := -drawn.global_basis.z
		headings.append(atan2(forward.x, forward.z))
	# Smooth travel: once moving, every frame moves the drawn tank a similar
	# small step. Without blending, four of every five frames would not move
	# and the fifth would jump a whole tick.
	var steps: Array[float] = []
	for index in range(300, positions.size()):
		steps.append(positions[index].distance_to(positions[index - 1]))
	steps.sort()
	var median: float = steps[steps.size() / 2]
	var largest: float = steps[steps.size() - 1]
	check(median > 0.005, "the drawn tank moves on most frames (median step %.4f m)" % median)
	check(largest <= median * 1.6, "the drawn tank moves without tick-sized jumps (largest step %.4f m, median %.4f m)" % [largest, median])
	# Smooth turning: no frame turns the hull by more than a frame's share of
	# its fastest turn rate (2.4 rad/s for this tank), with margin.
	var largest_turn := 0.0
	for index in range(1, headings.size()):
		largest_turn = maxf(largest_turn, absf(angle_difference(headings[index - 1], headings[index])))
	check(largest_turn <= 2.4 * 0.010 * 1.6, "the drawn hull turns gradually (largest turn %.4f rad per 10 ms frame)" % largest_turn)
	# Facing travel: the drawn hull points at the destination while driving.
	var drawn := _drawn(view, tank)
	var to_destination := (destination - Vector2(drawn.global_position.x, drawn.global_position.z)).normalized()
	var facing := Vector2(-drawn.global_basis.z.x, -drawn.global_basis.z.z).normalized()
	check(facing.dot(to_destination) >= 0.98, "the drawn hull faces the destination it drives to (alignment %.3f)" % facing.dot(to_destination))


func _test_slope(view: Node) -> void:
	var extension: Object = view.extension
	var engineer: Array = extension.call("get_unit_position", int(view._player_engineer_id()))
	var origin := Vector2(float(engineer[0]), float(engineer[1]))
	var slope := Vector2.INF
	var normal := Vector3.UP
	for ring in range(4, 60):
		for step in range(24):
			var point := origin + Vector2.from_angle(TAU * step / 24.0) * ring * 100.0
			if not bool(extension.call("is_land_position", point.x, point.y)):
				continue
			var candidate: Vector3 = view._terrain_normal_at(point.x, point.y)
			if rad_to_deg(candidate.angle_to(Vector3.UP)) >= 6.0 and rad_to_deg(candidate.angle_to(Vector3.UP)) <= 25.0:
				slope = point
				normal = candidate
				break
		if slope != Vector2.INF:
			break
	check(slope != Vector2.INF, "a sloped, drivable hillside exists near the start")
	if slope == Vector2.INF:
		return
	var tank := int(extension.call("create_unit_with_type", slope.x, slope.y, ELITE_MBT, 0))
	check(tank > 0, "a tank spawns on the hillside")
	if tank <= 0:
		return
	view._sync_new_entities()
	extension.call("update_simulation", 50.0)
	view._sync_unit_transforms()
	var drawn := _drawn(view, tank)
	var position := Vector2(drawn.global_position.x, drawn.global_position.z)
	var expected: Vector3 = view._terrain_normal_at(position.x, position.y)
	check(drawn.global_basis.y.normalized().dot(expected) >= 0.9995,
		"a tank on a %.1f degree slope tilts with the ground (up vector off by %.2f degrees)" % [rad_to_deg(expected.angle_to(Vector3.UP)), rad_to_deg(drawn.global_basis.y.angle_to(expected))])


func _run() -> void:
	_test_model_facing()
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	await process_frame
	# The test steps the simulation itself.
	view.set_process(false)
	check(view.match_started, "the skirmish starts")
	if view.match_started:
		_test_motion(view)
		_test_slope(view)
	view.queue_free()
	await process_frame
	print("UNIT_MOTION checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
