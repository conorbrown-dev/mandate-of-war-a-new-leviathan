extends SceneTree

const VisualRegistryScript = preload("res://visual_definition_registry.gd")

func _init() -> void:
	var registry = VisualRegistryScript.new()
	assert(registry.load_definitions())
	var mbt := registry.resolve("visual.elite.mbt.prototype")
	assert(String(mbt.visual_id) == "visual.elite.mbt.prototype")
	# Donor replacement may change the directory without changing visual identity.
	assert(ResourceLoader.exists(String(mbt.model_path)))
	var loaded := registry.load_model(String(mbt.visual_id))
	assert(loaded is PackedScene)
	assert(registry.load_model(String(mbt.visual_id)) == loaded)
	var fallback := registry.resolve("visual.does.not.exist")
	assert(bool(fallback.fallback))
	var found_unknown_error := false
	for error in registry.errors:
		found_unknown_error = found_unknown_error or String(error).contains("Unknown visual_id")
	assert(found_unknown_error)
	var fixture := "user://registry-test.json"
	var file := FileAccess.open(fixture, FileAccess.WRITE)
	assert(file != null)
	file.store_string(JSON.stringify({"visual_definitions": [mbt, mbt]}))
	file.close()
	assert(not registry.load_definitions(fixture))
	assert(String(registry.errors[0]).contains("Duplicate visual_id"))
	file = FileAccess.open(fixture, FileAccess.WRITE)
	file.store_string(JSON.stringify({"visual_definitions": [{"visual_id": "missing.model", "model_path": "res://absent-validation-model.glb"}]}))
	file.close()
	assert(registry.load_definitions(fixture))
	assert(registry.load_model("missing.model") == null)
	assert(String(registry.errors[0]).contains("Visual model is missing"))
	print("VISUAL_REGISTRY checks=13 failures=0")
	quit()
