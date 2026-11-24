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

func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	view.extension.call("skirmish_set_ai_enabled", false)
	var effects: Node3D = view.combat_effects
	effects.set_process(false)
	var engineer := int(view._player_engineer_id())
	var position: PackedFloat32Array = view.extension.call("get_unit_position", engineer)
	var spot := Vector2(position[0], position[1])
	var shooter := int(view.extension.call("create_unit_with_type", spot.x, spot.y, 0, 0))
	var victim := int(view.extension.call("create_unit_with_type", spot.x + 50, spot.y, 3, 1))
	check(shooter > 0 and victim > 0, "real battle units spawn")
	view._sync_new_entities()
	view.camera_target = Vector3(spot.x + 25, view._terrain_height_at(spot.x + 25, spot.y), spot.y)
	view.camera_distance = 130.0
	view.target_camera_distance = 130.0
	view._update_camera(1.0)
	var kinds := {}
	var last_packet := {}
	for _tick in range(25):
		# Exercise the actual main loop, including its event query and dispatch.
		view._process(0.05)
		last_packet = view.extension.call("get_combat_events", 0, 0)
		for event: Dictionary in last_packet.events:
			kinds[int(event.kind)] = true
		if kinds.size() == 3:
			break
	check(kinds.has(0) and kinds.has(1) and kinds.has(2), "native shot hit and death reach the real match loop")
	check(view.extension.call("get_removed_entities", PackedInt32Array([victim])).has(victim) and view.extension.call("get_unit_health", victim).is_empty(), "destruction corresponds to actual native death")
	check(effects._active.size() >= 3, "main loop creates visible combat nodes")
	var presented_kinds := {}
	for effect: Dictionary in effects._active:
		presented_kinds[int(effect.event.kind)] = true
		check(effect.flash.mesh != null and effect.flash.visible, "effect has renderable geometry")
	check(presented_kinds.size() == 3, "flash impact and destruction each have geometry")
	var playing := 0
	for voice: AudioStreamPlayer3D in effects._voices:
		if voice.playing:
			playing += 1
		check(voice.max_distance == effects.MAX_SOUND_DISTANCE and voice.unit_size > 0, "world audio attenuates with distance")
	check(playing > 0, "real combat starts positional sound")
	var active_before: int = effects._active.size()
	effects.consume(last_packet)
	check(effects._active.size() == active_before, "duplicate packet creates no duplicate effects")
	check(view.extension.call("get_combat_events", effects.cursor, 0).events.is_empty(), "cursor reports no repeated native events")
	check(view.extension.call("get_combat_events", 0, -1).is_empty(), "invalid viewer rejected at bridge")
	check(view.extension.call("get_combat_events", -1, 0).is_empty(), "invalid cursor rejected at bridge")

	# Optional real-renderer evidence, never substituted for state checks.
	var screenshot := OS.get_environment("RTS_COMBAT_SCREENSHOT")
	if not screenshot.is_empty() and DisplayServer.get_name() != "headless":
		for _frame in range(5):
			await process_frame
		RenderingServer.force_draw()
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png(screenshot)
		effects._process(0.65)
		for effect: Dictionary in effects._active:
			if int(effect.event.kind) == 2:
				check(effect.smoke.visible and effect.smoke.scale.x > 5.0, "destruction smoke grows after the flash")
		for _frame in range(3):
			await process_frame
		RenderingServer.force_draw()
		await RenderingServer.frame_post_draw
		root.get_texture().get_image().save_png(screenshot.get_basename() + "_smoke.png")

	for effect: Dictionary in effects._active:
		if int(effect.event.kind) == 2:
			# Probe the death's smoke phase without expiring shot/hit fixtures:
			# the sight-loss checks below still need all their original effects.
			var original_age := float(effect.age)
			var probe := screenshot.is_empty() or DisplayServer.get_name() == "headless"
			if probe:
				effect.age = original_age + 0.65
				effects._update_effect(effect)
			check(effect.flash.material_override.albedo_color.a <= 0.01 and not effect.flash.visible, "destruction flash is hidden during the smoke phase")
			if probe:
				effect.age = original_age
				effects._update_effect(effect)

	# Native history is still present after removal, but losing current sight
	# must also remove already-presented enemy effects and stop their sound.
	effects.position_visible = func(_event: Dictionary) -> bool: return false
	effects._process(0.01)
	for effect: Dictionary in effects._active:
		check(int(effect.event.faction) == 0, "enemy effect removed when sight is lost")
	for index in range(effects._voices.size()):
		var event: Dictionary = effects._voice_events[index]
		check(not effects._voices[index].playing or event.is_empty() or int(event.faction) == 0, "hidden enemy sound stops on sight loss")
	effects.clear()
	effects.consume(last_packet)
	for effect: Dictionary in effects._active:
		check(int(effect.event.faction) == 0, "hidden incoming enemy effect rejected")
	effects._process(3.0)
	check(effects._active.is_empty(), "all effects expire")
	await process_frame
	effects.clear()
	var stale: Dictionary = last_packet.duplicate(true)
	stale.tick = int(last_packet.tick) + 100
	effects.consume(stale)
	check(effects._active.is_empty(), "stale events never replay effects")
	for voice: AudioStreamPlayer3D in effects._voices:
		check(not voice.playing, "stale events never replay sound")

	# Burst a real native event through the presentation to prove bounded load.
	effects.clear()
	var burst := []
	var template: Dictionary = last_packet.events[0]
	for index in range(effects.MAX_EFFECTS + 50):
		var event: Dictionary = template.duplicate()
		event.sequence = index + 1
		event.faction = 0
		event.tick = 0
		burst.append(event)
	effects.consume({"cursor": burst.size(), "tick": 0, "events": burst})
	check(effects._active.size() == effects.MAX_EFFECTS, "effect allocation bounded under burst")
	check(effects._voices.size() == effects.MAX_VOICES, "audio voice allocation bounded under burst")
	effects.clear()
	check(effects._active.is_empty() and effects.cursor == 0, "match reset clears transient effects and cursor")
	view.queue_free()
	await process_frame
	var mixer_deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < mixer_deadline:
		await process_frame
	print("COMBAT_EFFECTS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
