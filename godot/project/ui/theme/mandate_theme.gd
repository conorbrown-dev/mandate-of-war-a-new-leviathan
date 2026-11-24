class_name MandateTheme
extends Theme
## The project-wide Theme. Every Control under a themed root gets these styles
## by default, so most UI needs no local overrides. Variations:
##   Buttons:  PrimaryButton, SecondaryButton, GhostButton, DangerButton,
##             TabButton, TabSelected
##   Panels:   CommandPanel, RaisedPanel, InsetPanel, ModalPanel, GlassPanel,
##             KeycapPanel, BadgePanel
##   Labels:   DisplayHeading, MajorHeading, SectionHeading, EyebrowLabel,
##             BodyLabel, MutedLabel, CaptionLabel, StatLabel, CommandLabel,
##             StrategicWarning, KeycapLabel

const Tokens = preload("res://ui/theme/ui_tokens.gd")
const Fonts = preload("res://ui/theme/mandate_fonts.gd")


func _init() -> void:
	default_font = Fonts.body()
	default_font_size = Tokens.FONT_MD
	for variation in ["PrimaryButton", "SecondaryButton", "GhostButton", "DangerButton", "TabButton", "TabSelected"]:
		set_type_variation(variation, "Button")
	for variation in ["CommandPanel", "RaisedPanel", "InsetPanel", "ModalPanel", "GlassPanel", "KeycapPanel", "BadgePanel"]:
		set_type_variation(variation, "PanelContainer")
	for variation in ["DisplayHeading", "MajorHeading", "SectionHeading", "EyebrowLabel", "BodyLabel", "MutedLabel", "CaptionLabel", "StatLabel", "CommandLabel", "StrategicWarning", "KeycapLabel"]:
		set_type_variation(variation, "Label")
	_install_defaults()
	_install_button_variations()
	_install_panel_variations()
	_install_label_variations()


static func box(fill: Color, border: Color, border_width: int = Tokens.BORDER_WIDTH, margin_x: int = Tokens.SPACE_MD, margin_y: int = Tokens.SPACE_SM) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = fill
	style.border_color = border
	style.set_border_width_all(border_width)
	style.set_corner_radius_all(Tokens.RADIUS_MD)
	style.content_margin_left = margin_x
	style.content_margin_right = margin_x
	style.content_margin_top = margin_y
	style.content_margin_bottom = margin_y
	style.anti_aliasing = false
	return style


