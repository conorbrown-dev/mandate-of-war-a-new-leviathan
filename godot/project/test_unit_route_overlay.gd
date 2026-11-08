extends SceneTree
## Held-Shift unit routes. The native simulation reports the route a unit's
## movement will follow, turns included; the overlay draws exactly that route;
## and the unit then drives through the corners it was shown. In a real
## skirmish, holding Shift shows the selected units' routes and releasing it
## hides them.

const ELITE_MBT := 0
const Renderer := preload("res://unit_route_renderer.gd")
const UiTokens := preload("res://ui/theme/ui_tokens.gd")
const GROUND_HEIGHT := 3.5

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	OS.set_environment("RTS_DATA_ROOT", ProjectSettings.globalize_path("res://../../data").simplify_path())
	_run.call_deferred()


func _distance_to_polyline(point: Vector2, route: PackedVector2Array) -> float:
	var best := INF
	for index in range(route.size() - 1):
		best = minf(best, point.distance_to(Geometry2D.get_closest_point_to_segment(point, route[index], route[index + 1])))
	return best


func _test_route_matches_drive() -> void:
	var extension: Object = ClassDB.instantiate("RtsExtension")
	extension.call("start_simulation")
	# A block of civilian buildings sits on the direct line, so the order must bend.
	extension.call("block_civilian_area", 0.0, 0.0, 18.0)
	var tank := int(extension.call("create_unit_with_type", -45.0, 0.0, ELITE_MBT, 0))
	check(tank > 0, "a tank spawns west of the obstacle")
	if tank <= 0:
		return
	var destination := Vector2(45.0, 0.0)
	check(int(extension.call("issue_move_commands", PackedInt32Array([tank]), 0, destination.x, destination.y, 3.0)) == 1,
		"the tank accepts a move order across the obstacle")
	extension.call("update_simulation", 50.0)

	var routes: Array = extension.call("get_unit_routes", PackedInt32Array([tank]), 0)
	check(routes.size() == 1, "one route is returned per queried unit")
	var route: PackedVector2Array = routes[0] if routes.size() == 1 else PackedVector2Array()
	check(route.size() >= 3, "the route around the obstacle has at least one turn (%d points)" % route.size())
	if route.size() < 3:
		return
	var position: PackedFloat32Array = extension.call("get_unit_position", tank)
	check(route[0].distance_to(Vector2(position[0], position[1])) < 0.01, "the route starts at the unit")
	check(route[route.size() - 1].distance_to(destination) < 0.75, "the route ends at the ordered destination")
	for index in range(1, route.size() - 1):
		check(route[index].length() > 17.0, "corner %d stays outside the obstacle" % index)
	var foreign: Array = extension.call("get_unit_routes", PackedInt32Array([tank]), 1)
	check(foreign.size() == 1 and (foreign[0] as PackedVector2Array).is_empty(), "another player cannot read this unit's route")

	# The overlay draws exactly the native route, on the ground.
	var renderer := Renderer.new()
	renderer.set_terrain_height_sampler(func(_x: float, _z: float): return GROUND_HEIGHT)
	root.add_child(renderer)
	renderer.show_routes(routes)
	check(renderer.visible, "the overlay shows a route")
	check(renderer.drawn_routes.size() == 1 and renderer.drawn_routes[0] == route, "the overlay draws the route the simulation reported")
	check(renderer.corner_count() == route.size() - 2, "the overlay marks every turn")
	check(renderer._material.albedo_color.is_equal_approx(Color(UiTokens.ACCENT, 0.55)), "routes use the command accent token")
	var mesh: ArrayMesh = renderer.route_mesh_instance.mesh
	check(mesh != null, "the overlay builds a route mesh")
	if mesh != null:
		var vertices: PackedVector3Array = mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
		var mark := maxf(renderer._width * 1.6, 0.8)
		var off_route := 0
		for vertex in vertices:
			if not is_equal_approx(vertex.y, GROUND_HEIGHT + Renderer.HEIGHT_OFFSET) and not is_equal_approx(vertex.y, GROUND_HEIGHT + Renderer.HEIGHT_OFFSET + 0.01):
				off_route += 1
			elif _distance_to_polyline(Vector2(vertex.x, vertex.z), route) > mark + 0.01:
				off_route += 1
		check(off_route == 0, "every drawn vertex lies on the ground along the reported route (%d stray)" % off_route)
		for index in range(1, route.size() - 1):
			var marked := false
			for vertex in vertices:
				if absf(Vector2(vertex.x, vertex.z).distance_to(route[index]) - mark) < 0.01:
					marked = true
					break
			check(marked, "turn %d has a corner mark" % index)

	# The tank then drives through the corners it was shown, and the route
	# re-read mid-drive starts from where the tank now is.
	var corners := route.slice(1, route.size() - 1)
	var closest: Array[float] = []
	closest.resize(corners.size())
	closest.fill(INF)
	var followed := false
	var deviation := 0.0
	for tick in range(1500):
		if not bool(extension.call("has_unit_move_target", tank)):
			break
		extension.call("update_simulation", 50.0)
		position = extension.call("get_unit_position", tank)
		var here := Vector2(position[0], position[1])
		for index in range(corners.size()):
			closest[index] = minf(closest[index], here.distance_to(corners[index]))
		deviation = maxf(deviation, _distance_to_polyline(here, route))
		if tick == 30:
			var moving: PackedVector2Array = (extension.call("get_unit_routes", PackedInt32Array([tank]), 0) as Array)[0]
			followed = moving.size() >= 2 and moving[0].distance_to(here) < 0.01 and moving[moving.size() - 1].distance_to(destination) < 0.75
	# The drawn route is the 1 m cell path; the hull swings wide of each turn by
	# up to its 4 m turning radius (data/unit_faction_stats.json).
	check(deviation < 6.0, "the tank stays within two cells plus its turning radius of the route it was shown (%.2f m)" % deviation)
	check(followed, "the route re-read mid-drive starts at the moving tank")
	check(not bool(extension.call("has_unit_move_target", tank)), "the tank reaches its destination")
	for index in range(corners.size()):
		check(closest[index] < 2.0, "the tank passes within 2 m of corner %d (closest %.2f m)" % [index, closest[index]])
	var arrived: Array = extension.call("get_unit_routes", PackedInt32Array([tank]), 0)
	check((arrived[0] as PackedVector2Array).is_empty(), "an arrived unit has no route")
	renderer.show_routes(arrived)
	check(not renderer.visible, "the overlay hides when no selected unit is moving")
	renderer.queue_free()
	extension.call("reset_simulation")


