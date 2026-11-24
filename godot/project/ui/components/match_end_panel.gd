class_name MatchEndPanel
extends Control

signal play_again_requested
signal quit_requested

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const PanelComponent = preload("res://ui/components/mandate_panel.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")

var _title: Label
var _subtitle: Label
var _detail: Label


func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	var backdrop := ColorRect.new()
	backdrop.color = Color(Tokens.SURFACE_BASE, 0.72)
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(backdrop)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(center)
	var panel := PanelComponent.new()
	panel.custom_minimum_size = Vector2(420, 0)
	center.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["margin_left", "margin_right", "margin_top", "margin_bottom"]:
		margin.add_theme_constant_override(side, Tokens.SPACE_LG)
	panel.add_child(margin)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", Tokens.SPACE_MD)
	margin.add_child(column)
	_title = Label.new()
	_title.name = "MatchTitle"
	_title.add_theme_font_size_override("font_size", 40)
	column.add_child(_title)
	_subtitle = Label.new()
	_subtitle.name = "MatchSubtitle"
	_subtitle.theme_type_variation = "MutedLabel"
	column.add_child(_subtitle)
	_detail = Label.new()
	_detail.name = "MatchDetail"
	_detail.theme_type_variation = "StatLabel"
	column.add_child(_detail)
	var actions := HBoxContainer.new()
	actions.add_theme_constant_override("separation", Tokens.SPACE_SM)
	column.add_child(actions)
	var again := ButtonComponent.new()
	again.name = "PlayAgain"
	again.text = "PLAY AGAIN"
	again.variant = "primary"
	again.pressed.connect(play_again_requested.emit)
	actions.add_child(again)
	var quit_button := ButtonComponent.new()
	quit_button.name = "Quit"
	quit_button.text = "QUIT"
	quit_button.variant = "secondary"
	quit_button.pressed.connect(quit_requested.emit)
	actions.add_child(quit_button)


func show_result(title: String, subtitle: String, detail: String, accent: Color) -> void:
	_title.text = title
	_title.add_theme_color_override("font_color", accent)
	_subtitle.text = subtitle
	_detail.text = detail
	visible = true
