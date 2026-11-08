extends SceneTree
## The F1 hotkey sheet lists every control, each listed action is one main.gd
## really dispatches, and F1 / Esc show and hide it in a running match.

const HotkeyManager := preload("res://hotkey_manager.gd")
const HotkeyDisplay := preload("res://hotkey_display.gd")

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _key(keycode: Key) -> void:
	var event := InputEventKey.new()
	event.keycode = keycode
	event.pressed = true
	Input.parse_input_event(event)
	var release := event.duplicate()
	release.pressed = false
	Input.parse_input_event(release)


func _rows(node: Node) -> int:
	var count := 0
	for child in node.get_children():
		if child is GridContainer:
			count += child.get_child_count() / 2
		count += _rows(child)
	return count


func _run() -> void:
	var manager: Node = HotkeyManager.new()
	root.add_child(manager)
	var sections: Array = manager.get_hotkey_sections()
	var listed := 0
	var categories: Array[String] = []
	for section in sections:
		categories.append(String(section.category))
		listed += section.hotkeys.size()
	check(categories == HotkeyManager.CATEGORY_ORDER, "the sheet has every category in order (%s)" % [categories])
	check(listed == manager.get_all_hotkeys().size(), "every hotkey is in a section (%d of %d)" % [listed, manager.get_all_hotkeys().size()])

	# Each listed action must be one main.gd handles, so the sheet never
	# advertises a key that does nothing.
	var source := FileAccess.get_file_as_string("res://main.gd")
	var handler := source.substr(source.find("func _handle_hotkey"))
	var mouse_or_elsewhere := ["zoom_in", "zoom_out", "pan_camera", "move_order", "attack_order"]
	for hotkey in manager.get_all_hotkeys():
		var action := String(hotkey.get("action", ""))
		if action.is_empty() or action in mouse_or_elsewhere:
			continue
		check(handler.contains('"%s"' % action), "main.gd handles the %s action (%s)" % [action, hotkey.label])
	for key in ["KEY_F1", "KEY_G", "KEY_X", "KEY_J", "KEY_T", "KEY_ESCAPE"]:
		check(not manager.get_hotkey(key).is_empty(), "%s is on the sheet" % key)
		check(source.contains(key), "main.gd binds %s" % key)

	var display: Control = HotkeyDisplay.new()
	display.hotkey_manager = manager
	root.add_child(display)
	check(not display.visible, "the sheet starts hidden")
	display.toggle()
	check(display.visible, "toggle shows the sheet")
	check(_rows(display) == listed, "the sheet draws a row per hotkey (%d of %d)" % [_rows(display), listed])
	display.toggle()
	check(not display.visible, "toggle hides it again")
	display.queue_free()

	# In a running match F1 opens the sheet and Esc closes it without quitting.
	var main: Node = load("res://main.tscn").instantiate()
	root.add_child(main)
	for _frame in range(10):
		await process_frame
	var sheet: Control = main.get_node_or_null("HUD/UIRoot/HotkeyDisplay")
	check(sheet != null, "the match HUD has the hotkey sheet")
	if sheet != null:
		_key(KEY_F1)
		await process_frame
		await process_frame
		check(sheet.visible, "F1 opens the hotkey sheet")
		var folder := OS.get_environment("RTS_HOTKEY_SCREENSHOT")
		if not folder.is_empty() and DisplayServer.get_name() != "headless":
			for _frame in range(5):
				await process_frame
			await RenderingServer.frame_post_draw
			root.get_texture().get_image().save_png(folder.path_join("hotkey_sheet.png"))
		_key(KEY_ESCAPE)
		await process_frame
		await process_frame
		check(not sheet.visible, "Esc closes the hotkey sheet")
		check(main.is_inside_tree(), "Esc on the sheet does not quit the game")
		_key(KEY_F1)
		await process_frame
		await process_frame
		_key(KEY_F1)
		await process_frame
		await process_frame
		check(not sheet.visible, "F1 again closes the sheet")

	# T starts Airfield placement (build type 102) for the selected engineer,
	# the key the build catalog shows for it.
	main._on_start_skirmish_pressed()
	var waited := 0
	while not main.match_started and waited < 600:
		await process_frame
		waited += 1
	check(main.match_started, "the skirmish starts")
	var engineer: int = main._player_engineer_id()
	check(engineer >= 0, "the player has a Field Engineer")
	if engineer >= 0:
		main.selected_ids = [engineer]
		_key(KEY_T)
		await process_frame
		await process_frame
		check(main.build_mode_type == 102, "T starts Airfield placement (build mode %d)" % main.build_mode_type)
	print("HOTKEY_SHEET checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
