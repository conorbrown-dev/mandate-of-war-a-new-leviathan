extends SceneTree
## Reproducible 24-second listening reel through the real game buses/limiter.
## RTS_AUDIO_REEL=/absolute/path.wav. Uses ordinary wall time for the mixer.
const Palette := preload("res://audio/audio_palette.gd")
var cues: Array[Dictionary] = []
var meter: AudioEffectCapture
var captured_frames := 0

func _initialize() -> void:
	_run.call_deferred()

func wait_wall(seconds: float) -> void:
	var deadline := Time.get_ticks_msec() + int(seconds * 1000.0)
	while Time.get_ticks_msec() < deadline:
		await process_frame
		_drain_meter()

func _drain_meter() -> void:
	if meter != null:
		var frames := meter.get_frames_available()
		if frames > 0:
			captured_frames += meter.get_buffer(frames).size()

func recorded_seconds() -> float:
	_drain_meter()
	return float(captured_frames) / AudioServer.get_mix_rate()

func _run() -> void:
	var output := OS.get_environment("RTS_AUDIO_REEL")
	if output.is_empty():
		push_error("Set RTS_AUDIO_REEL to a WAV output path")
		quit(1)
		return
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view.set_process(false)
	var effects: Node3D = view.combat_effects
	var recorder := AudioEffectRecord.new()
	recorder.format = AudioStreamWAV.FORMAT_16_BITS
	var master := AudioServer.get_bus_index("Master")
	AudioServer.add_bus_effect(master, recorder)
	meter = AudioEffectCapture.new()
	meter.buffer_length = 0.5
	AudioServer.add_bus_effect(master, meter)
	recorder.set_recording_active(true)
	for cue in ["panel_open", "order_accepted", "order_refused", "panel_close"]:
		cues.append({"seconds": recorded_seconds(), "cue": cue})
		view.ui_audio.play_cue(cue)
		await wait_wall(0.6)
	var origin: Vector3 = view.camera.global_position - view.camera.global_basis.z * 300.0
	for type in [0, 1, 2, 8, 4]:
		cues.append({"seconds": recorded_seconds(), "unit_type": type, "cue": Palette.shot_path(type)})
		effects._play_sound({"kind": 0, "unit_type": type, "faction": 0}, origin)
		await wait_wall(1.25)
	for path in [Palette.GROUND_ENGINE, Palette.AIR_ENGINE]:
		cues.append({"seconds": recorded_seconds(), "cue": path})
		var candidates: Array[Dictionary] = [{"id": 1, "distance": 300.0, "position": origin, "path": path, "pitch": 1.0}]
		view.engine_audio.clear()
		for _pass in range(5): view.engine_audio.apply_candidates(candidates)
		await wait_wall(6.0)
		cues.back()["final_voice_gain_db"] = view.engine_audio._voices[0].volume_db
	view.engine_audio.clear()
	cues.append({"seconds": recorded_seconds(), "cue": "eight engines plus overlapping destruction stress"})
	var candidates: Array[Dictionary] = []
	for id in range(8): candidates.append({"id": id, "distance": 300.0, "position": origin + Vector3(id * 5, 0, 0), "path": Palette.GROUND_ENGINE, "pitch": 0.85 + id * 0.04})
	for _pass in range(5): view.engine_audio.apply_candidates(candidates)
	for _pass in range(20):
		effects._play_sound({"kind": 2, "faction": 0}, origin)
		await wait_wall(0.1)
	view.engine_audio.clear()
	await wait_wall(2.0)
	recorder.set_recording_active(false)
	var recording := recorder.get_recording()
	var saved := recording.save_to_wav(output)
	var discarded := meter.get_discarded_frames()
	AudioServer.remove_bus_effect(master, AudioServer.get_bus_effect_count(master) - 1)
	AudioServer.remove_bus_effect(master, AudioServer.get_bus_effect_count(master) - 1)
	var file := FileAccess.open(output.get_basename() + ".json", FileAccess.WRITE)
	file.store_string(JSON.stringify({"cues": cues, "duration": recording.get_length(), "saved": saved, "capture_discarded_frames": discarded}, "  "))
	print("AUDIO_REEL saved=%d seconds=%.2f path=%s" % [saved, recording.get_length(), output])
	view.queue_free()
	await process_frame
	await wait_wall(0.2)
	quit(0 if saved == OK and recording.get_length() > 15.0 and discarded == 0 else 1)
