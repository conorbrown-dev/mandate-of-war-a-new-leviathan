extends Node3D

# Presentation-only mirror of the native control-point battle snapshot. It has
# no movement, ownership, capture, or result logic.
const NEUTRAL := Color("#ffbd52")
const FRIENDLY := Color("#47d9ff")
const ENEMY := Color("#ff625a")
const CONTESTED := Color("#d88cff")

var _views: Array[Dictionary] = []


func sync(points: Array, terrain_height: Callable) -> void:
	while _views.size() < points.size():
		_views.append(_create_view(_views.size()))
	for index in range(_views.size()):
		var view: Dictionary = _views[index]
		var root: Node3D = view.root
		root.visible = index < points.size()
		if index >= points.size():
			continue
		var point: Dictionary = points[index]
		var x := float(point.get("x", 0.0))
		var z := float(point.get("y", 0.0))
		root.position = Vector3(x, terrain_height.call(x, z) + 0.15, z)
		var state := int(point.get("state", 0))
		var color := _color_for_state(state)
		(view.ring_material as StandardMaterial3D).albedo_color = color
		(view.ring_material as StandardMaterial3D).emission = color
		(view.beacon_material as StandardMaterial3D).albedo_color = color
		(view.beacon_material as StandardMaterial3D).emission = color
		var radius := maxf(1.0, float(point.get("radius", 1.0)))
		(view.ring as MeshInstance3D).scale = Vector3(radius, 1.0, radius)
		var label: Label3D = view.label
		label.text = "CONTROL %s  %.0f%%" % [_state_name(state), absf(float(point.get("capture_progress", 0.0))) * 100.0]
		label.modulate = color


func _create_view(index: int) -> Dictionary:
	var root := Node3D.new()
	root.name = "ControlPoint%d" % index
	add_child(root)
	var ring := MeshInstance3D.new()
	var ring_mesh := CylinderMesh.new()
	ring_mesh.top_radius = 1.0
	ring_mesh.bottom_radius = 1.0
	ring_mesh.height = 0.05
	ring_mesh.radial_segments = 48
	ring.mesh = ring_mesh
	var ring_material := StandardMaterial3D.new()
	ring_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	ring_material.albedo_color = NEUTRAL.darkened(0.3)
	ring_material.emission_enabled = true
	ring_material.emission = NEUTRAL
	ring_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	ring.material_override = ring_material
	root.add_child(ring)
	var beacon := MeshInstance3D.new()
	var beacon_mesh := CylinderMesh.new()
	beacon_mesh.top_radius = 0.6
	beacon_mesh.bottom_radius = 0.9
	beacon_mesh.height = 6.0
	beacon.mesh = beacon_mesh
	beacon.position.y = 3.0
	var beacon_material := StandardMaterial3D.new()
	beacon_material.albedo_color = NEUTRAL
	beacon_material.emission_enabled = true
	beacon_material.emission = NEUTRAL
	beacon.material_override = beacon_material
	root.add_child(beacon)
	var label := Label3D.new()
	label.position = Vector3(0.0, 8.0, 0.0)
	label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	label.font_size = 48
	label.outline_size = 8
	label.modulate = NEUTRAL
	root.add_child(label)
	return {"root": root, "ring": ring, "label": label, "ring_material": ring_material,
		"beacon_material": beacon_material}


func _color_for_state(state: int) -> Color:
	match state:
		1: return FRIENDLY
		2: return ENEMY
		3: return CONTESTED
		_: return NEUTRAL


func _state_name(state: int) -> String:
	match state:
		1: return "FRIENDLY"
		2: return "ENEMY"
		3: return "CONTESTED"
		_: return "NEUTRAL"
