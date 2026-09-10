extends SceneTree

const RegistryScript = preload("res://visual_definition_registry.gd")
const VisualRootScript = preload("res://unit_visual_root.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	var registry = RegistryScript.new()
	assert(registry.load_definitions())
	var wrapper = VisualRootScript.new()
	root.add_child(wrapper)
	assert(not wrapper.configure(registry, "visual.elite.mbt.prototype", Color.BLUE))
	assert(wrapper.get_node("ModelRoot/DevelopmentFallbackMesh") != null)
	wrapper.apply_simulation_transform(Transform3D(Basis.IDENTITY, Vector3(17, 2, -8)))
	assert(wrapper.global_position == Vector3(17, 2, -8))
	wrapper.set_selected(true)
	assert(wrapper.get_node("SelectionVisual").visible)
	assert(wrapper.hardpoint_root().name == "OptionalHardpointMarkers")
	assert(wrapper.get_node("OptionalTurretRoot/GunRoot/muzzle") != null)
	var before_muzzle := wrapper.hardpoint_transform("muzzle").origin
	wrapper.apply_presentation_pose(PI * 0.5, 0.25, 0.1)
	assert(is_equal_approx(wrapper.get_node("OptionalTurretRoot").rotation.y, PI * 0.5))
	assert(not wrapper.hardpoint_transform("muzzle").origin.is_equal_approx(before_muzzle))
	assert(wrapper.global_position == Vector3(17, 2, -8))
	assert(String(wrapper.presentation_footprint().shape) == "circle")
	wrapper.update_lod(250.0)
	assert(not wrapper.get_node("ModelRoot").visible)
	assert(wrapper.get_node("StrategicZoomVisual").visible)
	print("UNIT_VISUAL_ROOT checks=14 failures=0")
	quit()
