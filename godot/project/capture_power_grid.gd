extends SceneTree
## Rendered evidence for the power grid: the station model, steel pylons, the
## wooden line beside the supply road, the strait crossing, a cut line throwing
## sparks, and the map view. Every line is revealed for the pictures; in a
## match the enemy's and the civilian grid stay hidden until scouted.
## Run without --headless:
##   Godot --path godot/project --script res://capture_power_grid.gd -- <folder>

var folder := ""


func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	folder = args[0] if args.size() > 0 else "user://power_grid"
	DirAccess.make_dir_recursive_absolute(folder)
	_run.call_deferred()


func _shot(name: String, frames := 30) -> void:
	for _frame in range(frames):
		await process_frame
	await RenderingServer.frame_post_draw
	var path := folder.path_join("%s.png" % name)
	root.get_texture().get_image().save_png(path)
	print("POWER_GRID_CAPTURE ", path)


func _aim(view: Node, world: Vector2, distance: float, yaw := 0.0) -> void:
	view.camera_target = Vector3(world.x, maxf(view._terrain_height_at(world.x, world.y), 0.0), world.y)
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
	var native: Object = view.extension
	native.call("skirmish_set_ai_enabled", false)
	var grid: Node3D = view.power_grid_view
	for node: Dictionary in grid.nodes.values():
		node.discovered = true
		node.known_owner = int(node.owner)
		grid._rebuild_node_view(node)
	for line: Dictionary in grid.lines.values():
		line.discovered = true
		line.known_owner = int(line.owner)
		grid._rebuild_line_view(line)
	var harbor: Vector2 = grid.nodes[1].position
	_aim(view, harbor, 330.0, 0.7)
	for _frame in range(90):
		await process_frame
	_aim(view, harbor, 330.0, 0.7)
	await _shot("power_station")
	_aim(view, harbor, 130.0, 2.2)
	await _shot("power_station_switchyard")
	var trunk: Array = grid.lines[101].supports
	_aim(view, trunk[2], 160.0, 1.1)
	await _shot("steel_pylons")
	_aim(view, Vector2(-12125, 2000), 60.0, 0.5)
	await _shot("wooden_line_beside_road")
	_aim(view, Vector2(-12125, 2400), 420.0, 0.2)
	await _shot("wooden_line_420m")
	_aim(view, Vector2(-6200, 1960), 900.0, 0.9)
	await _shot("strait_crossing")
	_aim(view, Vector2(-12125, 2400), 4500.0, 0.0)
	await _shot("map_view_4500m")
	# Cut the player's line and photograph the break while it arcs.
	var raider := int(native.call("create_unit_with_type", -12200.0, 2000.0, 3, 1))
	native.call("power_order", raider, 1, 3, 102)
	for _frame in range(240):
		await process_frame
		if not bool(grid.lines[102].known_intact):
			break
	native.call("destroy_unit", raider)
	var cut: Vector2 = grid.lines[102].known_cut
	_aim(view, cut, 45.0, 0.5)
	for _frame in range(20):
		await process_frame
	grid._burst_sparks(grid.lines[102])
	await _shot("cut_line_sparks", 14)
	_aim(view, cut, 420.0, 0.2)
	await _shot("cut_line_420m_hud")
	print("POWER_GRID_CAPTURE done known_intact=", grid.lines[102].known_intact)
	quit()
