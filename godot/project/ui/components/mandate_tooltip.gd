class_name MandateTooltip
extends PanelContainer
## An inline help note: an accent rail, a short title and wrapped body text.

const Tokens = preload("res://ui/theme/ui_tokens.gd")
func set_content(title: String, detail: String) -> void:
	theme_type_variation = "InsetPanel"
	var style := StyleBoxFlat.new()
	style.bg_color = Tokens.SURFACE_INSET
	style.border_color = Tokens.ACCENT
	style.border_width_left = Tokens.RAIL_WIDTH
	style.content_margin_left = Tokens.SPACE_MD
	style.content_margin_right = Tokens.SPACE_MD
	style.content_margin_top = Tokens.SPACE_SM
	style.content_margin_bottom = Tokens.SPACE_SM
	add_theme_stylebox_override("panel", style)
	for child in get_children(): child.queue_free()
	var box := VBoxContainer.new(); box.add_theme_constant_override("separation", Tokens.SPACE_XS); add_child(box)
	var heading := Label.new(); heading.text = title; heading.theme_type_variation = "EyebrowLabel"; box.add_child(heading)
	var body := Label.new(); body.text = detail; body.theme_type_variation = "CaptionLabel"; body.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY); body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART; box.add_child(body)
