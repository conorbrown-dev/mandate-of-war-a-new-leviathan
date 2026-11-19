class_name ExitConfirm
extends Control
## Exit confirmation for a running match or setup screen.

signal exit_requested

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const PanelComponent = preload("res://ui/components/mandate_panel.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")

var _stay: Button

func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	mouse_filter = Control.MOUSE_FILTER_STOP
	var backdrop := ColorRect.new()
	backdrop.color = Tokens.SURFACE_SCRIM
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(backdrop)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(center)
	var panel := PanelComponent.new()
	panel.surface = "modal"
	panel.rail_color = Tokens.WARNING
	panel.custom_minimum_size = Vector2(420, 0)
	center.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, Tokens.SPACE_XL)
	panel.add_child(margin)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", Tokens.SPACE_MD)
	margin.add_child(column)
	var heading := Label.new()
	heading.text = "EXIT MANDATE OF WAR?"
	heading.theme_type_variation = "SectionHeading"
	column.add_child(heading)
	var detail := Label.new()
	detail.text = "Your current match will end."
	detail.theme_type_variation = "BodyLabel"
	column.add_child(detail)
	var actions := HBoxContainer.new()
	actions.alignment = BoxContainer.ALIGNMENT_END
	actions.add_theme_constant_override("separation", Tokens.SPACE_SM)
	column.add_child(actions)
	var exit_button := ButtonComponent.new()
	exit_button.name = "Exit"
	exit_button.text = "EXIT TO DESKTOP"
	exit_button.variant = "danger"
	exit_button.pressed.connect(exit_requested.emit)
	actions.add_child(exit_button)
	_stay = ButtonComponent.new()
	_stay.name = "Stay"
	_stay.text = "STAY IN MATCH"
	_stay.variant = "primary"
	_stay.pressed.connect(hide)
	actions.add_child(_stay)
	visibility_changed.connect(_on_visibility_changed)
	_on_visibility_changed()

func _on_visibility_changed() -> void:
	if visible and _stay != null:
		_stay.grab_focus.call_deferred()
