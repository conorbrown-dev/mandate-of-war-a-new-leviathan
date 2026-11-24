extends SceneTree
## Rendered evidence for the command post: the player's post at the default
## 420 m camera, a closer view, and the HUD with the post selected.
## Run without --headless:
##   Godot --path godot/project --script res://capture_command_post.gd -- <folder> <tag>

var folder := ""
var tag := "after"


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	folder = args[0] if args.size() > 0 else "user://command_post"
	tag = args[1] if args.size() > 1 else "after"
	DirAccess.make_dir_recursive_absolute(folder)
	_run.call_deferred()


func _shot(name: String) -> void:
	for _frame in range(30):
		await process_frame
	await RenderingServer.frame_post_draw
	var path := folder.path_join("%s_%s.png" % [tag, name])
	root.get_texture().get_image().save_png(path)
	print("COMMAND_POST_CAPTURE ", path)


func _aim(view: Node, world: Vector2, distance: float, yaw := 0.0) -> void:
	view.camera_target = Vector3(world.x, view._terrain_height_at(world.x, world.y), world.y)
	view.camera_distance = distance
	view.target_camera_distance = distance
	view.tactical_camera_yaw = yaw
	for _step in range(40):
		view._update_camera(0.1)


func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	for _frame in range(10):
		await process_frame
	view.extension.call("skirmish_set_ai_enabled", false)
	var spawn: Array = view.scenario_definition.player.spawn
	var post := Vector2(float(spawn[0]), float(spawn[1]))
	_aim(view, post, 420.0, 0.6)
	await _shot("default_420m")
	_aim(view, post, 70.0, 0.6)
	await _shot("close_70m")
	_aim(view, post, 70.0, 2.6)
	await _shot("close_70m_rear")
	_aim(view, post, 420.0, 0.6)
	if view.has_method("_select_command_post"):
		view._clear_selection()
		view._select_command_post()
		view._update_hud()
	await _shot("selected_hud")
	print("COMMAND_POST_CAPTURE done")
	quit()
