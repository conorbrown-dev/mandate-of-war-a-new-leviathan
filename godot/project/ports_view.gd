extends Node3D
## Presents the native ports: the quay model, a name and holder label, and the
## progress of a capture. The simulation owns every rule (who holds a port,
## whether a ship may unload, whether the lane is open); this only draws what
## the viewer's side knows. A port is a public coastal site, so every port is
## drawn from the start, but its holder is only updated while the viewer can see
## the quay (or holds it), then remembered.

const PORT_MODEL := "res://assets/source_3d/units/port.glb"
const NEUTRAL_COLOR := Color("#d9d3c4")
const LABEL_HEIGHT := 40.0

var extension: Object
var viewer := 0
var height_at: Callable
var can_see: Callable
var friendly_color := Color(0.12, 0.62, 1.0)
var hostile_color := Color(0.92, 0.20, 0.18)

## id -> {id, name, quay, berth, entry, owner (true), known_owner, view, label, ring}
var ports: Dictionary = {}
var active := false
## Set when a port changes hands in the viewer's sight; main shows it.
var pending_notice := ""

var _model: PackedScene


func setup(native: Object, viewer_faction: int, terrain_height: Callable, sight: Callable) -> void:
	extension = native
	viewer = viewer_faction
	height_at = terrain_height
	can_see = sight
	var resource: Resource = load(PORT_MODEL)
	_model = resource if resource is PackedScene else null


func clear() -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	ports.clear()
	active = false
	pending_notice = ""


## Hands the scenario's ports to the simulation and builds their views.
## Each definition is {id, name, quay [x, y], berth [x, y], entry [x, y], owner}.
func load_ports(definitions: Array) -> bool:
	clear()
	for definition: Dictionary in definitions:
		var id := int(definition.get("id", 0))
		var quay := Vector2(float(definition.quay[0]), float(definition.quay[1]))
		var berth := Vector2(float(definition.berth[0]), float(definition.berth[1]))
		var entry := Vector2(float(definition.entry[0]), float(definition.entry[1]))
		var owner := int(definition.get("owner", -1))
		if not bool(extension.call("port_add", id, quay.x, quay.y, berth.x, berth.y, entry.x, entry.y, owner)):
			push_error("Port %d was refused" % id)
			continue
		ports[id] = {
			"id": id, "name": String(definition.get("name", "PORT")), "quay": quay, "berth": berth, "entry": entry,
			"owner": owner, "known_owner": owner if owner == viewer else -1, "hold": 0.0, "claimant": -1,
			"view": null, "label": null, "ring": null,
		}
	active = not ports.is_empty()
	for port: Dictionary in ports.values():
		_build_view(port)
	sync()
	return active


func owner_color(owner: int) -> Color:
	if owner < 0:
		return NEUTRAL_COLOR
	return friendly_color if owner == viewer else hostile_color


func holder_text(owner: int) -> String:
	if owner < 0:
		return "NEUTRAL"
	return "YOURS" if owner == viewer else "ENEMY"


## Pulls the real state and updates what the viewer knows of each port.
func sync() -> void:
	if not active or extension == null:
		return
	for entry: Dictionary in extension.call("ports_state"):
		var port: Dictionary = ports.get(int(entry.id), {})
		if port.is_empty():
			continue
		var truth := int(entry.owner)
		var watching: bool = truth == viewer or bool(can_see.call(port.quay.x, port.quay.y))
		port.owner = truth
		port.hold = float(entry.hold)
		port.claimant = int(entry.claimant)
		if watching and int(port.known_owner) != truth:
			if int(port.known_owner) != -1 or truth != -1:
				pending_notice = "%s: %s" % [String(port.name), "NOW HELD BY YOU" if truth == viewer else "NOW HELD BY THE ENEMY" if truth >= 0 else "NOW NEUTRAL"]
			port.known_owner = truth
		_refresh_view(port, watching)


func _build_view(port: Dictionary) -> void:
	var quay: Vector2 = port.quay
	var berth: Vector2 = port.berth
	var root := Node3D.new()
	root.name = "Port_%d" % int(port.id)
	var ground := float(height_at.call(quay.x, quay.y))
	root.position = Vector3(quay.x, ground, quay.y)
	# The model's quay edge faces +X; turn it toward the water.
	var toward := berth - quay
	root.rotation.y = atan2(-toward.y, toward.x)
	add_child(root)
	if _model != null:
		root.add_child(_model.instantiate())
	var ring := MeshInstance3D.new()
	ring.name = "Ring"
	var torus := TorusMesh.new()
	torus.inner_radius = 130.0
	torus.outer_radius = 140.0
	ring.mesh = torus
	ring.position = Vector3(0.0, 2.0, 0.0)
	ring.rotation.y = -root.rotation.y
	ring.material_override = _ring_material(NEUTRAL_COLOR)
	root.add_child(ring)
	var label := Label3D.new()
	label.name = "Label"
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.fixed_size = true
	label.no_depth_test = true
	label.pixel_size = 0.0011
	label.font_size = 36
	label.outline_size = 8
	label.position = Vector3(0.0, LABEL_HEIGHT, 0.0)
	root.add_child(label)
	port.view = root
	port.label = label
	port.ring = ring


func _refresh_view(port: Dictionary, watching: bool) -> void:
	if port.view == null:
		return
	var shown_owner := int(port.known_owner)
	var color := owner_color(shown_owner)
	var label: Label3D = port.label
	var progress := ""
	if watching and int(port.claimant) >= 0 and float(port.hold) > 0.0:
		progress = "  ·  TAKING %d%%" % int(float(port.hold) * 100.0)
	label.text = "%s  ·  %s%s" % [String(port.name), holder_text(shown_owner), progress]
	label.modulate = color
	label.outline_modulate = Color(0.04, 0.05, 0.06, 0.9)
	var ring: MeshInstance3D = port.ring
	(ring.material_override as StandardMaterial3D).albedo_color = color


func _ring_material(color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = color
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material


func take_notice() -> String:
	var notice := pending_notice
	pending_notice = ""
	return notice
