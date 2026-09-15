class_name UiScaleSettingsService
extends Node

## Player-facing content scale for the 2D interface. This uses Godot's Window
## content scaling rather than manually scaling the HUD root, and deliberately
## does not alter renderer resolution or any 3D quality setting.

signal ui_scale_changed(scale: float)

const DEFAULT_SCALE := 1.0
const SETTINGS_PATH := "user://mandate_ui_settings.cfg"
const SETTINGS_SECTION := "interface"
const SETTINGS_KEY := "ui_scale"
const SUPPORTED_SCALES := [0.75, 0.90, 1.0, 1.10, 1.25, 1.50, 1.75, 2.0]

var _ui_scale := DEFAULT_SCALE


func _ready() -> void:
	_ui_scale = _normalize_scale(_load_saved_scale())
	_apply_to_window()


func get_ui_scale() -> float:
	return _ui_scale


func set_ui_scale(scale: float) -> void:
	var normalized := _normalize_scale(scale)
	if is_equal_approx(_ui_scale, normalized):
		return
	_ui_scale = normalized
	_apply_to_window()
	_save()
	ui_scale_changed.emit(_ui_scale)


func get_supported_scales() -> PackedFloat32Array:
	return PackedFloat32Array(SUPPORTED_SCALES)


func _apply_to_window() -> void:
	var window := get_window()
	if window != null:
		window.content_scale_factor = _ui_scale


func _load_saved_scale() -> float:
	var config := ConfigFile.new()
	if config.load(SETTINGS_PATH) != OK:
		return DEFAULT_SCALE
	return float(config.get_value(SETTINGS_SECTION, SETTINGS_KEY, DEFAULT_SCALE))


func _save() -> void:
	var config := ConfigFile.new()
	config.set_value(SETTINGS_SECTION, SETTINGS_KEY, _ui_scale)
	if config.save(SETTINGS_PATH) != OK:
		push_warning("Unable to persist UI scale preference")


func _normalize_scale(scale: float) -> float:
	var closest := DEFAULT_SCALE
	var closest_distance := INF
	for supported in SUPPORTED_SCALES:
		var distance := absf(scale - supported)
		if distance < closest_distance:
			closest = supported
			closest_distance = distance
	return closest