func _install_defaults() -> void:
	set_color("font_color", "Label", Tokens.TEXT_PRIMARY)
	set_font("font", "Label", Fonts.body())
	set_stylebox("panel", "PanelContainer", box(Tokens.SURFACE_PANEL, Tokens.BORDER_DEFAULT))
	set_stylebox("panel", "Panel", box(Tokens.SURFACE_PANEL, Tokens.BORDER_DEFAULT))
	# Plain Buttons, OptionButtons and menu buttons look like secondary actions.
	for type in ["Button", "OptionButton", "MenuButton"]:
		_install_button(type, Tokens.SURFACE_RAISED, Tokens.BORDER_DEFAULT, Tokens.SURFACE_HOVER, Tokens.BORDER_STRONG, Tokens.TEXT_PRIMARY)
	var popup := box(Tokens.SURFACE_RAISED, Tokens.BORDER_STRONG, 1, Tokens.SPACE_XS, Tokens.SPACE_XS)
	set_stylebox("panel", "PopupMenu", popup)
	set_stylebox("hover", "PopupMenu", box(Tokens.ACCENT_FILL, Tokens.ACCENT, 0))
	set_color("font_color", "PopupMenu", Tokens.TEXT_PRIMARY)
	set_color("font_hover_color", "PopupMenu", Tokens.ACCENT_STRONG)
	set_font("font", "PopupMenu", Fonts.body())
	# Native tooltips match the command panels.
	var tooltip := box(Tokens.SURFACE_INSET, Tokens.ACCENT, 1, Tokens.SPACE_MD, Tokens.SPACE_SM)
	tooltip.border_width_left = Tokens.RAIL_WIDTH
	set_stylebox("panel", "TooltipPanel", tooltip)
	set_color("font_color", "TooltipLabel", Tokens.TEXT_PRIMARY)
	set_font("font", "TooltipLabel", Fonts.body())
	set_font_size("font_size", "TooltipLabel", Tokens.FONT_SM)
	var line_edit := box(Tokens.SURFACE_INSET, Tokens.BORDER_DEFAULT)
	set_stylebox("normal", "LineEdit", line_edit)
	set_stylebox("focus", "LineEdit", _focus_ring())
	set_color("font_color", "LineEdit", Tokens.TEXT_PRIMARY)
	set_color("caret_color", "LineEdit", Tokens.ACCENT)
	set_color("selection_color", "LineEdit", Color(Tokens.ACCENT, 0.3))
	var track := box(Tokens.SURFACE_INSET, Tokens.BORDER_SUBTLE, 1, 0, 0)
	var fill := box(Tokens.ACCENT, Tokens.ACCENT, 0, 0, 0)
	set_stylebox("background", "ProgressBar", track)
	set_stylebox("fill", "ProgressBar", fill)
	set_color("font_color", "ProgressBar", Tokens.TEXT_PRIMARY)
	var separator := StyleBoxLine.new()
	separator.color = Tokens.BORDER_SUBTLE
	separator.thickness = 1
	set_stylebox("separator", "HSeparator", separator)
	set_constant("separation", "HSeparator", Tokens.SPACE_SM)
	var vertical := StyleBoxLine.new()
	vertical.color = Tokens.BORDER_SUBTLE
	vertical.thickness = 1
	vertical.vertical = true
	set_stylebox("separator", "VSeparator", vertical)
	var grabber := box(Tokens.BORDER_STRONG, Tokens.BORDER_STRONG, 0, 0, 0)
	var grabber_hot := box(Tokens.ACCENT, Tokens.ACCENT, 0, 0, 0)
	var scroll := box(Color(0, 0, 0, 0), Color(0, 0, 0, 0), 0, 3, 3)
	for bar in ["VScrollBar", "HScrollBar"]:
		set_stylebox("scroll", bar, scroll)
		set_stylebox("grabber", bar, grabber)
		set_stylebox("grabber_highlight", bar, grabber_hot)
		set_stylebox("grabber_pressed", bar, grabber_hot)
	for check in ["CheckBox", "CheckButton"]:
		set_color("font_color", check, Tokens.TEXT_PRIMARY)
		set_color("font_hover_color", check, Tokens.ACCENT_STRONG)
		set_font("font", check, Fonts.body())


func _focus_ring() -> StyleBoxFlat:
	var ring := box(Color(0, 0, 0, 0), Tokens.BORDER_FOCUS, 1)
	ring.draw_center = false
	ring.set_expand_margin_all(2)
	return ring


func _install_button_variations() -> void:
	_install_button("PrimaryButton", Tokens.ACCENT, Tokens.ACCENT, Tokens.ACCENT_STRONG, Tokens.ACCENT_STRONG, Tokens.TEXT_ON_ACCENT)
	_install_button("SecondaryButton", Tokens.SURFACE_RAISED, Tokens.BORDER_DEFAULT, Tokens.SURFACE_HOVER, Tokens.ACCENT, Tokens.TEXT_PRIMARY)
	_install_button("GhostButton", Color(0, 0, 0, 0), Color(0, 0, 0, 0), Tokens.SURFACE_HOVER, Tokens.BORDER_DEFAULT, Tokens.TEXT_SECONDARY)
	_install_button("DangerButton", Tokens.DANGER.darkened(0.8), Tokens.DANGER.darkened(0.25), Tokens.DANGER.darkened(0.65), Tokens.DANGER, Tokens.TEXT_PRIMARY)
	# Tabs: an underline marks the selected tab, as in a segmented control.
	_install_button("TabButton", Color(0, 0, 0, 0), Color(0, 0, 0, 0), Tokens.SURFACE_HOVER, Color(0, 0, 0, 0), Tokens.TEXT_MUTED)
	_install_button("TabSelected", Tokens.ACCENT_DIM, Color(0, 0, 0, 0), Tokens.ACCENT_DIM, Color(0, 0, 0, 0), Tokens.ACCENT_STRONG)
	for variation in ["TabButton", "TabSelected"]:
		for state in ["normal", "hover", "pressed"]:
			var style := (get_stylebox(state, variation) as StyleBoxFlat).duplicate() as StyleBoxFlat
			style.set_border_width_all(0)
			style.border_width_bottom = Tokens.RAIL_WIDTH
			style.border_color = Tokens.ACCENT if variation == "TabSelected" else (Tokens.BORDER_STRONG if state == "hover" else Tokens.BORDER_SUBTLE)
			style.content_margin_left = Tokens.SPACE_LG
			style.content_margin_right = Tokens.SPACE_LG
			set_stylebox(state, variation, style)


