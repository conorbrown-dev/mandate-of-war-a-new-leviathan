extends SceneTree

const RegistryScript = preload("res://visual_definition_registry.gd")
const PRODUCTION_UNIT_TYPE_COUNT := 11

func _init() -> void:
	OS.set_environment("RTS_DATA_ROOT", ProjectSettings.globalize_path("res://../../data").simplify_path())
	assert(ClassDB.class_exists("RtsExtension"))
	var extension: Object = ClassDB.instantiate("RtsExtension")
	assert(extension != null)
	var registry = RegistryScript.new()
	assert(registry.load_definitions())

	for unit_type in range(PRODUCTION_UNIT_TYPE_COUNT):
		var visual_id := String(extension.call("get_unit_visual_id", unit_type))
		assert(not visual_id.is_empty())
		var definition: Dictionary = registry.resolve(visual_id)
		assert(not bool(definition.get("fallback", false)))

	assert(String(extension.call("get_unit_visual_id", -1)).is_empty())
	assert(String(extension.call("get_unit_visual_id", PRODUCTION_UNIT_TYPE_COUNT)).is_empty())
	print("NATIVE_VISUAL_IDS checks=27 failures=0")
	quit()
