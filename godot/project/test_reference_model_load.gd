extends SceneTree

const RegistryScript = preload("res://visual_definition_registry.gd")

func _init() -> void:
	var registry = RegistryScript.new()
	assert(registry.load_definitions())
	for visual_id in registry._definitions.keys():
		print("REFERENCE_LOAD_START %s" % visual_id)
		var resource: Resource = registry.load_model(String(visual_id))
		assert(resource != null)
		print("REFERENCE_LOAD_OK %s" % visual_id)
	print("REFERENCE_MODEL_LOAD checks=%d failures=0" % registry._definitions.size())
	quit()
