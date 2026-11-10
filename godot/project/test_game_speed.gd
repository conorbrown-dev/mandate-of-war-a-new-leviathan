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

func key(code: Key, echo := false, control := false, unicode_value := 0) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	event.echo = echo
	event.ctrl_pressed = control
	event.unicode = unicode_value
	root.push_input(event,true)
	await process_frame
	var release := event.duplicate()
	release.pressed = false
	root.push_input(release,true)
	await process_frame

func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	var extension: Object = view.extension
	check(extension.call("get_game_speed")==1.0, "a new match starts at 100 percent")
	var camera_before: float = view.target_camera_distance
	for code in [KEY_PLUS,KEY_EQUAL,KEY_KP_ADD]:
		await key(code)
	check(extension.call("get_game_speed")==1.75, "all plus aliases step up to 175 percent")
	await key(KEY_PLUS)
	check(extension.call("get_game_speed")==1.75, "speed clamps at 175 percent")
	for i in range(6): await key(KEY_MINUS)
	check(extension.call("get_game_speed")==0.25, "minus reaches 25 percent")
	await key(KEY_KP_SUBTRACT)
	check(extension.call("get_game_speed")==0.25, "numpad minus respects the lower clamp")
	await key(KEY_KP_ADD)
	check(extension.call("get_game_speed")==0.5, "numpad speed input steps by 25 percent")
	await key(KEY_PLUS,true)
	check(extension.call("get_game_speed")==0.5, "key echo does not race speed to a limit")
	await key(KEY_PLUS,false,true)
	check(extension.call("get_game_speed")==0.5, "modified shortcuts are left alone")
	check(view.target_camera_distance==camera_before, "speed controls do not zoom")
	check(view.command_hud.snapshot.game_speed==50 and not view.command_hud.snapshot.speed_locked, "HUD reports current speed and offline availability")
	for scale in [0.25,0.5,0.75,1.0,1.25,1.5,1.75]:
		extension.call("set_game_speed",scale)
		var before: float = extension.call("get_tick_clock")[0]
		for i in range(4): extension.call("update_simulation",250.0)
		check(extension.call("get_tick_clock")[0]-before==20.0*scale, "one wall second runs at %d percent" % roundi(scale*100))
	# Exercise the actual presentation update path with a known wall delta.
	extension.call("set_game_speed",1.75)
	var before_process: float = extension.call("get_tick_clock")[0]
	for i in range(4): view._process(0.25)
	check(extension.call("get_tick_clock")[0]-before_process==35, "the real game loop uses the native speed setting")
	var peer := ENetMultiplayerPeer.new()
	peer.set_bind_ip("127.0.0.1")
	var opened := peer.create_server(0)
	check(opened == OK, "real Godot multiplayer listener opens")
	if opened == OK:
		root.multiplayer.multiplayer_peer = peer
		view._enforce_game_speed_policy()
		await key(KEY_PLUS)
		check(extension.call("get_game_speed")==1.0 and not view._can_change_game_speed(), "multiplayer blocks speed changes and resets pacing")
		view._update_hud()
		check(view.command_hud.snapshot.speed_locked and view.command_hud.snapshot.game_speed==100, "HUD shows multiplayer speed locked at 100 percent")
		var before_multiplayer: float = extension.call("get_tick_clock")[0]
		for i in range(4): view._process(0.25)
		check(extension.call("get_tick_clock")[0]-before_multiplayer==20, "Godot multiplayer game loop runs at normal speed")
		peer.close()
		root.multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
	var field := LineEdit.new()
	field.position = Vector2(30,300)
	field.size = Vector2(250,40)
	view.ui_root.add_child(field)
	field.grab_focus()
	var speed_before_text: float = extension.call("get_game_speed")
	await key(KEY_PLUS,false,false,43)
	await key(KEY_MINUS,false,false,45)
	await key(KEY_SLASH,false,false,47)
	check(field.text=="+-/", "focused text fields receive the remapped keys")
	check(extension.call("get_game_speed")==speed_before_text and view.target_camera_distance==camera_before, "typing does not change game speed or camera zoom")
	field.queue_free()
	await process_frame
	check(view.hotkey_manager.get_description("KEY_PLUS").contains("175%") and view.hotkey_manager.get_description("KEY_MINUS").contains("25%"), "F1 documents speed bounds")
	check(view.hotkey_manager.get_description("KEY_SLASH").contains("Cycle zoom"), "F1 documents slash zoom cycling")
	extension.call("set_game_speed",1.75)
	view._update_hud()
	if not OS.get_environment("RTS_SPEED_SCREENSHOT").is_empty():
		for i in range(3): await process_frame
		RenderingServer.force_draw()
		await RenderingServer.frame_post_draw
		check(root.get_texture().get_image().save_png(OS.get_environment("RTS_SPEED_SCREENSHOT"))==OK, "game-speed HUD checkpoint saved")
	extension.call("stop_simulation")
	extension.call("start_simulation")
	check(extension.call("get_game_speed")==1.0, "restarting native simulation restores 100 percent")
	view.queue_free()
	await process_frame
	var deadline := Time.get_ticks_msec()+150
	while Time.get_ticks_msec()<deadline: await process_frame
	print("GAME_SPEED checks=%d failures=%d" % [checks,failures])
	quit(1 if failures else 0)
