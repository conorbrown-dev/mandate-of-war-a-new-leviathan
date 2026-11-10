extends SceneTree
## Smooth camera panning and units in the magnifier, through the real game.
## - The forest builds in frame-sized slices, not one 100-240 ms chunk a frame.
## - Camera easing moves at an even pace when frame times vary.
## - Unit models stay drawable for the magnifier while the main view, zoomed
##   out, leaves them to its screen-space icons.

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _start_match() -> Node:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	return view


func _test_forest_builds_in_slices(view: Node) -> void:
	var vegetation = view.terrain_vegetation
	check(vegetation != null and bool(vegetation.building), "the forest is building when the match starts")
	# The enemy's forces landing (new unit models loading for the first time)
	# make slow frames of their own once the match is a minute old. This check is
	# about the forest, so keep the enemy out of the window it measures.
	view.extension.call("skirmish_set_ai_enabled", false)
	var longest_ms := 0.0
	var frames := 0
	var long_frames := 0
	var long_at := PackedStringArray()
	var last := Time.get_ticks_usec()
	var started := Time.get_ticks_msec()
	while vegetation != null and bool(vegetation.building) and Time.get_ticks_msec() - started < 180000:
		await process_frame
		var now := Time.get_ticks_usec()
		var gap := float(now - last) / 1000.0
		longest_ms = maxf(longest_ms, gap)
		if gap > 50.0:
			long_frames += 1
			long_at.append("#%d:%.0fms" % [frames, gap])
		last = now
		frames += 1
	print("PACING forest_build frames=%d longest_frame_ms=%.1f frames_over_50ms=%d at=%s instances=%d" % [frames, longest_ms, long_frames, ",".join(long_at), int(vegetation.instance_count)])
	check(not bool(vegetation.building), "the forest finishes building")
	check(int(vegetation.instance_count) == 953020, "into the same forest as before (%d instances)" % int(vegetation.instance_count))
	# Building a whole chunk in one frame made every one of its 196 chunks a
	# 100-240 ms frame. A stray slow frame from the machine is not that.
	check(long_frames <= 3, "no frame waits on a whole chunk (%d frames over 50 ms)" % long_frames)


func _test_camera_paces_evenly(view: Node) -> void:
	view.camera_distance = 420.0
	view.target_camera_distance = 420.0
	view.magnifier_view.set_enabled(false)
	Input.action_press("ui_right")
	var speeds: Array = []
	var last: Vector3 = view.camera.global_position
	for frame in range(160):
		# A long frame every fourth frame, as when the renderer hitches.
		var delta := 0.2 if frame % 4 == 3 else 1.0 / 60.0
		view._update_keyboard_pan(delta)
		view._update_camera(delta)
		var now: Vector3 = view.camera.global_position
		if frame >= 60:
			speeds.append(Vector2(now.x - last.x, now.z - last.z).length() / delta)
		last = now
	Input.action_release("ui_right")
	speeds.sort()
	var ratio: float = speeds[speeds.size() - 1] / maxf(speeds[0], 0.001)
	print("PACING camera_speed min=%.1f max=%.1f ratio=%.2f" % [speeds[0], speeds[speeds.size() - 1], ratio])
	check(ratio < 1.6, "the camera keeps an even pace through uneven frames (fastest/slowest %.2f)" % ratio)


func _model_layers_ok(node: Node) -> bool:
	for child in node.get_children():
		if child is VisualInstance3D and (child as VisualInstance3D).layers != UnitVisualRoot.MODEL_LAYER:
			return false
		if not _model_layers_ok(child):
			return false
	return true


func _test_units_in_magnifier(view: Node) -> void:
	var wrappers: Array = []
	for id in view.player_entity_ids:
		if view.prototype_visual_views.has(id):
			wrappers.append(view.prototype_visual_views[id])
	check(not wrappers.is_empty(), "the player's units have models")
	view.camera_distance = 1500.0
	view.target_camera_distance = 1500.0
	view._update_camera(1.0)
	check((view.camera.cull_mask & UnitVisualRoot.MODEL_LAYER) == 0, "zoomed out, the main view leaves unit models to its icons")
	view.magnifier_view.set_enabled(false)
	view._sync_unit_transforms()
	check(wrappers.all(func(wrapper): return not wrapper.get_node("ModelRoot").visible), "with the magnifier off, models are not kept loaded at map scale")
	view.magnifier_view.set_enabled(true)
	view._sync_unit_transforms()
	check(wrappers.all(func(wrapper): return wrapper.get_node("ModelRoot").visible), "with the magnifier on, models stay up for it")
	check(wrappers.all(func(wrapper): return _model_layers_ok(wrapper.get_node("ModelRoot"))), "every model mesh is on the unit model layer")
	check((view.magnifier_view.camera().cull_mask & UnitVisualRoot.MODEL_LAYER) != 0, "and the magnifier camera draws that layer")
	view.camera_distance = 300.0
	view.target_camera_distance = 300.0
	view._update_camera(1.0)
	check((view.camera.cull_mask & UnitVisualRoot.MODEL_LAYER) != 0, "close in, the main view draws unit models again")


func _run() -> void:
	var view: Node = await _start_match()
	await _test_forest_builds_in_slices(view)
	_test_camera_paces_evenly(view)
	_test_units_in_magnifier(view)
	view.queue_free()
	await process_frame
	print("PRESENTATION_PACING checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
