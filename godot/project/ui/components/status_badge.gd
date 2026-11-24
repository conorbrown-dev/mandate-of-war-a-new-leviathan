class_name StatusBadge
extends Label
## A small status pill: a coloured dot and an uppercase state word.
const Tokens = preload("res://ui/theme/ui_tokens.gd")
const Fonts = preload("res://ui/theme/mandate_fonts.gd")
const STATE_COLORS := {"available": Tokens.SUCCESS, "locked": Tokens.TEXT_MUTED, "warning": Tokens.WARNING, "danger": Tokens.DANGER, "deploying": Tokens.ACCENT}
@export_enum("available", "locked", "warning", "danger", "deploying") var state := "available":
	set(value):
		state = value
		_apply_state()
func _ready() -> void:
	_apply_state()
func _apply_state() -> void:
	var color: Color = STATE_COLORS.get(state, Tokens.TEXT_MUTED)
	add_theme_color_override("font_color", color)
	add_theme_font_override("font", Fonts.label())
	add_theme_font_size_override("font_size", Tokens.FONT_XS)
	autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	var style := StyleBoxFlat.new()
	style.bg_color = Color(color, 0.12)
	style.border_color = Color(color, 0.9)
	style.border_width_left = 2
	style.content_margin_left = 8
	style.content_margin_right = 8
	style.content_margin_top = 3
	style.content_margin_bottom = 3
	add_theme_stylebox_override("normal", style)
