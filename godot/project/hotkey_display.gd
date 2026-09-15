extends Control

var hotkey_manager: Node = null

const INK := Color("#d9edf3")
const MUTED := Color("#7f9ba8")
const PANEL := Color("#07131d", 0.92)
const PANEL_EDGE := Color("#1f5266", 0.96)
const CYAN := Color("#47d9ff")
const HIGHLIGHT := Color("#ffbd52")

var ui_font: Font = null
var ui_font_small: Font = null


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_ready_setup()


func _ready_setup() -> void:
	ui_font = UiTypography.compact_tactical_font()
	if not is_instance_valid(hotkey_manager):
		hotkey_manager = preload("res://hotkey_manager.gd").new()
		add_child(hotkey_manager)


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		queue_redraw()


func refresh() -> void:
	queue_redraw()


func _draw() -> void:
	if not is_instance_valid(hotkey_manager):
		return

	var ui_scale := clampf(minf(size.x / 1920.0, size.y / 1080.0), 0.62, 1.0)
	draw_set_transform(Vector2.ZERO, 0.0, Vector2(ui_scale, ui_scale))

	var w := size.x / ui_scale
	var h := size.y / ui_scale

	var hotkey_list := hotkey_manager.get_all_hotkeys()
	if hotkey_list.is_empty():
		return

	var panel_rect := Rect2(w - 260.0 - 14.0, h - 320.0, 260.0, 306.0)

	draw_rect(panel_rect, PANEL, true)
	draw_rect(panel_rect, PANEL_EDGE, false, 1.0)

	var title_rect := Rect2(panel_rect.position + Vector2(10, 10), Vector2(panel_rect.size.x - 20, 30))
	_font_draw(title_rect.position + Vector2(0, 20), "HOTKEY ASSIGNMENTS", 10, CYAN, ui_scale)

	var entry_height := 24.0
	var start_y := title_rect.position.y + 40.0

	var visible_count := 12
	var current_y := start_y

	for index in range(mini(visible_count, hotkey_list.size())):
		var hotkey := hotkey_list[index]
		var entry_rect := Rect2(panel_rect.position + Vector2(10, current_y), Vector2(panel_rect.size.x - 20, entry_height))

		draw_rect(entry_rect, Color("#0b1c27", 0.96), true)
		draw_rect(entry_rect, PANEL_EDGE, false, 1.0)

		var key_text := hotkey.get("key", "UNKNOWN")
		var description := hotkey.get("description", "")

		_font_draw(entry_rect.position + Vector2(12, 16), key_text, 11, HIGHLIGHT, ui_scale)
		_font_draw(entry_rect.position + Vector2(120, 16), description, 9, INK, ui_scale)

		current_y += entry_height + 2

	var more_count := hotkey_list.size() - visible_count
	if more_count > 0:
		var more_rect := Rect2(panel_rect.position + Vector2(10, current_y + 4), Vector2(panel_rect.size.x - 20, 20))
		_font_draw(more_rect.position + Vector2(0, 14), "… and %d more" % more_count, 9, MUTED, ui_scale)


func _font_draw(at: Vector2, text: String, font_size: int, color: Color, ui_scale: float) -> void:
	if ui_font != null:
		draw_string(ui_font, at, text, HORIZONTAL_ALIGNMENT_LEFT, -1, floor(font_size * ui_scale), color)
	else:
		draw_string(ThemeDB.fallback_font, at, text, HORIZONTAL_ALIGNMENT_LEFT, -1, floor(font_size * ui_scale), color)
