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
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://audio/manifest.json"))
	var listed := {}
	for entry: Dictionary in manifest.files:
		var path := String(entry.path)
		check(not listed.has(path), "unique asset path: " + path)
		listed[path] = true
		check(FileAccess.get_sha256(path) == entry.sha256, "source bytes match manifest: " + path)
		var stream := load(path) as AudioStream
		check(stream != null and stream.get_length() > 0.0, "Godot decodes nonempty audio: " + path)
		check(not String(entry.creator).is_empty() and not String(entry.source_url).is_empty() and entry.license in ["CC0-1.0", "CC-BY-3.0"], "asset has provenance: " + path)
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	var sound: Node = view.ui_audio
	# Four cue voices, plus the one quiet voice hover ticks restart.
	check(sound.get_child_count() == 5 and sound._voices.size() == 4 and sound.get_child(4) == sound.hover_voice, "four cue voices plus one hover voice")
	for cue: String in manifest.cues:
		check(sound.play_cue(cue), "known cue accepted: " + cue)
		var voice: AudioStreamPlayer = sound.get_child((sound._next_voice + 3) % 4)
		check(voice.stream.resource_path == manifest.cues[cue] and voice.playing, "cue starts its actual stream: " + cue)
	check(not sound.play_cue("missing"), "unknown cue refused")
	sound.enabled = false
	var next: int = sound._next_voice
	check(not sound.play_cue("panel_open") and sound._next_voice == next, "disabled audio does not start a voice")
	sound.enabled = true
	for _index in range(12):
		sound.play_cue("panel_open")
	check(sound.get_child_count() == 5, "burst of cues never allocates extra voices")
	view._on_start_skirmish_pressed()
	view.extension.call("stop_simulation")
	next = sound._next_voice
	view._toggle_strategic_command()
	var voice: AudioStreamPlayer = sound.get_child((sound._next_voice + 3) % 4)
	check(sound._next_voice == (next + 1) % 4 and view.strategic_command_view.visible and voice.stream.resource_path == manifest.cues.panel_open, "real panel opening plays open cue")
	view._close_strategic_command()
	voice = sound.get_child((sound._next_voice + 3) % 4)
	check(not view.strategic_command_view.visible and voice.stream.resource_path == manifest.cues.panel_close, "real panel closing plays close cue")
	next = sound._next_voice
	view._close_strategic_command()
	check(sound._next_voice == next, "closing an already closed panel is silent")
	# Ground forces require an airfield at this point; the native answer must
	# choose refusal feedback, not a success chime when the button was clicked.
	view._on_strategic_command_order_unit(0)
	voice = sound.get_child((sound._next_voice + 3) % 4)
	check(voice.stream.resource_path == manifest.cues.order_refused and view.reinforcement_delivery_status.contains("AIRFIELD"), "native refusal chooses refusal sound")
	var spawn: Array = view.scenario_definition.player.spawn
	var site: Vector2 = view._find_engineer_spawn(Vector2(float(spawn[0]), float(spawn[1])))
	check(bool(view.extension.call("landing_zone_set", 0, site.x, site.y)), "ordinary landing zone enables native delivery for acceptance check")
	var inbound_before: int = view.extension.call("delivery_state", 0).inbound.size()
	view._on_strategic_command_order_unit(0)
	voice = sound.get_child((sound._next_voice + 3) % 4)
	check(voice.stream.resource_path == manifest.cues.order_accepted and view.extension.call("delivery_state", 0).inbound.size() == inbound_before + 1, "native inbound order chooses success sound")
	view.queue_free()
	await process_frame
	# Mixer releases stopped playback on its own thread, after node teardown.
	var mixer_deadline := Time.get_ticks_msec() + 150
	while Time.get_ticks_msec() < mixer_deadline:
		await process_frame
	print("AUDIO_LIBRARY checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
