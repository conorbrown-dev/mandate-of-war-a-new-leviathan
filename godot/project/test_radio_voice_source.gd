extends SceneTree

func _initialize() -> void:
	run.call_deferred()

func run() -> void:
	preload("res://audio/audio_mix.gd").new().setup()
	var radio := preload("res://audio/radio/radio_announcer.gd").new()
	root.add_child(radio)
	radio.set_process(false)
	radio.prefs.voice_enabled = true
	# user:// has no importer: this reproduces a newly installed WAV.
	var path := "user://unimported_voice.wav"
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = 24000
	var pcm := PackedByteArray()
	pcm.resize(48000)
	for i in range(24000):
		pcm.encode_s16(i * 2, int(sin(i * TAU * 440.0 / 24000.0) * 5000.0))
	wav.data = pcm
	assert(wav.save_to_wav(path) == OK)
	var manifest_path := "user://voice_manifest.json"
	var file := FileAccess.open(manifest_path, FileAccess.WRITE)
	file.store_string(JSON.stringify({"clips": [{"id": "elite_command/objective.fuel_depot_secured.v1", "path": path}]}))
	file.close()
	assert(radio.load_voice_manifest(manifest_path) == 1)
	radio.consume({"cursor": 1, "tick": 0, "events": [{"sequence": 1, "tick": 0, "kind": 0, "x": 0.0, "y": 0.0, "count": 1, "subject": 0}]})
	radio.advance(0.05)
	var ok: bool = radio.current.get("voiced", false) and radio.voice_playing()
	ok = ok and not radio.get_node("RadioAttention").playing
	var played: AudioStreamWAV = radio.get_node("RadioVoice").stream
	ok = ok and played != null and played.data == pcm
	ok = ok and AudioServer.get_bus_effect_count(AudioServer.get_bus_index("Voice")) == 0
	print("RADIO_VOICE_SOURCE checks=4 failures=", 0 if ok else 1)
	radio.free()
	await process_frame
	quit(0 if ok else 1)
