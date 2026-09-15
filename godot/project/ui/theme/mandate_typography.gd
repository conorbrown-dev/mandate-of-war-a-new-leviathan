class_name MandateTypography
extends RefCounted

const ROLE_SIZES := {
	"display": 28,
	"heading": 20,
	"section": 16,
	"body": 14,
	"label": 12,
	"stat": 16,
}

static func apply_role(control: Control, role: String) -> void:
	control.add_theme_font_size_override("font_size", ROLE_SIZES.get(role, ROLE_SIZES.body))
	if role == "label":
		control.add_theme_color_override("font_color", preload("res://ui/theme/ui_tokens.gd").TEXT_SECONDARY)
