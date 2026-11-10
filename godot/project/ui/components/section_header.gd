class_name MandateSectionHeader
extends VBoxContainer
## Eyebrow + title pair: a small tracked caption above a display heading.

const Tokens = preload("res://ui/theme/ui_tokens.gd")
@export var title := "SECTION"
@export var detail := ""
@export var display := false
func _ready() -> void:
	add_theme_constant_override("separation", 2)
	if not detail.is_empty():
		var eyebrow := Label.new(); eyebrow.name = "Eyebrow"; eyebrow.text = detail; eyebrow.theme_type_variation = "EyebrowLabel"; add_child(eyebrow)
	var heading := Label.new(); heading.name = "Title"; heading.text = title; heading.theme_type_variation = "MajorHeading" if display else "SectionHeading"; add_child(heading)
