class_name VisualAssetValidator
extends RefCounted

const RegistryScript = preload("res://visual_definition_registry.gd")
const PROTOTYPE_TRIANGLE_BUDGET := 20000
const EXTREME_BOUND_METERS := 400.0

func validate(definitions_path := "res://visuals/visual_definitions.json") -> Dictionary:
	var registry = RegistryScript.new()
	var registry_ok := registry.load_definitions(definitions_path)
	var report := {"schema_version": 1, "definitions_path": definitions_path, "valid": registry_ok, "entries": [], "errors": [], "warnings": []}
	report.errors.append_array(registry.errors)
	var file := FileAccess.open(definitions_path, FileAccess.READ)
	if file == null:
		return report
	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if not parsed is Dictionary:
		report.errors.append("Definitions JSON could not be parsed")
		return report
	var seen := {}
	for definition_value in parsed.get("visual_definitions", []):
		var definition: Dictionary = definition_value
		var visual_id := String(definition.get("visual_id", ""))
		var path := String(definition.get("model_path", ""))
		var entry := {
			"visual_id": visual_id,
			"category": _category(definition),
			"status": String(definition.get("status", "prototype_donor")),
			"model_path": path,
			"metadata_path": path.get_basename() + ".meta.json",
			"bounds_m": {},
			"effective_bounds_m": {},
			"triangle_count": -1,
			"imported_mesh_count": 0,
			"material_reference_count": 0,
			"texture_reference_count": 0,
			"issues": []
		}
		if seen.has(visual_id):
			entry.issues.append("duplicate_visual_id")
		seen[visual_id] = true
		var scale: Array = definition.get("scale", [])
		if scale.size() != 3 or scale.any(func(value): return not is_finite(float(value)) or absf(float(value)) < 0.001 or absf(float(value)) > 100.0):
			entry.issues.append("invalid_or_extreme_scale")
		if not ResourceLoader.exists(path):
			entry.issues.append("missing_glb")
		else:
			_validate_metadata(entry, definition)
			_validate_imported_materials(entry, path)
		if not definition.get("lod", null) is Dictionary:
			entry.issues.append("missing_lod_configuration")
		elif String(definition.lod.get("mode", "strategic_marker_only")) not in ["strategic_marker_only", "imported_nodes"]:
			entry.issues.append("invalid_lod_mode")
		elif String(definition.lod.get("mode", "strategic_marker_only")) == "imported_nodes":
			_validate_imported_lod_scene(entry, path)
		if not definition.get("hardpoints", null) is Array:
			entry.issues.append("invalid_hardpoints")
		else:
			_validate_hardpoints(entry, definition)
		if String(definition.get("status", "prototype_donor")) == "shipping" and entry.issues.size() > 0:
			report.errors.append("%s has shipping-blocking validation issues: %s" % [visual_id, ", ".join(entry.issues)])
		if not entry.issues.is_empty():
			report.warnings.append("%s: %s" % [visual_id, ", ".join(entry.issues)])
		report.entries.append(entry)
	report.valid = report.errors.is_empty()
	return report


func _validate_metadata(entry: Dictionary, definition: Dictionary) -> void:
	var metadata_path := String(entry.metadata_path)
	var file := FileAccess.open(metadata_path, FileAccess.READ)
	if file == null:
		entry.issues.append("missing_pipeline_metadata")
		return
	var metadata: Variant = JSON.parse_string(file.get_as_text())
	if not metadata is Dictionary:
		entry.issues.append("invalid_pipeline_metadata")
		return
	var bounds: Dictionary = metadata.get("bounds_m", {})
	entry.bounds_m = bounds
	var dimensions := [float(bounds.get("x", 0.0)), float(bounds.get("y", 0.0)), float(bounds.get("z", 0.0))]
	if dimensions.any(func(value): return not is_finite(value) or value <= 0.0):
		entry.issues.append("invalid_bounds")
	var scale: Array = definition.get("scale", [1.0, 1.0, 1.0])
	if scale.size() != 3:
		entry.issues.append("invalid_or_extreme_scale")
		scale = [1.0, 1.0, 1.0]
	var effective_dimensions := [dimensions[0] * absf(float(scale[0])), dimensions[1] * absf(float(scale[1])), dimensions[2] * absf(float(scale[2]))]
	entry.effective_bounds_m = {"x": effective_dimensions[0], "y": effective_dimensions[1], "z": effective_dimensions[2]}
	if dimensions.max() > EXTREME_BOUND_METERS:
		entry.issues.append("extreme_bounds_prototype")
	if effective_dimensions.max() > EXTREME_BOUND_METERS:
		entry.issues.append("extreme_effective_bounds")
	var triangles := int(metadata.get("triangles_all_exported_meshes", -1))
	entry.triangle_count = triangles
	if triangles < 0:
		entry.issues.append("missing_triangle_metadata")
	elif triangles > int(definition.get("triangle_budget", PROTOTYPE_TRIANGLE_BUDGET)):
		entry.issues.append("triangle_budget_exceeded")


