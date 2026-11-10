extends SceneTree

var checks := 0
var failures := 0

func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	var extension: Object = view.extension
	var friend := int(extension.call("create_unit_with_type", -12500.0, 1000.0, 0, 0))
	var enemy := int(extension.call("create_unit_with_type", -12500.0, 1300.0, 3, 1))
	check(friend > 0 and enemy > 0, "native radar fixtures spawn")
	view._sync_new_entities()
	extension.call("update_simulation", 250.0)
	view._sync_unit_transforms()
	view._sync_vision()
	var profile: PackedFloat32Array = extension.call("get_unit_range_profile", friend)
	check(profile == PackedFloat32Array([115,150,450]), "native attack/view/intel radii are separate authored distances")
	check(extension.call("get_unit_range_profile", -1) == PackedFloat32Array([0,0,0]), "unknown IDs have no invented ranges")
	var wrapper: Node3D = view.prototype_visual_views[friend]
	for spec: Dictionary in wrapper._range_overlays:
		var index := 0 if spec.node.name == "AttackHex" else (1 if spec.node.name == "VisibilityHex" else 2)
		check(is_equal_approx(spec.radius, profile[index]), "%s uses native metres" % spec.node.name)
		check(spec.node.visible, "each supported range is visible")
		var points: PackedVector3Array = wrapper._range_vertices(Vector3.ZERO, spec.radius, false)
		check(points.size() > 12 and points.size() % 2 == 0, "flat range is a curved outline of many short pieces")
		var on_range := true
		for point in points:
			on_range = on_range and is_equal_approx(point.length(), profile[index])
		check(on_range, "every outline point lies exactly at the gameplay range")
		check(((wrapper.get_node(String(spec.node.name)) as MeshInstance3D).material_override as ShaderMaterial).get_shader_parameter("width_px") == 3.0, "range outlines are as thick as the fog of war edge")
	var tick_before: float = extension.call("get_tick_clock")[0]
	var native_before: PackedFloat32Array = extension.call("get_unit_position", friend)
	var air_pose := Transform3D(Basis(Vector3.UP, 0.8)*Basis(Vector3.FORWARD, 0.3), Vector3(-12500,view._terrain_height_at(-12500,1000)+220,1000))
	wrapper.set_ranges_airborne(true)
	wrapper.apply_simulation_transform(air_pose)
	for ring_name in ["AttackHex", "VisibilityHex", "RadarHex"]:
		var ring := wrapper.get_node(ring_name) as MeshInstance3D
		check(is_equal_approx(ring.global_position.y, air_pose.origin.y+0.12), "aircraft hex follows flight height")
		check(ring.global_basis.is_equal_approx(Basis.IDENTITY), "bank/yaw do not tilt range plane")
	var first_mesh: Mesh = wrapper.get_node("RadarHex").mesh
	wrapper.apply_simulation_transform(Transform3D(air_pose.basis,air_pose.origin+Vector3(10,5,20)))
	check(wrapper.get_node("RadarHex").mesh == first_mesh, "moving aircraft reuse range geometry")
	var placement: Dictionary = view.strategic_icon_overlay.marker_placement(friend, view.camera)
	check(placement.world.is_equal_approx(wrapper.global_position+Vector3.UP*5), "strategic icon leader anchors at drawn flight pose")
	check(placement.icon_center.y == placement.anchor.y-22, "icon sits above aircraft in screen space")
	check(placement.world.y > view._terrain_height_at(-12500,1000)+100, "strategic icon is never anchored to ground for aircraft")
	check(extension.call("get_unit_position", friend) == native_before and extension.call("get_tick_clock")[0] == tick_before, "overlay updates do not mutate simulation")
	wrapper.set_ranges_airborne(false)
	wrapper.set_range_terrain_height_sampler(func(x,z): return 10.0+x*0.001+z*0.002)
	wrapper.apply_simulation_transform(air_pose)
	var ground_vertices: PackedVector3Array = wrapper._range_vertices(air_pose.origin,450.0,true)
	check(ground_vertices.size() > 12, "wide ground ranges sample terrain along the curve")
	for point in [ground_vertices[0],ground_vertices[ground_vertices.size()/2],ground_vertices[-1]]:
		check(absf(point.y-(10.0+point.x*0.001+point.z*0.002+0.12)) < 0.001, "ground envelope follows terrain, not aircraft height")
	var stationary_mesh: Mesh = wrapper.get_node("RadarHex").mesh
	wrapper.apply_simulation_transform(air_pose)
	check(wrapper.get_node("RadarHex").mesh == stationary_mesh, "stationary ground geometry is reused")
	wrapper.set_range_terrain_height_sampler(func(x,z): return 20.0+x*0.001+z*0.002)
	check(wrapper.get_node("RadarHex").mesh != stationary_mesh, "changing terrain sampler invalidates even a stationary envelope")
	wrapper.set_gameplay_ranges(195,195,600)
	var attack_points: PackedVector3Array = wrapper._range_vertices(Vector3.ZERO,195,false,0)
	var view_points: PackedVector3Array = wrapper._range_vertices(Vector3.ZERO,195,false,1)
	check(attack_points[0] != view_points[0], "equal gameplay ranges use alternating red/blue segments")
	wrapper.set_gameplay_ranges(0,300,900)
	check(not wrapper.get_node("AttackHex").visible and wrapper.get_node("RadarHex").visible, "unarmed aircraft get sight/intel rings without a fake weapon ring")
	var vision: Dictionary = extension.call("vision_state",0)
	var contacts: PackedFloat32Array = vision.get("radar_contacts",PackedFloat32Array())
	check(contacts.size() == 2, "radar exposes one anonymous x/z pair")
	check(not view._is_unit_shown(enemy), "radar contact does not reveal the enemy model")
	check(not view.prototype_visual_views[enemy].get_node("RadarHex").is_visible_in_tree(), "unseen enemy range envelopes stay hidden too")
	for ring_name in ["AttackHex", "VisibilityHex", "RadarHex"]:
		check(not (view.prototype_visual_views[enemy].get_node(ring_name) as Node3D).visible, "enemy %s outline is never drawn for the opponent, even once the unit is seen" % ring_name)
	check(not (vision.get("seen_enemies",PackedInt32Array()) as PackedInt32Array).has(enemy), "radar grants no visual targeting identity")
	check(not contacts.is_empty() and Vector2(contacts[0],contacts[1]).distance_to(Vector2(-12500,1300)) < 72, "radar contact uses a coarse cell centre")
	if not OS.get_environment("RTS_OVERLAY_SCREENSHOT").is_empty():
		wrapper.set_gameplay_ranges(profile[0],profile[1],profile[2])
		wrapper.set_range_terrain_height_sampler(func(x,z): return view._terrain_height_at(x,z))
		view._sync_unit_transforms()
		view._clear_selection()
		view._set_selected(friend,true)
		view.camera_target = wrapper.global_position
		view.camera_distance = 800.0
		view.target_camera_distance = 800.0
		view._update_camera(1.0)
		view._sync_unit_transforms()
		view._update_hud()
		for i in range(3): await process_frame
		RenderingServer.force_draw()
		await RenderingServer.frame_post_draw
		check(view.get_viewport().get_texture().get_image().save_png(OS.get_environment("RTS_OVERLAY_SCREENSHOT")) == OK, "rendered radar/range checkpoint saved")
	view.queue_free()
	await process_frame
	print("UNIT_RANGE_OVERLAYS checks=%d failures=%d" % [checks,failures])
	quit(1 if failures else 0)
