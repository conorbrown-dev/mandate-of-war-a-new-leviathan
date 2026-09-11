extends SceneTree

const ValidatorScript = preload("res://visual_asset_validator.gd")

func _init() -> void:
	var report = ValidatorScript.new().validate()
	var missing_visual_ids: Array[String] = []
	for entry in report.entries:
		if "missing_glb" in entry.issues:
			missing_visual_ids.append(String(entry.visual_id))
	print("VISUAL_ASSET_VALIDATOR missing_glb=%s" % ", ".join(missing_visual_ids))
	assert(bool(report.valid))
	assert(report.entries.size() == 15)
	assert(missing_visual_ids.is_empty())
	assert(report.entries.all(func(entry): return int(entry.triangle_count) >= 0 and not entry.bounds_m.is_empty()))
	assert(report.entries.all(func(entry): return not entry.effective_bounds_m.is_empty()))
	assert(report.entries.all(func(entry): return not ("extreme_effective_bounds" in entry.issues)))
	assert(report.entries.all(func(entry): return int(entry.imported_mesh_count) > 0))
	assert(report.entries.all(func(entry): return int(entry.material_reference_count) > 0))
	assert(report.entries.all(func(entry): return not ("triangle_budget_exceeded" in entry.issues)))
	assert(report.entries.all(func(entry): return not ("extreme_bounds_prototype" in entry.issues)))
	assert(ValidatorScript.new().to_markdown(report).contains("Visual Asset Validation"))
	assert(ValidatorScript.new().write_reports(report, "/tmp/visual_asset_validation.json", "/tmp/visual_asset_validation.md"))
	var shipping_report = ValidatorScript.new().validate("res://visuals/test_shipping_visual_definitions.json")
	assert(not bool(shipping_report.valid))
	assert(shipping_report.errors.size() == 1)
	assert("shipping_muzzle_offset_required" in shipping_report.entries[0].issues)
	var lod_report = ValidatorScript.new().validate("res://visuals/test_shipping_lod_definitions.json")
	assert(not bool(lod_report.valid))
	assert(lod_report.errors.size() == 1)
	assert("multiple_lods_visible" in lod_report.entries[0].issues or "imported_lod_nodes_missing" in lod_report.entries[0].issues)
	print("VISUAL_ASSET_VALIDATOR checks=16 failures=0 entries=%d missing_glb=0" % report.entries.size())
	quit()
