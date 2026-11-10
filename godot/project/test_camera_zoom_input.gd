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

func _dispatch(event: InputEvent) -> void:
	# Inject viewport-local coordinates: a headless OS window is only 64 px,
	# while the game viewport is scaled to desktop logical coordinates.
	root.push_input(event, true)
	await process_frame

func _key(code: Key, pressed := true, repeat := false, control := false) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = pressed
	event.echo = repeat
	event.ctrl_pressed = control
	await _dispatch(event)

func _pinch(point: Vector2, factor: float) -> void:
	var event := InputEventMagnifyGesture.new()
	event.position = point
	event.factor = factor
	await _dispatch(event)

func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	var selection: PackedInt32Array = view.selected_ids.duplicate()
	var tick_before: float = view.extension.call("get_tick_clock")[0]
	var point := root.get_visible_rect().size * Vector2(0.6, 0.65)
	var pointer := InputEventMouseMotion.new()
	pointer.position = point
	await _dispatch(pointer)
	var actual_cursor: Vector2 = view.get_viewport().get_mouse_position()
	view.target_camera_distance = 1500.0
	for expected in [4500.0,12000.0,26000.0,24.0,120.0,420.0,1500.0]:
		await _key(KEY_SLASH)
		check(is_equal_approx(view.target_camera_distance, expected), "slash cycles to %.0f m" % expected)
		check(view.zoom_focus_screen.distance_to(actual_cursor) < 1, "slash zoom anchors at the cursor")
		await _key(KEY_SLASH, false)
		check(view.target_camera_distance == expected, "slash release does not cycle again")
	await _key(KEY_SLASH, true, true)
	check(view.target_camera_distance == 1500.0, "holding slash does not race through presets")
	await _key(KEY_SLASH, false)
	await _key(KEY_SLASH, true, false, true)
	check(view.target_camera_distance == 1500.0, "Ctrl slash is not a camera shortcut")
	await _key(KEY_SLASH, false)
	await _key(KEY_KP_DIVIDE)
	check(view.target_camera_distance == 4500.0, "numpad divide cycles zoom")
	await _key(KEY_KP_DIVIDE, false)
	for code in [KEY_PLUS,KEY_EQUAL,KEY_KP_ADD,KEY_MINUS,KEY_KP_SUBTRACT]:
		await _key(code)
		check(view.target_camera_distance == 4500.0, "speed shortcuts do not change camera zoom")
		await _key(code,false)
	view.extension.call("set_game_speed",1.0)
	view.target_camera_distance = 1500.0

	await _pinch(point, 1.25)
	check(is_equal_approx(view.target_camera_distance, 1200.0), "spreading fingers smoothly zooms in by reciprocal factor")
	await _pinch(point, 0.8)
	check(is_equal_approx(view.target_camera_distance, 1500.0), "pinching fingers zooms out and reverses the spread")
	for _sample in range(10):
		await _pinch(point, 1.01)
	check(is_equal_approx(view.target_camera_distance, 1500.0 / pow(1.01, 10)), "small trackpad deltas accumulate smoothly, not as wheel notches")
	for factor in [0.0, -1.0, NAN, INF, 1.0]:
		var before: float = view.target_camera_distance
		await _pinch(point, factor)
		check(view.target_camera_distance == before and is_finite(view.target_camera_distance), "invalid/neutral pinch factor leaves zoom intact")

	await _pinch(point, 10000.0)
	check(view.target_camera_distance == view.CAMERA_MIN_DISTANCE, "pinch respects closest zoom")
	await _pinch(point, 0.000001)
	check(view.target_camera_distance == view.CAMERA_MAX_DISTANCE, "pinch respects widest zoom")

	# Existing camera contract allows at most 25 m anchor error on this relief.
	view.camera_distance = 1500.0
	view.target_camera_distance = 1500.0
	view.zoom_focus_pending = false
	view._update_camera(1.0)
	var ground_before: Vector2 = view._screen_to_world(point)
	await _pinch(point, 1.2)
	for _frame in range(150):
		view._update_camera(1.0 / 60.0)
	var error: float = ground_before.distance_to(view._screen_to_world(point))
	check(error <= 25.0, "pinch preserves terrain under cursor (error %.3f m)" % error)
	check(view.camera_distance < 1500.0, "pinch changes the rendered camera distance")
	check(view.selected_ids == selection, "zoom does not alter selection")
	check(view.extension.call("get_tick_clock")[0] == tick_before, "zoom does not advance native simulation")

	# Unhandled shortcuts must leave real text entry alone.
	var edit := LineEdit.new()
	edit.position = Vector2(20, 300)
	edit.size = Vector2(220, 40)
	view.ui_root.add_child(edit)
	edit.grab_focus()
	var before_text: float = view.target_camera_distance
	var text_key := InputEventKey.new()
	text_key.keycode = KEY_PLUS
	text_key.unicode = 43
	text_key.pressed = true
	await _dispatch(text_key)
	check(edit.text == "+" and view.target_camera_distance == before_text, "typing plus in a focused field does not zoom")
	await _key(KEY_PLUS, false)
	edit.queue_free()
	await process_frame
	for key in ["KEY_SLASH", "KEY_PLUS", "KEY_MINUS", "TRACKPAD_PINCH"]:
		check(not view.hotkey_manager.get_description(key).is_empty(), "F1 sheet explains " + key)
	view.queue_free()
	await process_frame
	var deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < deadline:
		await process_frame
	print("CAMERA_ZOOM_INPUT checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
