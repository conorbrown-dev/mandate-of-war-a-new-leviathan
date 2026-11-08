class_name UnitVisualRoot
extends Node3D

const StrategicUnitIconScript = preload("res://strategic_unit_icon.gd")

## Generic presentation wrapper for imported donor GLBs. This node has no
## gameplay processing, physics, or timers; simulation code drives it through
## apply_simulation_transform().

var _registry
var _definition: Dictionary = {}
var _model_root: Node3D
var _turret_root: Node3D
var _hardpoint_markers: Node3D
var _debug_visuals: Node3D
var _strategic_visual: MeshInstance3D
var _strategic_stem: MeshInstance3D
var _strategic_icon_scale := Vector3.ONE
var _gun_root: Node3D
var _hardpoints: Dictionary = {}
var _lod_tier := 0
var _terrain_height_sampler: Callable
var _range_overlays: Array[Dictionary] = []
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
		if _definition.has("isolate"):
			_isolate_vehicle(instance as Node3D, _definition.isolate, visual_id)
		_make_materials_opaque(instance, _definition.get("opaque_materials", []))
	else:
		_add_fallback_mesh(faction_color)
	_apply_definition_transform()
	_configure_hardpoints()
	return not bool(_definition.get("fallback", false)) and model != null


# Some donor files are packs that lay out many vehicles side by side, with
# several vehicles' parts merged into one mesh. "isolate" keeps only the
# surfaces centred inside a rectangle of the model's own ground plane:
#   {"center": [x, z], "half_extent": [x, z], "length_m": metres}
# and then centres that one vehicle and scales it to its real length. The cut
# meshes are cached per visual so every unit of the type shares them.
static var _isolated_meshes := {}


func _isolate_vehicle(instance: Node3D, spec: Dictionary, visual_id: String) -> void:
	var center := Vector2(float(spec.center[0]), float(spec.center[1]))
	var half := Vector2(float(spec.half_extent[0]), float(spec.half_extent[1]))
	var area := Rect2(center - half, half * 2.0)
	var kept_bounds = null
	for node in instance.find_children("*", "MeshInstance3D", true, false):
		var mesh_instance := node as MeshInstance3D
		if mesh_instance.mesh == null:
			continue
		var to_instance := _transform_between(mesh_instance, instance)
		var key := "%s|%s" % [visual_id, instance.get_path_to(mesh_instance)]
		if not _isolated_meshes.has(key):
			_isolated_meshes[key] = _cut_mesh(mesh_instance.mesh, to_instance, area)
		var cut: Array = _isolated_meshes[key]
		mesh_instance.mesh = cut[0]
		if cut[1] != null:
			var bounds: AABB = cut[1]
			kept_bounds = bounds if kept_bounds == null else (kept_bounds as AABB).merge(bounds)
	if kept_bounds == null:
		push_warning("%s: isolate rectangle %s contains no geometry" % [visual_id, area])
		return
	var box: AABB = kept_bounds
	var length := maxf(box.size.x, box.size.z)
	var scale := float(spec.get("length_m", length)) / maxf(length, 0.0001)
	var middle := box.get_center()
	instance.transform = Transform3D(Basis.from_scale(Vector3.ONE * scale), Vector3(-middle.x, 0.0, -middle.z) * scale) * instance.transform


# The transform from a descendant's space into an ancestor's.
func _transform_between(node: Node3D, ancestor: Node3D) -> Transform3D:
	var transform := Transform3D.IDENTITY
	var current: Node = node
	while current != null and current != ancestor:
		if current is Node3D:
			transform = (current as Node3D).transform * transform
		current = current.get_parent()
	return transform


# [mesh holding only the surfaces centred inside area, or null, and the kept
# surfaces' bounds in instance space, or null].
func _cut_mesh(mesh: Mesh, to_instance: Transform3D, area: Rect2) -> Array:
	var kept := ArrayMesh.new()
	var bounds = null
	var all_kept := true
	for surface in range(mesh.get_surface_count()):
		var arrays := mesh.surface_get_arrays(surface)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		if vertices.is_empty():
			continue
		var box := AABB(to_instance * vertices[0], Vector3.ZERO)
		for vertex in vertices:
			box = box.expand(to_instance * vertex)
		var middle := box.get_center()
		if not area.has_point(Vector2(middle.x, middle.z)):
			all_kept = false
			continue
		bounds = box if bounds == null else (bounds as AABB).merge(box)
		kept.add_surface_from_arrays(mesh.surface_get_primitive_type(surface), arrays)
		kept.surface_set_material(kept.get_surface_count() - 1, mesh.surface_get_material(surface))
	if all_kept:
		return [mesh, bounds]
	return [kept if kept.get_surface_count() > 0 else null, bounds]


