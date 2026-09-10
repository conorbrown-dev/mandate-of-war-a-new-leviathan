class_name UnitVisualRoot
extends Node3D

## Generic presentation wrapper for imported donor GLBs. This node has no
## gameplay processing, physics, or timers; simulation code drives it through
## apply_simulation_transform().

var _registry
var _definition: Dictionary = {}
var _model_root: Node3D
var _turret_root: Node3D
var _hardpoint_markers: Node3D
var _selection_visual: MeshInstance3D
var _debug_visuals: Node3D
var _strategic_visual: MeshInstance3D
var _gun_root: Node3D
var _hardpoints: Dictionary = {}
var _lod_tier := 0
static var _fallback_materials: Dictionary = {}


func _ready() -> void:
	_ensure_nodes()


func configure(registry, visual_id: String, faction_color := Color.WHITE) -> bool:
	_ensure_nodes()
	_registry = registry
	_definition = registry.resolve(visual_id)
	_clear_model()
	var model: Resource = registry.load_model(visual_id)
	if model is PackedScene:
		var instance := (model as PackedScene).instantiate()
		_model_root.add_child(instance)
	else:
		_add_fallback_mesh(faction_color)
	_apply_definition_transform()
	_configure_hardpoints()
	_selection_visual.visible = false
	return not bool(_definition.get("fallback", false)) and model != null


func apply_simulation_transform(world_transform: Transform3D) -> void:
	global_transform = world_transform


func set_selected(selected: bool) -> void:
	_ensure_nodes()
	_selection_visual.visible = selected


func update_lod(camera_distance: float) -> void:
	# Called by the central presentation synchronizer, never from _process().
	var lod: Dictionary = _definition.get("lod", {})
	var far_distance := float(lod.get("strategic_distance", 190.0))
	var mid_distance := float(lod.get("reduced_distance", 95.0))
	var tier := 2 if camera_distance >= far_distance else (1 if camera_distance >= mid_distance else 0)
	if tier == _lod_tier:
		return
	_lod_tier = tier
	_model_root.visible = tier < 2
	_strategic_visual.visible = tier == 2
	_set_imported_lod_visibility(_model_root, tier)


func presentation_footprint() -> Dictionary:
	# Data only: simulation owns collision/pathing. No render-mesh collision or
	# per-unit physics body is created by this wrapper.
	return _definition.get("footprint", {"shape": "circle", "radius": float(_definition.get("selection_radius", 2.0))})


func hardpoint_root() -> Node3D:
	_ensure_nodes()
	return _hardpoint_markers


func hardpoint_transform(hardpoint_id: String) -> Transform3D:
	var marker: Node3D = _hardpoints.get(hardpoint_id, null)
	return marker.global_transform if marker != null else global_transform


func apply_presentation_pose(turret_yaw := 0.0, gun_elevation := 0.0, aircraft_bank := 0.0) -> void:
	# These are visual-only offsets supplied by the central presentation system.
	# They never alter the simulation transform, pathing heading, or firing rule.
	_ensure_nodes()
	_turret_root.rotation.y = turret_yaw
	_gun_root.rotation.x = gun_elevation
	_model_root.rotation.z = deg_to_rad(float(_definition.get("bank_correction_degrees", 0.0))) + aircraft_bank


func _ensure_nodes() -> void:
	if _model_root != null:
		return
	_model_root = Node3D.new()
	_model_root.name = "ModelRoot"
	add_child(_model_root)
	_turret_root = Node3D.new()
	_turret_root.name = "OptionalTurretRoot"
	add_child(_turret_root)
	_gun_root = Node3D.new()
	_gun_root.name = "GunRoot"
	_turret_root.add_child(_gun_root)
	_hardpoint_markers = Node3D.new()
	_hardpoint_markers.name = "OptionalHardpointMarkers"
	add_child(_hardpoint_markers)
	_selection_visual = MeshInstance3D.new()
	_selection_visual.name = "SelectionVisual"
	var ring := CylinderMesh.new()
	ring.top_radius = 1.0
	ring.bottom_radius = 1.0
	ring.height = 0.04
	ring.radial_segments = 12
	_selection_visual.mesh = ring
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.25, 0.85, 1.0, 0.55)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_selection_visual.material_override = material
	_selection_visual.position.y = 0.03
	_selection_visual.visible = false
	add_child(_selection_visual)
	_strategic_visual = MeshInstance3D.new()
	_strategic_visual.name = "StrategicZoomVisual"
	var strategic_mesh := CylinderMesh.new()
	strategic_mesh.top_radius = 0.85
	strategic_mesh.bottom_radius = 0.85
	strategic_mesh.height = 0.12
	strategic_mesh.radial_segments = 6
	_strategic_visual.mesh = strategic_mesh
	_strategic_visual.position.y = 0.12
	_strategic_visual.material_override = _shared_fallback_material(Color("#72c9e8"))
	_strategic_visual.visible = false
	add_child(_strategic_visual)
	_debug_visuals = Node3D.new()
	_debug_visuals.name = "DebugVisuals"
	_debug_visuals.visible = false
	add_child(_debug_visuals)


