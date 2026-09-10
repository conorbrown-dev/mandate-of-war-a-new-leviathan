extends SceneTree

const VisualRegistryScript = preload("res://visual_definition_registry.gd")

func _init() -> void:
	var registry = VisualRegistryScript.new()
	assert(registry.load_definitions())
	var mbt := registry.resolve("visual.elite.mbt.prototype")
	assert(String(mbt.visual_id) == "visual.elite.mbt.prototype")
	assert(String(mbt.model_path).begins_with("res://assets/source_3d/cc0/"))
	var fallback := registry.resolve("visual.does.not.exist")
	assert(bool(fallback.fallback))
	var found_unknown_error := false
	for error in registry.errors:
		found_unknown_error = found_unknown_error or String(error).contains("Unknown visual_id")
	assert(found_unknown_error)
	print("VISUAL_REGISTRY checks=4 failures=0")
	quit()