# Some donor models mark solid surfaces as alpha-blended, which draws an
# airframe see-through. The definition names those materials; they are drawn
# opaque. Shared per material so every unit of the type reuses one copy.
static var _opaque_copies := {}


func _make_materials_opaque(node: Node, names: Array) -> void:
	if names.is_empty():
		return
	if node is MeshInstance3D:
		var mesh_instance := node as MeshInstance3D
		for surface in range(mesh_instance.mesh.get_surface_count() if mesh_instance.mesh != null else 0):
			var material := mesh_instance.mesh.surface_get_material(surface) as BaseMaterial3D
			if material == null or not names.has(material.resource_name):
				continue
			if not _opaque_copies.has(material):
				var opaque := material.duplicate() as BaseMaterial3D
				opaque.transparency = BaseMaterial3D.TRANSPARENCY_DISABLED
				opaque.albedo_color.a = 1.0
				_opaque_copies[material] = opaque
			mesh_instance.set_surface_override_material(surface, _opaque_copies[material])
	for child in node.get_children():
		_make_materials_opaque(child, names)


func apply_simulation_transform(world_transform: Transform3D) -> void:
	global_transform = world_transform
	_update_range_overlays(world_transform.origin)


func set_selected(selected: bool) -> void:
	# Selection is already communicated by the persistent range envelopes.
	# Do not add a redundant yellow ring beneath the unit.
	pass


func set_range_terrain_height_sampler(sampler: Callable) -> void:
	_terrain_height_sampler = sampler
	_update_range_overlays(global_position)


func update_lod(camera_distance: float) -> void:
	# Called by the central presentation synchronizer, never from _process().
	var lod: Dictionary = _definition.get("lod", {})
	# Keep detailed models readable through the first zoom-out band. The
	# strategic marker is reserved for map-scale navigation, where it acts as a
	# location beacon rather than a replacement vehicle mesh.
	# Keep the 3D model visible until the screen-space strategic overlay turns
	# off at 600 m. The old 500 m floor created a 500-600 m gap where the
	# strategic marker was still visible but the unit mesh was already hidden.
	var far_distance := maxf(float(lod.get("strategic_distance", 190.0)), 600.0)
	var mid_distance := float(lod.get("reduced_distance", 95.0))
	var tier := 2 if camera_distance >= far_distance else (1 if camera_distance >= mid_distance else 0)
	if tier == _lod_tier:
		return
	_lod_tier = tier
	_model_root.visible = tier < 2
	# Map-scale symbols are now rendered by StrategicIconOverlay as fixed-pixel
	# screen-space markers, like Forged Alliance's strategic view. Do not leave
	# a scaled 3D glyph on the hull plane where it reads as a gray dot.
	_strategic_visual.visible = false
	_strategic_stem.visible = false
	_set_imported_lod_visibility(_model_root, tier)


func set_strategic_icon(unit_type: int, faction_color: Color) -> void:
	_ensure_nodes()
	_strategic_visual.mesh = StrategicUnitIconScript.mesh_for(unit_type)
	_strategic_icon_scale = StrategicUnitIconScript.shape_scale_for(unit_type)
	_strategic_visual.scale = _strategic_icon_scale
	_strategic_visual.rotation.y = StrategicUnitIconScript.rotation_for(unit_type)
	_strategic_visual.material_override = StrategicUnitIconScript.material_for(faction_color)


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
	_add_hex_range_overlay("VisibilityHex", 15.0, Color("#2d8cff"))
	_add_hex_range_overlay("RadarHex", 22.0, Color("#a04dff"))
	_add_hex_range_overlay("AttackHex", 10.0, Color("#e33b3b"))
	_strategic_visual = MeshInstance3D.new()
	_strategic_visual.name = "StrategicZoomVisual"
	_strategic_visual.mesh = StrategicUnitIconScript.mesh_for(-1)
	_strategic_visual.position.y = 5.0
	_strategic_visual.material_override = StrategicUnitIconScript.material_for(Color("#72c9e8"))
	_strategic_visual.visible = false
	add_child(_strategic_visual)
	_strategic_stem = MeshInstance3D.new()
	_strategic_stem.name = "StrategicMarkerStem"
	var stem_mesh := CylinderMesh.new()
	stem_mesh.top_radius = 0.10
	stem_mesh.bottom_radius = 0.10
	stem_mesh.height = 5.0
	stem_mesh.radial_segments = 6
	_strategic_stem.mesh = stem_mesh
	_strategic_stem.position.y = 2.5
	var stem_material := StrategicUnitIconScript.material_for(Color("#72c9e8"))
	stem_material.albedo_color = stem_material.albedo_color.darkened(0.35)
	_strategic_stem.material_override = stem_material
	_strategic_stem.visible = false
	add_child(_strategic_stem)
	_debug_visuals = Node3D.new()
	_debug_visuals.name = "DebugVisuals"
	_debug_visuals.visible = false
	add_child(_debug_visuals)