func _install_button(variation: String, fill: Color, border: Color, hover_fill: Color, hover_border: Color, text: Color) -> void:
	var normal := box(fill, border)
	var hover := box(hover_fill, hover_border)
	var pressed := box(fill.darkened(0.25) if fill.a > 0.0 else Tokens.SURFACE_SELECTED, hover_border)
	var disabled := box(Color(Tokens.SURFACE_RAISED, 0.35), Tokens.BORDER_SUBTLE)
	set_stylebox("normal", variation, normal)
	set_stylebox("hover", variation, hover)
	set_stylebox("pressed", variation, pressed)
	set_stylebox("hover_pressed", variation, hover)
	set_stylebox("focus", variation, _focus_ring())
	set_stylebox("disabled", variation, disabled)
	set_color("font_color", variation, text)
	set_color("font_hover_color", variation, text if fill.a > 0.5 and fill.v > 0.5 else Tokens.TEXT_PRIMARY)
	set_color("font_pressed_color", variation, text)
	set_color("font_hover_pressed_color", variation, text)
	set_color("font_focus_color", variation, text)
	set_color("font_disabled_color", variation, Tokens.TEXT_DISABLED)
	set_color("icon_normal_color", variation, text)
	set_color("icon_hover_color", variation, text if fill.a > 0.5 and fill.v > 0.5 else Tokens.ACCENT_STRONG)
	set_color("icon_disabled_color", variation, Tokens.TEXT_DISABLED)
	set_font("font", variation, Fonts.label())
	set_font_size("font_size", variation, Tokens.FONT_SM)
	set_constant("h_separation", variation, Tokens.SPACE_SM)


func _install_panel_variations() -> void:
	set_stylebox("panel", "CommandPanel", box(Color(Tokens.SURFACE_PANEL, 0.96), Tokens.BORDER_DEFAULT))
	set_stylebox("panel", "RaisedPanel", box(Tokens.SURFACE_RAISED, Tokens.BORDER_DEFAULT))
	set_stylebox("panel", "InsetPanel", box(Tokens.SURFACE_INSET, Tokens.BORDER_SUBTLE))
	set_stylebox("panel", "GlassPanel", box(Tokens.SURFACE_GLASS, Tokens.BORDER_DEFAULT))
	var modal := box(Tokens.SURFACE_PANEL, Tokens.BORDER_STRONG, 1, 0, 0)
	modal.shadow_color = Color(0, 0, 0, 0.55)
	modal.shadow_size = 28
	set_stylebox("panel", "ModalPanel", modal)
	var keycap := box(Tokens.SURFACE_RAISED, Tokens.BORDER_STRONG, 1, 6, 1)
	keycap.border_width_bottom = 2
	set_stylebox("panel", "KeycapPanel", keycap)
	set_stylebox("panel", "BadgePanel", box(Tokens.ACCENT_DIM, Color(0, 0, 0, 0), 0, 6, 2))


func _install_label_variations() -> void:
	_label("DisplayHeading", Fonts.display(), Tokens.FONT_DISPLAY, Tokens.TEXT_PRIMARY)
	_label("MajorHeading", Fonts.display(), Tokens.FONT_2XL, Tokens.TEXT_PRIMARY)
	_label("SectionHeading", Fonts.display(), Tokens.FONT_LG, Tokens.TEXT_PRIMARY)
	_label("EyebrowLabel", Fonts.label(), Tokens.FONT_XS, Tokens.ACCENT)
	_label("BodyLabel", Fonts.body(), Tokens.FONT_MD, Tokens.TEXT_SECONDARY)
	_label("MutedLabel", Fonts.label(), Tokens.FONT_XS, Tokens.TEXT_MUTED)
	_label("CaptionLabel", Fonts.body(), Tokens.FONT_SM, Tokens.TEXT_MUTED)
	_label("StatLabel", Fonts.display(), Tokens.FONT_LG, Tokens.ACCENT)
	_label("CommandLabel", Fonts.display(), Tokens.FONT_MD, Tokens.TEXT_PRIMARY)
	_label("StrategicWarning", Fonts.display(), 30, Tokens.WARNING)
	_label("KeycapLabel", Fonts.display(), Tokens.FONT_SM, Tokens.TEXT_PRIMARY)
	set_constant("line_spacing", "BodyLabel", 4)


func _label(variation: String, font: Font, size: int, color: Color) -> void:
	set_font("font", variation, font)
	set_font_size("font_size", variation, size)
	set_color("font_color", variation, color)
