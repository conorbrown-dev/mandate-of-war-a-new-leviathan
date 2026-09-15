class_name MandateTheme
extends Theme

const Tokens = preload("res://ui/theme/ui_tokens.gd")

func _init() -> void:
	default_font_size = 14
	_install_button_variations()
	_install_panel_variations()
	set_type_variation("PrimaryButton", "Button")
	set_type_variation("SecondaryButton", "Button")
	set_type_variation("GhostButton", "Button")
	set_type_variation("DangerButton", "Button")
	set_type_variation("CommandPanel", "PanelContainer")
	set_type_variation("RaisedPanel", "PanelContainer")
	set_type_variation("InsetPanel", "PanelContainer")
	set_type_variation("SectionHeading", "Label")
	set_type_variation("MutedLabel", "Label")
	set_type_variation("StatLabel", "Label")

func _box(fill: Color, border: Color, radius: int = Tokens.RADIUS_MD) -> StyleBoxFlat:
	var box := StyleBoxFlat.new()
	box.bg_color = fill
	box.border_color = border
	box.set_border_width_all(1)
	box.set_corner_radius_all(radius)
	box.content_margin_left = Tokens.SPACE_MD
	box.content_margin_right = Tokens.SPACE_MD
	box.content_margin_top = Tokens.SPACE_SM
	box.content_margin_bottom = Tokens.SPACE_SM
	return box

func _install_button_variations() -> void:
	_install_button("PrimaryButton", Tokens.ACCENT.darkened(0.72), Tokens.ACCENT, Tokens.SURFACE_HOVER)
	_install_button("SecondaryButton", Tokens.SURFACE_RAISED, Tokens.BORDER_DEFAULT, Tokens.SURFACE_HOVER)
	_install_button("GhostButton", Color(0, 0, 0, 0), Tokens.BORDER_SUBTLE, Tokens.SURFACE_HOVER)
	_install_button("DangerButton", Tokens.DANGER.darkened(0.78), Tokens.DANGER, Tokens.DANGER.darkened(0.60))

func _install_button(variation: String, fill: Color, border: Color, hover_fill: Color) -> void:
	var normal := _box(fill, border)
	var hover := _box(hover_fill, Tokens.BORDER_FOCUS)
	var pressed := _box(fill.darkened(0.22), border)
	var disabled := _box(fill, Tokens.BORDER_SUBTLE)
	disabled.bg_color.a = Tokens.DISABLED_ALPHA
	set_stylebox("normal", variation, normal)
	set_stylebox("hover", variation, hover)
	set_stylebox("pressed", variation, pressed)
	set_stylebox("focus", variation, hover)
	set_stylebox("disabled", variation, disabled)
	set_color("font_color", variation, Tokens.TEXT_PRIMARY)
	set_color("font_hover_color", variation, Tokens.TEXT_PRIMARY)
	set_color("font_pressed_color", variation, Tokens.TEXT_PRIMARY)
	set_color("font_disabled_color", variation, Tokens.TEXT_DISABLED)

func _install_panel_variations() -> void:
	set_stylebox("panel", "CommandPanel", _box(Tokens.SURFACE_PANEL, Tokens.BORDER_DEFAULT))
	set_stylebox("panel", "RaisedPanel", _box(Tokens.SURFACE_RAISED, Tokens.BORDER_DEFAULT, Tokens.RADIUS_LG))
	set_stylebox("panel", "InsetPanel", _box(Tokens.SURFACE_INSET, Tokens.BORDER_SUBTLE))
	set_font_size("font_size", "SectionHeading", 16)
	set_color("font_color", "SectionHeading", Tokens.TEXT_PRIMARY)
	set_font_size("font_size", "MutedLabel", 12)
	set_color("font_color", "MutedLabel", Tokens.TEXT_MUTED)
	set_font_size("font_size", "StatLabel", 16)
	set_color("font_color", "StatLabel", Tokens.ACCENT)
