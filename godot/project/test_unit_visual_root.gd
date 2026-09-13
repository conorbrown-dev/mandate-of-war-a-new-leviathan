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
	assert(wrapper.configure(registry, "visual.elite.mbt.prototype", Color.BLUE))
	assert(wrapper.get_node("ModelRoot").get_child_count() > 0)
	wrapper.apply_simulation_transform(Transform3D(Basis.IDENTITY, Vector3(17, 2, -8)))
	assert(wrapper.global_position == Vector3(17, 2, -8))
	wrapper.set_selected(true)
	assert(wrapper.get_node("VisibilityHex").visible and wrapper.get_node("AttackHex").visible)
	assert(wrapper.hardpoint_root().name == "OptionalHardpointMarkers")
	assert(wrapper.get_node("OptionalTurretRoot/GunRoot/muzzle") != null)
	var before_muzzle := wrapper.hardpoint_transform("muzzle").origin
	wrapper.apply_presentation_pose(PI * 0.5, 0.25, 0.1)
	assert(is_equal_approx(wrapper.get_node("OptionalTurretRoot").rotation.y, PI * 0.5))
	assert(not wrapper.hardpoint_transform("muzzle").origin.is_equal_approx(before_muzzle))
	assert(wrapper.global_position == Vector3(17, 2, -8))
	assert(String(wrapper.presentation_footprint().shape) == "circle")
	wrapper.update_lod(250.0)
	# 250 m is the reduced-detail band; the imported model remains visible until
	# the strategic overlay's 600 m cutoff.
	assert(wrapper.get_node("ModelRoot").visible)
	# Strategic markers are rendered by the external screen-space overlay; the
	# per-unit 3D strategic visual remains hidden at strategic zoom.
	assert(not wrapper.get_node("StrategicZoomVisual").visible)
	wrapper.update_lod(600.0)
	assert(not wrapper.get_node("ModelRoot").visible)
	wrapper.update_lod(10.0)
	assert(wrapper.get_node("ModelRoot").visible)
	var fallback = VisualRootScript.new()
	root.add_child(fallback)
	assert(not fallback.configure(registry, "missing.visual", Color.BLUE))
	assert(fallback.get_node("ModelRoot").get_child(0).name == "DevelopmentFallbackMesh")
	print("UNIT_VISUAL_ROOT checks=18 failures=0")
	quit()
