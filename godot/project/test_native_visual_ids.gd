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
	# The patrol boat (11) has its own model now, and so do the units added after it.
	for unit_type in [11, 21, 22, 23, 24, 25]:
		var boat_visual := String(extension.call("get_unit_visual_id", unit_type))
		assert(not boat_visual.is_empty())
		assert(not bool(registry.resolve(boat_visual).get("fallback", false)))
	assert(String(extension.call("get_unit_visual_id", 200)).is_empty())
	print("NATIVE_VISUAL_IDS checks=27 failures=0")
	quit()
