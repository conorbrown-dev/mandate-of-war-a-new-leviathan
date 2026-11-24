class_name MandateTypography
extends RefCounted
## Type roles for code that cannot use a Theme variation. Prefer the Label
## variations in mandate_theme.gd (DisplayHeading, SectionHeading, BodyLabel...).

const Tokens = preload("res://ui/theme/ui_tokens.gd")
const Fonts = preload("res://ui/theme/mandate_fonts.gd")

const ROLE_SIZES := {
	"display": Tokens.FONT_2XL,
	"heading": Tokens.FONT_XL,
	"section": Tokens.FONT_LG,
	"body": Tokens.FONT_MD,
	"label": Tokens.FONT_XS,
	"stat": Tokens.FONT_LG,
}

static func apply_role(control: Control, role: String) -> void:
	control.add_theme_font_size_override("font_size", ROLE_SIZES.get(role, ROLE_SIZES.body))
	match role:
		"display", "heading", "section", "stat":
			control.add_theme_font_override("font", Fonts.display())
		"label":
			control.add_theme_font_override("font", Fonts.label())
			control.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY)
		_:
			control.add_theme_font_override("font", Fonts.body())