func _add_hex_range_overlay(ring_name: String, radius: float, color: Color) -> void:
	var overlay := MeshInstance3D.new()
	overlay.name = ring_name
	# Top-level world geometry avoids inheriting vehicle rotation or model lift.
	overlay.top_level = true
	var material := _shared_fallback_material(color)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color.a = 0.88
	overlay.material_override = material
	overlay.visible = true
	add_child(overlay)
	_range_overlays.append({"node": overlay, "radius": radius, "material": material})


func _update_range_overlays(center: Vector3) -> void:
	for spec in _range_overlays:
		var overlay := spec.node as MeshInstance3D
		if overlay == null:
			continue
		var radius := float(spec.radius)
		var lines := ImmediateMesh.new()
		lines.surface_begin(Mesh.PRIMITIVE_LINES)
		for side in range(6):
			var a := TAU * float(side) / 6.0 + PI / 6.0
			var b := TAU * float(side + 1) / 6.0 + PI / 6.0
			var start := Vector3(center.x + cos(a) * radius, center.y, center.z + sin(a) * radius)
			var finish := Vector3(center.x + cos(b) * radius, center.y, center.z + sin(b) * radius)
			if _terrain_height_sampler.is_valid():
				start.y = float(_terrain_height_sampler.call(start.x, start.z)) + 0.12
				finish.y = float(_terrain_height_sampler.call(finish.x, finish.z)) + 0.12
			else:
				start.y += 0.12
				finish.y += 0.12
			lines.surface_add_vertex(start)
			lines.surface_add_vertex(finish)
		lines.surface_end()
		lines.surface_set_material(0, spec.material)
		overlay.mesh = lines


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
	_model_root.position = Vector3.ZERO
	var scale_values: Array = _definition.get("scale", [1.0, 1.0, 1.0])
	if scale_values.size() == 3:
		_model_root.scale = Vector3(float(scale_values[0]), float(scale_values[1]), float(scale_values[2]))
	var rotation_values: Array = _definition.get("rotation_degrees", [0.0, 0.0, 0.0])
	if rotation_values.size() == 3:
		_model_root.rotation_degrees = Vector3(float(rotation_values[0]), float(rotation_values[1]), float(rotation_values[2]))
	var ground_offset := float(_definition.get("ground_offset", 0.0))
	if String(_definition.get("grounding", "origin")) == "mesh_bottom":
		var minimum_y := _minimum_model_y(_model_root)
		if is_finite(minimum_y):
			_model_root.position.y = ground_offset - minimum_y
	else:
		_model_root.position.y = ground_offset
	_lod_tier = -1
	update_lod(0.0)


func _minimum_model_y(node: Node) -> float:
	var minimum_y := INF
	if node is MeshInstance3D:
		var mesh_instance := node as MeshInstance3D
		if mesh_instance.mesh != null:
			var relative_transform := global_transform.affine_inverse() * mesh_instance.global_transform
			var bounds := mesh_instance.mesh.get_aabb()
			for endpoint in range(8):
				minimum_y = minf(minimum_y, (relative_transform * bounds.get_endpoint(endpoint)).y)
	for child in node.get_children():
		minimum_y = minf(minimum_y, _minimum_model_y(child))
	return minimum_y


func model_bottom_height() -> float:
	return _minimum_model_y(_model_root)


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
