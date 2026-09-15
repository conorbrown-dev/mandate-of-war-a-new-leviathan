class_name MandateTooltip
extends PanelContainer

const Tokens = preload("res://ui/theme/ui_tokens.gd")
func set_content(title: String, detail: String) -> void:
	theme_type_variation = "InsetPanel"
	for child in get_children(): child.queue_free()
	var box := VBoxContainer.new(); box.add_theme_constant_override("separation", Tokens.SPACE_XS); add_child(box)
	var heading := Label.new(); heading.text = title; heading.theme_type_variation = "SectionHeading"; heading.add_theme_color_override("font_color", Tokens.TEXT_PRIMARY); box.add_child(heading)
	var body := Label.new(); body.text = detail; body.theme_type_variation = "MutedLabel"; body.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY); body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; box.add_child(body)
