extends SceneTree
## Saves screenshots of every main UI surface for visual review. Presentation
## only; it asserts nothing. Run with a display (not --headless):
##   Godot --path godot/project --resolution 1920x1080 --script res://tools/capture_ui_tour.gd
## Set RTS_UI_TOUR_DIR to choose the output folder (default user://ui_tour).

var folder := ""


func _initialize() -> void:
	_run.call_deferred()


func _shot(name: String) -> void:
	for i in range(4):
		await process_frame
	RenderingServer.force_draw()
	await RenderingServer.frame_post_draw
	var path := folder.path_join(name + ".png")
	root.get_texture().get_image().save_png(path)
	print("UI_TOUR saved ", path)


func _key(code: Key) -> void:
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
	folder = OS.get_environment("RTS_UI_TOUR_DIR")
	if folder.is_empty():
		folder = ProjectSettings.globalize_path("user://ui_tour")
	DirAccess.make_dir_recursive_absolute(folder)
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await _shot("01_briefing")
	view._on_start_skirmish_pressed()
	for i in range(30):
		await process_frame
	await _shot("02_hud_idle")
	var engineer: int = view._player_engineer_id()
	if engineer > 0:
		view._set_selected(engineer, true)
		view._update_hud()
		await _shot("03_hud_engineer")
		view._set_selected(engineer, false)
	var ids: PackedInt32Array = view.player_entity_ids
	for id in ids:
		view._set_selected(id, true)
	view._update_hud()
	await _shot("04_hud_group")
	for id in ids:
		view._set_selected(id, false)
	view._toggle_strategic_command()
	await _shot("05_strategic_command")
	view._close_strategic_command()
	if view.hotkey_display != null:
		view.hotkey_display.set_sheet_visible(true)
		await _shot("06_hotkeys")
		view.hotkey_display.set_sheet_visible(false)
	var end_panel_script = load("res://ui/components/match_end_panel.gd")
	var end_panel: Control = end_panel_script.new()
	end_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	view.ui_root.add_child(end_panel)
	end_panel.show_result("VICTORY", "ENEMY COMMAND POST DESTROYED", "MATCH TIME 12:41  //  ENEMY WAVES 4\nENEMY COMMANDER  SWARM  //  HARD", Color("#7ad99b"))
	await _shot("07_match_end")
	view.audio_settings.toggle()
	await _shot("08_audio_settings")
	print("UI_TOUR done ", folder)
	quit()
