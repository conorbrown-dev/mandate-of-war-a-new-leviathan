class_name MandateSectionHeader
extends VBoxContainer

const Tokens = preload("res://ui/theme/ui_tokens.gd")
@export var title := "SECTION"
@export var detail := ""
func _ready() -> void:
	add_theme_constant_override("separation", Tokens.SPACE_XS)
	var heading := Label.new(); heading.text = title; heading.theme_type_variation = "SectionHeading"; heading.add_theme_color_override("font_color", Tokens.TEXT_PRIMARY); add_child(heading)
	if not detail.is_empty():
		var description := Label.new(); description.text = detail; description.theme_type_variation = "MutedLabel"; description.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY); add_child(description)