func _set_shift(pressed: bool) -> void:
	var event := InputEventKey.new()
	event.keycode = KEY_SHIFT
	event.physical_keycode = KEY_SHIFT
	event.pressed = pressed
	Input.parse_input_event(event)
	Input.flush_buffered_events()


func _test_shift_in_skirmish() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	await process_frame
	view.set_process(false)
	check(view.match_started, "the skirmish starts")
	if not view.match_started:
		view.queue_free()
		return
	var extension: Object = view.extension
	var engineer := int(view._player_engineer_id())
	var start: Array = extension.call("get_unit_position", engineer)
	var destination := Vector2(float(start[0]) + 900.0, float(start[1]) - 400.0)
	check(int(extension.call("issue_move_commands", PackedInt32Array([engineer]), 0, destination.x, destination.y, 10.0)) == 1,
		"the engineer accepts a move order")
	extension.call("update_simulation", 50.0)
	view._clear_selection()
	view._set_selected(engineer, true)

	view._update_unit_routes(1.0)
	check(view.unit_route_renderer == null or not view.unit_route_renderer.visible, "routes stay hidden without Shift")
	_set_shift(true)
	check(Input.is_key_pressed(KEY_SHIFT), "the test holds Shift")
	view._update_unit_routes(1.0)
	var renderer: Node3D = view.unit_route_renderer
	check(renderer != null and renderer.visible, "holding Shift shows the selected unit's route")
	if renderer != null:
		var native: Array = extension.call("get_unit_routes", PackedInt32Array([engineer]), 0)
		check(renderer.drawn_routes.size() == 1 and renderer.drawn_routes[0] == native[0], "the skirmish draws the native route")
		# Held Shift keeps refreshing the route as the unit drives.
		for _tick in range(20):
			extension.call("update_simulation", 50.0)
		view._update_unit_routes(1.0)
		position_check(extension, engineer, renderer)
	_set_shift(false)
	view._update_unit_routes(1.0)
	check(renderer == null or not renderer.visible, "releasing Shift hides the routes")
	view.queue_free()
	await process_frame


func position_check(extension: Object, engineer: int, renderer: Node3D) -> void:
	var position: PackedFloat32Array = extension.call("get_unit_position", engineer)
	check(renderer.drawn_routes.size() == 1 and renderer.drawn_routes[0][0].distance_to(Vector2(position[0], position[1])) < 0.01,
		"the drawn route starts at the unit after it moves")


func _run() -> void:
	_test_route_matches_drive()
	await _test_shift_in_skirmish()
	print("UNIT_ROUTE_OVERLAY checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
