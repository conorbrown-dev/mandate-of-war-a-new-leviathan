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

func key(code: Key) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	root.push_input(event, true)
	await process_frame
	var release := event.duplicate()
	release.pressed = false
	root.push_input(release, true)
	await process_frame

func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	await key(KEY_ESCAPE)
	check(view.is_inside_tree() and view.exit_confirm != null and view.exit_confirm.visible, "Esc asks before leaving a match")
	var stay: Button = view.exit_confirm.find_child("Stay", true, false)
	check(stay != null and view.exit_confirm.find_child("Exit", true, false) != null, "confirmation offers stay and exit actions")
	var speed: float = view.extension.call("get_game_speed")
	await key(KEY_PLUS)
	check(view.extension.call("get_game_speed") == speed, "confirmation blocks match hotkeys")
	await key(KEY_ESCAPE)
	check(view.is_inside_tree() and not view.exit_confirm.visible, "Esc dismisses confirmation without exiting")
	await key(KEY_ESCAPE)
	check(view.exit_confirm.visible, "Esc can reopen confirmation")
	stay.emit_signal("pressed")
	check(not view.exit_confirm.visible and view.is_inside_tree(), "Stay keeps the match open")
	view.queue_free()
	await process_frame
	print("EXIT_CONFIRMATION checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