func _clear_model() -> void:
	for child in _model_root.get_children():
		child.queue_free()


func _configure_hardpoints() -> void:
	for child in _hardpoint_markers.get_children():
		child.queue_free()
	_hardpoints.clear()
	# Donor-node names are never authoritative. Definitions may provide explicit
	# offsets later; these stable fallback markers keep prototype firing/FX hooks
	# usable until Blender cleanup supplies authored anchors.
	var ids: Array = _definition.get("hardpoints", [])
	if "turret" in ids or "turret_root" in ids:
		_register_hardpoint("turret_root", _turret_root, Vector3(0, 0.72, 0))
		_register_hardpoint("gun_root", _gun_root, Vector3.ZERO)
		_register_hardpoint("muzzle", _gun_root, Vector3(0, 0, -1.65))
	if "launcher" in ids:
		_register_hardpoint("weapon_primary", _hardpoint_markers, Vector3(0, 0.9, -1.1))
	for id in ids:
		var hardpoint_id := String(id)
		if hardpoint_id not in _hardpoints:
			_register_hardpoint(hardpoint_id, _hardpoint_markers, Vector3.ZERO)
	var authored_offsets: Dictionary = _definition.get("hardpoint_offsets", {})
	for id in authored_offsets:
		var marker: Node3D = _hardpoints.get(String(id), null)
		var values: Array = authored_offsets[id]
		if marker != null and values.size() == 3:
			marker.position = Vector3(float(values[0]), float(values[1]), float(values[2]))


func _register_hardpoint(hardpoint_id: String, parent: Node3D, offset: Vector3) -> void:
	var marker := Node3D.new()
	marker.name = hardpoint_id
	marker.position = offset
	parent.add_child(marker)
	_hardpoints[hardpoint_id] = marker


func _apply_definition_transform() -> void:
	var scale_values: Array = _definition.get("scale", [1.0, 1.0, 1.0])
	if scale_values.size() == 3:
		_model_root.scale = Vector3(float(scale_values[0]), float(scale_values[1]), float(scale_values[2]))
	var rotation_values: Array = _definition.get("rotation_degrees", [0.0, 0.0, 0.0])
	if rotation_values.size() == 3:
		_model_root.rotation_degrees = Vector3(float(rotation_values[0]), float(rotation_values[1]), float(rotation_values[2]))
	_model_root.position.y = float(_definition.get("ground_offset", 0.0))
	var radius := float(_definition.get("selection_radius", 2.0))
	_selection_visual.scale = Vector3(radius, 1.0, radius)
	_lod_tier = -1
	update_lod(0.0)


func _add_fallback_mesh(faction_color: Color) -> void:
	var mesh_instance := MeshInstance3D.new()
	mesh_instance.name = "DevelopmentFallbackMesh"
	var mesh := BoxMesh.new()
	mesh.size = Vector3(1.6, 0.7, 2.4)
	mesh_instance.mesh = mesh
	mesh_instance.material_override = _shared_fallback_material(faction_color.darkened(0.15))
	mesh_instance.position.y = 0.35
	_model_root.add_child(mesh_instance)


func _shared_fallback_material(color: Color) -> StandardMaterial3D:
	var key := color.to_html()
	if _fallback_materials.has(key):
		return _fallback_materials[key]
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = 0.35
	material.roughness = 0.55
	_fallback_materials[key] = material
	return material


func _set_imported_lod_visibility(node: Node, tier: int) -> void:
	for child in node.get_children():
		if child is VisualInstance3D and "_LOD" in child.name:
			child.visible = child.name.ends_with("LOD%d" % tier)
		_set_imported_lod_visibility(child, tier)
