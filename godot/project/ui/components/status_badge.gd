class_name StatusBadge
extends Label
const Tokens = preload("res://ui/theme/ui_tokens.gd")
@export_enum("available", "locked", "warning", "danger", "deploying") var state := "available":
	set(value):
		state = value
		_apply_state()
func _ready() -> void: _apply_state()
func _apply_state() -> void:
	add_theme_color_override("font_color", {"available": Tokens.SUCCESS, "locked": Tokens.TEXT_MUTED, "warning": Tokens.WARNING, "danger": Tokens.DANGER, "deploying": Tokens.ACCENT}.get(state, Tokens.TEXT_MUTED))
	add_theme_font_size_override("font_size", 11)
