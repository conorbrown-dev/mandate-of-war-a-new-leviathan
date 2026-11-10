class_name MatchEndPanel
extends Control
## The after-action report: the result in large type, why it ended, the match
## facts, and what to do next. Play Again is the focused default (Enter).

signal play_again_requested
signal quit_requested

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const PanelComponent = preload("res://ui/components/mandate_panel.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")

var _panel: PanelContainer
var _title: Label
var _subtitle: Label
var _detail: Label
var _again: Button


func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	var backdrop := ColorRect.new()
	backdrop.color = Tokens.SURFACE_SCRIM
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(backdrop)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(center)
	_panel = PanelComponent.new()
	_panel.surface = "modal"
	_panel.custom_minimum_size = Vector2(560, 0)
	center.add_child(_panel)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 0)
	_panel.add_child(column)
	var head := _margin(Tokens.SPACE_2XL, Tokens.SPACE_2XL, Tokens.SPACE_XL)
	column.add_child(head)
	var head_box := VBoxContainer.new()
	head_box.add_theme_constant_override("separation", Tokens.SPACE_XS)
	head.add_child(head_box)
	var eyebrow := Label.new()
	eyebrow.text = "AFTER-ACTION REPORT"
	eyebrow.theme_type_variation = "EyebrowLabel"
	head_box.add_child(eyebrow)
	_title = Label.new()
	_title.name = "MatchTitle"
	_title.theme_type_variation = "DisplayHeading"
	_title.add_theme_font_size_override("font_size", 64)
	head_box.add_child(_title)
	_subtitle = Label.new()
	_subtitle.name = "MatchSubtitle"
	_subtitle.theme_type_variation = "SectionHeading"
	_subtitle.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY)
	head_box.add_child(_subtitle)
	column.add_child(HSeparator.new())
	var body := _margin(Tokens.SPACE_2XL, Tokens.SPACE_LG, Tokens.SPACE_LG)
	column.add_child(body)
	_detail = Label.new()
	_detail.name = "MatchDetail"
	_detail.theme_type_variation = "CommandLabel"
	_detail.add_theme_color_override("font_color", Tokens.TEXT_PRIMARY)
	_detail.add_theme_constant_override("line_spacing", 6)
	body.add_child(_detail)
	column.add_child(HSeparator.new())
	var footer := _margin(Tokens.SPACE_2XL, Tokens.SPACE_LG, Tokens.SPACE_XL)
	column.add_child(footer)
	var actions := HBoxContainer.new()
	actions.add_theme_constant_override("separation", Tokens.SPACE_SM)
	actions.alignment = BoxContainer.ALIGNMENT_END
	footer.add_child(actions)
	var quit_button := ButtonComponent.new()
	quit_button.name = "Quit"
	quit_button.text = "QUIT TO DESKTOP"
	quit_button.variant = "ghost"
	quit_button.pressed.connect(quit_requested.emit)
	actions.add_child(quit_button)
	_again = ButtonComponent.new()
	_again.name = "PlayAgain"
	_again.text = "PLAY AGAIN"
	_again.variant = "primary"
	_again.custom_minimum_size = Vector2(200, 44)
	_again.pressed.connect(play_again_requested.emit)
	actions.add_child(_again)


func _margin(horizontal: int, top: int, bottom: int) -> MarginContainer:
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", horizontal)
	margin.add_theme_constant_override("margin_right", horizontal)
	margin.add_theme_constant_override("margin_top", top)
	margin.add_theme_constant_override("margin_bottom", bottom)
	return margin


func show_result(title: String, subtitle: String, detail: String, accent: Color) -> void:
	_title.text = title
	_title.add_theme_color_override("font_color", accent)
	_panel.set("rail_color", accent)
	_subtitle.text = subtitle
	_detail.text = detail
	visible = true
	_again.grab_focus.call_deferred()
