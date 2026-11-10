extends SceneTree
const Palette := preload("res://audio/audio_palette.gd")
const Mix := preload("res://audio/audio_mix.gd")
var checks := 0
var failures := 0

func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1

func _initialize() -> void:
	_run.call_deferred()

func key(code: Key) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	root.push_input(event, true)
	var release := event.duplicate()
	release.pressed = false
	root.push_input(release, true)

func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	var panel: Control = view.audio_settings
	key(KEY_F2)
	await process_frame
	check(panel.visible and panel.sliders.Master.has_focus(), "F2 opens audio before a match with keyboard focus")
	check(panel.credits.text.contains("Nayckron") and panel.credits.text.contains("qubodup") and panel.credits.text.contains("https://creativecommons.org/licenses/by/3.0/") and panel.credits.text.contains("Source unchanged") and panel.credits.text.contains("pitch"), "player-accessible credits contain engine author, source, license and playback modifications")
	check(view.startup_overlay.find_children("*", "Button", true, false).any(func(button: Button) -> bool: return button.text.contains("AUDIO & CREDITS")), "briefing offers an audio and credits button")
	var original := float(panel.sliders.Master.value)
	key(KEY_LEFT)
	await process_frame
	check(panel.sliders.Master.value < original and panel.mix.level("Master") < original / 100.0, "keyboard slider changes the real mixer")
	panel.sliders.Combat.value = 0
	check(AudioServer.is_bus_mute(AudioServer.get_bus_index("Combat")), "zero really mutes combat")
	panel.sliders.UI.value = 37
	var restored := Mix.new()
	restored.setup()
	check(is_equal_approx(restored.level("UI"), 0.37) and restored.level("Combat") == 0.0, "volume and mute survive a preferences reload")
	for bus: String in Mix.BUSES:
		panel.sliders[bus].value = float(Mix.DEFAULTS[bus]) * 100.0
	key(KEY_ESCAPE)
	await process_frame
	check(not panel.visible and view.is_inside_tree(), "Escape closes audio without quitting")
	view._on_start_skirmish_pressed()
	view.set_process(false)
	view.extension.call("skirmish_set_ai_enabled", false)
	var engineer := int(view._player_engineer_id())
	var xy: PackedFloat32Array = view.extension.call("get_unit_position", engineer)
	var spot := Vector2(xy[0], xy[1])
	var enemy := int(view.extension.call("create_unit_with_type", spot.x, spot.y + 850.0, 3, 1))
	check(enemy > 0, "hidden enemy uses a real native entity")
	view._sync_new_entities()
	view.camera_distance = 250.0
	view.target_camera_distance = 250.0
	view.camera_target = Vector3(spot.x, view._terrain_height_at(spot.x, spot.y), spot.y)
	view._update_camera(1.0)
	view.extension.call("issue_move_commands", PackedInt32Array([engineer]), 0, spot.x + 300.0, spot.y, 10.0)
	view.extension.call("issue_move_commands", PackedInt32Array([enemy]), 1, spot.x + 300.0, spot.y + 850.0, 10.0)
	for _tick in range(60):
		view._process(0.05)
	var engines: Node3D = view.engine_audio
	check(engines._owners.has(engineer), "actual moving vehicle starts its positional engine through main loop")
	var after_enemy: PackedFloat32Array = view.extension.call("get_unit_position", enemy)
	check(Vector2(after_enemy[0], after_enemy[1]).distance_to(Vector2(spot.x, spot.y + 850.0)) > 1.0 and not view._is_unit_shown(enemy) and not engines._owners.has(enemy), "a moving hidden enemy inside audio range stays silent")
	var slot: int = engines._owners.find(engineer)
	if slot >= 0:
		var voice: AudioStreamPlayer3D = engines._voices[slot]
		check(voice.playing and voice.stream.loop and voice.bus == "Engines", "ground loop is genuinely playing on its own bus")
		check(voice.global_position.distance_to(view._unit_overlay_anchor(engineer) - Vector3.UP * 5.0) < 2.0, "loop follows drawn vehicle")
	view.extension.call("issue_stop_commands", PackedInt32Array([engineer]), 0)
	for _tick in range(60):
		view._process(0.05)
	check(not engines._owners.has(engineer), "stopped vehicle releases its loop")
	# Use a paid paradrop's real airborne cargo, not a freshly created parked plane.
	var drop_position: PackedFloat32Array = view.extension.call("get_unit_position", engineer)
	check(String(view.extension.call("order_paradrop", 0, 13, drop_position[0], drop_position[1])).is_empty(), "paid paradrop accepted on scouted ground")
	for _tick in range(180): view.extension.call("update_simulation", 50.0)
	view._sync_new_entities()
	var plane := -1
	for id in view.player_entity_ids:
		if int(view.entity_unit_types.get(id, -1)) == 17:
			plane = id
	check(plane > 0, "paid paradrop launches a real cargo aircraft")
	if plane > 0:
		var plane_xy: PackedFloat32Array = view.extension.call("get_unit_position", plane)
		view.camera_target = Vector3(plane_xy[0], view._terrain_height_at(plane_xy[0], plane_xy[1]), plane_xy[1])
		view._update_camera(1.0)
	for _tick in range(10): view._process(0.05)
	var plane_slot: int = engines._owners.find(plane)
	check(plane_slot >= 0, "native airborne transport starts an aircraft loop")
	if plane_slot >= 0:
		check(engines._voices[plane_slot].stream != engines._streams[Palette.GROUND_ENGINE] and engines._voices[plane_slot].stream.loop, "aircraft loop differs from ground engine and loops continuously")
	view.camera_target = Vector3(spot.x, view._terrain_height_at(spot.x, spot.y), spot.y)
	view._update_camera(1.0)
	# Observe actual combat-event dispatch for each armed sound family. Event
	# fixtures keep checks deterministic; combat_effects covers native event emission.
	var effects: Node3D = view.combat_effects
	effects.clear()
	var actual_paths := {}
	for type in [0, 1, 2, 8, 4]:
		effects._last_sound_ms[0] = -1000
		var index: int = effects._next_voice
		effects.consume({"tick": 1, "cursor": type + 100, "events": [{"sequence": type + 100, "tick": 1, "kind": 0, "entity": engineer, "unit_type": type, "faction": 0, "x": spot.x, "y": spot.y, "vx": 100.0, "vy": 0.0, "airborne": false}]})
		# Reset cursor between deliberately unordered type IDs.
		effects.cursor = 0
		var voice: AudioStreamPlayer3D = effects._voices[index]
		check(voice.playing and voice.bus == "Combat" and voice.stream.resource_path == Palette.shot_path(type), "type %d plays its weapon cue on combat bus" % type)
		actual_paths[voice.stream.resource_path] = true
	check(actual_paths.size() == 5, "cannon, artillery, missile, rifle and burst sound different")
	check(view.ui_audio.get_child(0).bus == "UI", "interface sounds route independently")
	key(KEY_F2)
	await process_frame
	var selected_before: PackedInt32Array = view.selected_ids.duplicate()
	var zoom_before: float = view.target_camera_distance
	key(KEY_G)
	key(KEY_SLASH)
	key(KEY_PLUS)
	check(not view.strategic_command_view.visible and view.selected_ids == selected_before and view.target_camera_distance == zoom_before and view.extension.call("get_game_speed") == 1.0, "audio modal blocks gameplay hotkeys")
	var screenshot := OS.get_environment("RTS_AUDIO_SCREENSHOT")
	if not screenshot.is_empty() and DisplayServer.get_name() != "headless":
		for _frame in range(5): await process_frame
		await RenderingServer.frame_post_draw
		check(root.get_texture().get_image().save_png(screenshot) == OK, "audio settings screenshot saved")
	key(KEY_F2)
	await process_frame
	check(not panel.visible, "F2 closes settings even with slider focus")
	var before_tick: PackedFloat64Array = view.extension.call("get_tick_clock")
	engines.update_from_view(view, 0.1)
	check(view.extension.call("get_tick_clock") == before_tick, "audio query never advances simulation")
	# Allocation stress: more audible engines than the fixed eight voices.
	var candidates: Array[Dictionary] = []
	for id in range(1000):
		candidates.append({"id": id + 10000, "position": Vector3(id, 0, 0), "distance": float(1000 - id), "path": Palette.GROUND_ENGINE, "pitch": 1.0})
	var start := Time.get_ticks_usec()
	for _pass in range(20): engines.apply_candidates(candidates)
	print("ENGINE_AUDIO allocation_1000_mean_ms=%.3f voices=%d" % [float(Time.get_ticks_usec() - start) / 20000.0, engines._voices.size()])
	check(engines.get_child_count() == 8 and engines._owners.has(10999) and not engines._owners.has(10000), "thousand-unit allocation keeps only eight nearest engine voices")
	var owners: Array = engines._owners.duplicate()
	engines.apply_candidates(candidates)
	check(engines._owners == owners, "stable nearest units retain voice ownership")
	view.match_finished = true
	engines.update_from_view(view, 0.1)
	check(engines._owners.all(func(id: int) -> bool: return id == -1), "match end stops every loop")
	view.queue_free()
	await process_frame
	var deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < deadline: await process_frame
	print("SLICE4_AUDIO checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
