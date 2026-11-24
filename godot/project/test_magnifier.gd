extends SceneTree
## The magnifier inset shares the main world, looks at the focus point from the
## main camera's bearing at a closer distance, and zoom/toggle behave.

const MagnifierView := preload("res://ui/components/magnifier_view.gd")

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
	var main_camera := Camera3D.new()
	root.add_child(main_camera)
	main_camera.global_position = Vector3(0, 780, 1000)
	main_camera.look_at(Vector3.ZERO, Vector3.UP)
	var view := MagnifierView.new()
	root.add_child(view)
	await process_frame
	check(view.camera().get_viewport().world_3d == root.world_3d, "shares the main World3D, so fog and hidden units match")
	check(view.zoom_factor() == 8.0, "defaults to 8x")
	view.cycle_zoom(-1)
	check(view.zoom_factor() == 8.0, "8x is the most zoomed out")
	view.cycle_zoom(5)
	check(view.zoom_factor() == 32.0, "zoom clamps at 32x")
	view.cycle_zoom(-5)
	check(view.zoom_factor() == 8.0, "zoom returns to 8x and stops")
	var focus := Vector2(120, -40)
	view.update_view(main_camera, 1000.0, focus, 5.0)
	var look := Vector3(focus.x, 5.0, focus.y)
	var gap := view.camera().global_position.distance_to(look)
	check(absf(gap - 125.0) < 0.5, "camera sits main distance / zoom from the focus (got %f)" % gap)
	var toward := (look - view.camera().global_position).normalized()
	check(toward.dot(-view.camera().global_transform.basis.z) > 0.999, "camera looks at the focus")
	check(toward.dot(-main_camera.global_transform.basis.z) > 0.999, "same bearing and pitch as the main camera")
	view.update_view(main_camera, 1000.0, null, 0.0)
	check(view.camera().global_position.distance_to(look) < 126.0, "no focus keeps the last view")
	check(is_equal_approx(view.modulate.a, 0.6), "holding over HUD or off-map dims the inset")
	view.set_enabled(false)
	check(not view.visible, "toggled off hides the inset")
	view.update_view(main_camera, 1000.0, Vector2(900, 900), 0.0)
	check(absf(view.camera().global_position.distance_to(look) - 125.0) < 0.5, "disabled inset does not move")
	view.queue_free()
	main_camera.queue_free()
	await process_frame
	var match_view: Node = load("res://main.tscn").instantiate()
	root.add_child(match_view)
	await process_frame
	match_view._on_start_skirmish_pressed()
	match_view.set_process(false)
	match_view._process(0.05)
	check(match_view.magnifier_view.has_target(), "real match seeds a useful inset before the first battlefield hover")
	check(match_view.magnifier_view.camera().global_position.length() > 1000.0, "startup inset camera leaves the uninitialised world origin")
	match_view.queue_free()
	await process_frame
	var deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < deadline: await process_frame
	print("magnifier checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
