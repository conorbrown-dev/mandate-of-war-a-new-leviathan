extends SceneTree

func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	OS.set_environment("RTS_DATA_ROOT", ProjectSettings.globalize_path("res://../../data").simplify_path())
	var scene: PackedScene = load("res://main.tscn")
	var view: Node = scene.instantiate()
	root.add_child(view)
	await process_frame
	view.call("_on_start_skirmish_pressed")
	view.set_process(false)
	await process_frame

	var engineer_id := int(view.call("_player_engineer_id"))
	var extension: Object = view.get("extension")
	assert(engineer_id > 0 and extension != null)
	var start: PackedFloat32Array = extension.call("get_unit_position", engineer_id)
	assert(start.size() == 2)
	var ids := PackedInt32Array([engineer_id])
	assert(int(extension.call("issue_move_commands", ids, 0, start[0] + 25.0, start[1], 3.0)) == 1)

	var aligned_samples := 0
	var minimum_alignment := 1.0
	var arrived := false
	var previous := Vector2(start[0], start[1])
	for _tick in range(400):
		extension.call("update_simulation", 50.0)
		view.call("_sync_new_entities")
		view.call("_sync_unit_transforms")
		var position: PackedFloat32Array = extension.call("get_unit_position", engineer_id)
		assert(position.size() == 2)
		var current := Vector2(position[0], position[1])
		var motion := current - previous
		var wrapper: Node3D = view.get("prototype_visual_views").get(engineer_id, null)
		assert(wrapper != null)
		if motion.length() > 0.001:
			# The Boxer donor's plow/cab nose is authored along local +X. Its
			# visual-definition rotation must map that physical nose to native travel.
			var model_root: Node3D = wrapper.get_node("ModelRoot")
			var visual_forward := Vector2(model_root.global_basis.x.x, model_root.global_basis.x.z).normalized()
			minimum_alignment = minf(minimum_alignment, visual_forward.dot(motion.normalized()))
			aligned_samples += 1
		previous = current
		if current.distance_to(Vector2(start[0] + 25.0, start[1])) <= 0.75:
			arrived = true
			break

	assert(arrived)
	assert(aligned_samples > 0 and minimum_alignment >= 0.98)
	view.queue_free()
	print("ENGINEER_MOVEMENT_PRESENTATION checks=4 failures=0")
	quit()
