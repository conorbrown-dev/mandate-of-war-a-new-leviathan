extends SceneTree

var checks := 0
var failures := 0

func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	# Every unfinished structure must retain the placement silhouette, including
	# the airfield's full runway footprint and the thin radar/floodlight masts.
	for build_type in [100, 101, 102, 103]:
		var ghost: Node3D = view._make_build_ghost(build_type)
		var placement_material: Material = view.build_ghost_material
		for progress in [0.0, 0.5, 0.99]:
			view._sync_construction_frames([{"type": build_type, "name": "fixture", "target_x": -16500.0, "target_y": -1500.0, "progress": progress}])
			await process_frame
			var frame: Node3D = view.construction_frames.get_child(0)
			var matches := frame.get_child_count() == ghost.get_child_count()
			if matches:
				for index in range(ghost.get_child_count()):
					var preview := ghost.get_child(index) as MeshInstance3D
					var skeleton := frame.get_child(index) as MeshInstance3D
					matches = matches and skeleton != null and skeleton.mesh.get_class() == preview.mesh.get_class() and skeleton.mesh.get_aabb() == preview.mesh.get_aabb() and skeleton.transform == preview.transform
			check(matches, "construction %d at %.2f matches every placement mesh and transform" % [build_type, progress])
			check(frame.position == Vector3(-16500.0, view._terrain_height_at(-16500.0, -1500.0) + 0.08, -1500.0), "construction uses the placement terrain offset and queued site")
			check(view.build_ghost_material == placement_material, "construction leaves active placement material intact")
		ghost.free()
	view._sync_construction_frames([])
	await process_frame
	check(view.construction_frames.get_child_count() == 0, "completed or cancelled queue removes construction geometry")

	view.queue_free()
	await process_frame
	print("CONSTRUCTION_PREVIEW checks=%d failures=%d" % [checks, failures])
	quit(0 if failures == 0 else 1)
