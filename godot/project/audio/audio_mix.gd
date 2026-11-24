extends RefCounted
## Presentation preferences only. No Engine.time_scale or native state changes.

const SETTINGS_PATH := "user://audio.cfg"
const BUSES := ["Master", "UI", "Combat", "Engines", "Voice"]
const DEFAULTS := {"Master": 0.8, "UI": 0.8, "Combat": 0.8, "Engines": 0.6, "Voice": 0.8}

func setup() -> void:
	for bus: String in BUSES:
		if AudioServer.get_bus_index(bus) < 0:
			AudioServer.add_bus()
			var index := AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, bus)
			AudioServer.set_bus_send(index, "Master")
	# A final safety ceiling also catches overlapping destruction sounds.
	var master := AudioServer.get_bus_index("Master")
	var limited := false
	for index in range(AudioServer.get_bus_effect_count(master)):
		limited = limited or AudioServer.get_bus_effect(master, index) is AudioEffectLimiter
	if not limited:
		var limiter := AudioEffectLimiter.new()
		limiter.ceiling_db = -1.0
		AudioServer.add_bus_effect(master, limiter)
	var config := ConfigFile.new()
	config.load(SETTINGS_PATH)
	for bus: String in BUSES:
		var stored: Variant = config.get_value("volume", bus, DEFAULTS[bus])
		var value := float(stored) if typeof(stored) in [TYPE_FLOAT, TYPE_INT] else float(DEFAULTS[bus])
		set_level(bus, value if is_finite(value) else float(DEFAULTS[bus]))

func level(bus: String) -> float:
	var index := AudioServer.get_bus_index(bus)
	if index < 0 or AudioServer.is_bus_mute(index):
		return 0.0
	return db_to_linear(AudioServer.get_bus_volume_db(index))

func set_level(bus: String, value: float) -> void:
	if bus not in BUSES or not is_finite(value):
		return
	var index := AudioServer.get_bus_index(bus)
	value = clampf(value, 0.0, 1.0)
	AudioServer.set_bus_mute(index, value <= 0.0)
	AudioServer.set_bus_volume_db(index, linear_to_db(maxf(value, 0.0001)))

func save() -> Error:
	var config := ConfigFile.new()
	# Other sections (radio preferences) share this file; keep them.
	config.load(SETTINGS_PATH)
	for bus: String in BUSES:
		config.set_value("volume", bus, level(bus))
	return config.save(SETTINGS_PATH)