func _validate_hardpoints(entry: Dictionary, definition: Dictionary) -> void:
	var hardpoints: Array = definition.get("hardpoints", [])
	var weapon_bearing := "turret" in hardpoints or "launcher" in hardpoints
	var authored_offsets: Dictionary = definition.get("hardpoint_offsets", {})
	if weapon_bearing and not authored_offsets.has("muzzle"):
		entry.issues.append("fallback_muzzle_marker")
	if String(definition.get("status", "prototype_donor")) == "shipping" and weapon_bearing and not authored_offsets.has("muzzle"):
		entry.issues.append("shipping_muzzle_offset_required")


func _validate_imported_lod_scene(entry: Dictionary, path: String) -> void:
	var resource := load(path)
	if not resource is PackedScene:
		entry.issues.append("imported_lod_scene_unavailable")
		return
	var scene_state := (resource as PackedScene).get_state()
	var lod_named_nodes := 0
	for index in range(scene_state.get_node_count()):
		if "_LOD" in String(scene_state.get_node_name(index)):
			lod_named_nodes += 1
	if lod_named_nodes < 2:
		entry.issues.append("imported_lod_nodes_missing")
	var instance := (resource as PackedScene).instantiate()
	var visible_lod_nodes := _count_visible_lod_nodes(instance)
	instance.free()
	if visible_lod_nodes > 1:
		entry.issues.append("multiple_lods_visible")
	elif visible_lod_nodes == 0:
		entry.issues.append("no_lod_visible")


func _count_visible_lod_nodes(node: Node) -> int:
	var count := 0
	if node is VisualInstance3D and "_LOD" in node.name and (node as VisualInstance3D).visible:
		count = 1
	for child in node.get_children():
		count += _count_visible_lod_nodes(child)
	return count


func _validate_imported_materials(entry: Dictionary, path: String) -> void:
	var resource := load(path)
	if not resource is PackedScene:
		entry.issues.append("imported_scene_unavailable")
		return
	var instance := (resource as PackedScene).instantiate()
	_scan_materials(instance, entry)
	instance.free()
	if int(entry.imported_mesh_count) == 0:
		entry.issues.append("imported_mesh_missing")
	if int(entry.material_reference_count) == 0:
		entry.issues.append("missing_material_reference")


func _scan_materials(node: Node, entry: Dictionary) -> void:
	if node is MeshInstance3D:
		var mesh := (node as MeshInstance3D).mesh
		if mesh != null:
			entry.imported_mesh_count += 1
			for surface in range(mesh.get_surface_count()):
				var material: Material = (node as MeshInstance3D).get_surface_override_material(surface)
				if material == null:
					material = mesh.surface_get_material(surface)
				if material != null:
					entry.material_reference_count += 1
					if material is BaseMaterial3D and (material as BaseMaterial3D).albedo_texture != null:
						entry.texture_reference_count += 1
	for child in node.get_children():
		_scan_materials(child, entry)


func to_markdown(report: Dictionary) -> String:
	var lines := ["# Visual Asset Validation", "", "Definitions: %d" % report.entries.size(), "", "| Visual ID | Category | Meshes | Materials | Textures | Triangles | Issues |", "|---|---:|---:|---:|---:|---:|---|"]
	for entry in report.entries:
		lines.append("| %s | %s | %d | %d | %d | %d | %s |" % [entry.visual_id, entry.category, entry.imported_mesh_count, entry.material_reference_count, entry.texture_reference_count, entry.triangle_count, ", ".join(entry.issues) if not entry.issues.is_empty() else "none"])
	return "\n".join(lines) + "\n"


func write_reports(report: Dictionary, json_path: String, markdown_path: String) -> bool:
	var json_file := FileAccess.open(json_path, FileAccess.WRITE)
	var markdown_file := FileAccess.open(markdown_path, FileAccess.WRITE)
	if json_file == null or markdown_file == null:
		return false
	json_file.store_string(JSON.stringify(report, "  ") + "\n")
	markdown_file.store_string(to_markdown(report))
	return true


func _category(definition: Dictionary) -> String:
	var path := String(definition.get("model_path", "")).to_lower()
	if "/naval/" in path: return "naval"
	if "/air/" in path: return "air"
	if "freight" in path or "logistics" in path: return "logistics"
	if "/ground/" in path: return "ground"
	return "unknown"
