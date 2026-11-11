extends RefCounted
## The player's radio preferences. Presentation only; shares user://audio.cfg
## with the mixer levels. Voice volume itself is the "Voice" bus.

const SETTINGS_PATH := "user://audio.cfg"
const SECTION := "radio"

signal changed

var voice_enabled := true
var subtitles_enabled := true
## Category id -> bool. A category missing here is on.
var categories: Dictionary = {}


func load_saved() -> void:
	var config := ConfigFile.new()
	if config.load(SETTINGS_PATH) != OK:
		return
	voice_enabled = bool(config.get_value(SECTION, "voice_enabled", true))
	subtitles_enabled = bool(config.get_value(SECTION, "subtitles_enabled", true))
	var stored: Variant = config.get_value(SECTION, "categories", {})
	categories = stored.duplicate() if stored is Dictionary else {}


func category_enabled(category: String) -> bool:
	return bool(categories.get(category, true))


func set_category(category: String, enabled: bool) -> void:
	categories[category] = enabled
	changed.emit()


func set_voice_enabled(enabled: bool) -> void:
	voice_enabled = enabled
	changed.emit()


func set_subtitles_enabled(enabled: bool) -> void:
	subtitles_enabled = enabled
	changed.emit()


func restore_defaults() -> void:
	voice_enabled = true
	subtitles_enabled = true
	categories.clear()
	changed.emit()


func save() -> Error:
	var config := ConfigFile.new()
	config.load(SETTINGS_PATH)
	config.set_value(SECTION, "voice_enabled", voice_enabled)
	config.set_value(SECTION, "subtitles_enabled", subtitles_enabled)
	config.set_value(SECTION, "categories", categories)
	return config.save(SETTINGS_PATH)
