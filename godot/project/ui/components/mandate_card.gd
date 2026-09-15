class_name MandateCard
extends PanelContainer
const Tokens = preload("res://ui/theme/ui_tokens.gd")
const Typography = preload("res://ui/theme/mandate_typography.gd")
@export var title := "T1 INTERCEPTOR"
@export var subtitle := "Air Superiority Fighter"
@export var selected := false
@export var disabled := false
func _ready() -> void:
	theme_type_variation = "RaisedPanel" if selected else "CommandPanel"
	var box := VBoxContainer.new(); box.add_theme_constant_override("separation", Tokens.SPACE_XS); add_child(box)
	var heading := Label.new(); heading.text = title; Typography.apply_role(heading, "section"); heading.add_theme_color_override("font_color", Tokens.TEXT_PRIMARY); box.add_child(heading)
	var detail := Label.new(); detail.text = subtitle; detail.theme_type_variation = "MutedLabel"; detail.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY); box.add_child(detail)
	modulate = Color(1, 1, 1, Tokens.DISABLED_ALPHA) if disabled else Color.WHITE
