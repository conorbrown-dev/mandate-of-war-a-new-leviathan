extends Control
## Radio text: the line being spoken, and a log of recent ones (F3). The text
## is always the full report, so it works with speech muted or missing.
## Clicking a line with a known location asks the view to look there.

const PanelComponent := preload("res://ui/components/mandate_panel.gd")
const Tokens := preload("res://ui/theme/ui_tokens.gd")
const LOG_ROWS := 8
const WIDTH := 600.0
const TOP := 104.0
const SEVERITY_COLORS := {
	"critical": Tokens.DANGER, "urgent": Tokens.WARNING,
	"operational": Tokens.ACCENT, "routine": Tokens.TEXT_SECONDARY,
}

signal focus_requested(position: Vector2)

var announcer: Node
var subtitle_panel: PanelContainer
var subtitle_button: Button
var caption_label: Label
var text_label: Label
var detail_label: Label
var log_panel: PanelContainer
var log_rows: VBoxContainer
var _shown: Dictionary = {}


func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var column := VBoxContainer.new()
	column.name = "RadioColumn"
	column.anchor_left = 0.5
	column.anchor_right = 0.5
	column.offset_left = -WIDTH * 0.5
	column.offset_right = WIDTH * 0.5
	column.offset_top = TOP
	column.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_theme_constant_override("separation", Tokens.SPACE_SM)
	add_child(column)

	subtitle_panel = PanelComponent.new()
	subtitle_panel.name = "RadioSubtitle"
	subtitle_panel.surface = "glass"
	subtitle_panel.visible = false
	subtitle_panel.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_child(subtitle_panel)
	var margin := _margin(subtitle_panel)
	var lines := VBoxContainer.new()
	lines.mouse_filter = Control.MOUSE_FILTER_IGNORE
	lines.add_theme_constant_override("separation", 2)
	margin.add_child(lines)
	caption_label = _label(lines, "EyebrowLabel")
	text_label = _label(lines, "CommandLabel")
	text_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	detail_label = _label(lines, "CaptionLabel")
	detail_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	# A flat button over the strip, so the whole line is one click target.
	subtitle_button = Button.new()
	subtitle_button.name = "FocusButton"
	subtitle_button.flat = true
	subtitle_button.focus_mode = Control.FOCUS_NONE
	subtitle_button.pressed.connect(func() -> void: _request_focus(_shown))
	subtitle_panel.add_child(subtitle_button)

	log_panel = PanelComponent.new()
	log_panel.name = "RadioLog"
	log_panel.surface = "glass"
	log_panel.rail_color = Tokens.ACCENT
	log_panel.visible = false
	column.add_child(log_panel)
	var log_margin := _margin(log_panel)
	var log_column := VBoxContainer.new()
	log_column.add_theme_constant_override("separation", Tokens.SPACE_XS)
	log_margin.add_child(log_column)
	var heading := _label(log_column, "EyebrowLabel")
	heading.text = "RADIO LOG  //  F3 CLOSES  //  CLICK A REPORT TO VIEW ITS LOCATION"
	log_rows = VBoxContainer.new()
	log_rows.name = "Rows"
	log_rows.add_theme_constant_override("separation", 2)
	log_column.add_child(log_rows)


func _margin(parent: Control) -> MarginContainer:
	var margin := MarginContainer.new()
	margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for side in ["left", "right"]:
		margin.add_theme_constant_override("margin_" + side, Tokens.SPACE_LG)
	for side in ["top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, Tokens.SPACE_SM)
	parent.add_child(margin)
	return margin


func _label(parent: Control, variation: String) -> Label:
	var label := Label.new()
	label.theme_type_variation = variation
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label


func attach(radio: Node) -> void:
	announcer = radio
	announcer.announcement_started.connect(_on_started)
	announcer.announcement_finished.connect(_on_finished)
	announcer.history_changed.connect(_rebuild_log)
	announcer.prefs.changed.connect(_apply_prefs)


func _on_started(entry: Dictionary) -> void:
	_shown = entry
	var color: Color = SEVERITY_COLORS.get(entry.severity, Tokens.ACCENT)
	subtitle_panel.rail_color = color
	caption_label.text = "%s  //  %s%s" % [entry.category_name, String(entry.severity).to_upper(), "" if entry.voiced else "  //  TEXT ONLY"]
	caption_label.add_theme_color_override("font_color", color)
	text_label.text = entry.text
	detail_label.text = entry.detail
	detail_label.visible = not String(entry.detail).is_empty()
	subtitle_button.disabled = not _can_focus(entry)
	subtitle_button.tooltip_text = "View this location" if _can_focus(entry) else ""
	subtitle_button.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND if _can_focus(entry) else Control.CURSOR_ARROW
	_apply_prefs()


func _on_finished(entry: Dictionary) -> void:
	if _shown.get("sequence", -1) == entry.get("sequence", -2):
		_shown = {}
		subtitle_panel.visible = false


func _apply_prefs() -> void:
	subtitle_panel.visible = not _shown.is_empty() and announcer != null and announcer.prefs.subtitles_enabled


func toggle_log() -> void:
	log_panel.visible = not log_panel.visible
	if log_panel.visible:
		_rebuild_log()


func _rebuild_log() -> void:
	if log_panel == null or not log_panel.visible or announcer == null:
		return
	for row in log_rows.get_children():
		log_rows.remove_child(row)
		row.queue_free()
	var history: Array = announcer.history
	if history.is_empty():
		_label(log_rows, "CaptionLabel").text = "No reports yet."
		return
	for index in range(history.size() - 1, maxi(-1, history.size() - 1 - LOG_ROWS), -1):
		var entry: Dictionary = history[index]
		var row := Button.new()
		row.name = "Report%d" % int(entry.sequence)
		row.flat = true
		row.focus_mode = Control.FOCUS_NONE
		row.alignment = HORIZONTAL_ALIGNMENT_LEFT
		row.clip_text = true
		row.text = "%s  %s%s" % [_clock_text(int(entry.tick)), entry.text, ("   [%s]" % entry.grid) if not String(entry.grid).is_empty() else ""]
		row.add_theme_color_override("font_color", SEVERITY_COLORS.get(entry.severity, Tokens.ACCENT))
		row.disabled = not _can_focus(entry)
		row.tooltip_text = String(entry.detail)
		row.pressed.connect(func() -> void: _request_focus(entry))
		log_rows.add_child(row)


func _clock_text(tick: int) -> String:
	var seconds := tick / 20
	return "%02d:%02d" % [seconds / 60, seconds % 60]


func _can_focus(entry: Dictionary) -> bool:
	return bool(entry.get("focus", false)) and entry.has("position")


func _request_focus(entry: Dictionary) -> void:
	if _can_focus(entry):
		focus_requested.emit(entry.position)
