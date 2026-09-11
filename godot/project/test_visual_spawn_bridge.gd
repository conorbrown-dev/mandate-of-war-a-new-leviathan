extends SceneTree

const RegistryScript = preload("res://visual_definition_registry.gd")
const SpawnBridgeScript = preload("res://visual_spawn_bridge.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	var registry = RegistryScript.new()
	assert(registry.load_definitions())
	var holder := Node3D.new()
	root.add_child(holder)
	var wrapper = SpawnBridgeScript.new().spawn(holder, registry, "visual.elite.mbt.prototype", Transform3D(Basis.IDENTITY, Vector3(6, 0, -3)), Color.BLUE)
	assert(wrapper.global_position == Vector3(6, 0, -3))
	assert(wrapper.get_node("ModelRoot").get_child_count() > 0)
	assert(String(registry.resolve("visual.elite.mbt.prototype").visual_id) == "visual.elite.mbt.prototype")
	print("VISUAL_SPAWN_BRIDGE checks=3 failures=0")
	quit()
