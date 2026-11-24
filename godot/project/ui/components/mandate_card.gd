class_name MandateCard
extends PanelContainer
## An information card: optional icon, title, subtitle and a body column that
## callers fill (get_child(0) is that column, kept for older call sites; use
## body() in new code). Disabled cards keep legible text and dim only the art.

const Tokens = preload("res://ui/theme/ui_tokens.gd")
const PanelChrome = preload("res://ui/components/mandate_panel.gd")

@export var title := "T1 INTERCEPTOR"
@export var subtitle := "Air Superiority Fighter"
@export var selected := false
@export var disabled := false
@export var icon: Texture2D
@export var icon_tint := Color.WHITE
@export var accent := Tokens.ACCENT

var _body: VBoxContainer


func _ready() -> void:
	theme_type_variation = "RaisedPanel" if selected else "CommandPanel"
	add_theme_stylebox_override("panel", _style())
	_body = VBoxContainer.new()
	_body.name = "Body"
	_body.add_theme_constant_override("separation", Tokens.SPACE_SM)
	_body.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_body)
	var head := HBoxContainer.new()
	head.add_theme_constant_override("separation", Tokens.SPACE_MD)
	head.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_body.add_child(head)
	if icon != null:
		var art := TextureRect.new()
		art.name = "Icon"
		art.texture = icon
		art.custom_minimum_size = Vector2(36, 36)
		art.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		art.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		art.modulate = Color(icon_tint, 0.45) if disabled else icon_tint
		art.mouse_filter = Control.MOUSE_FILTER_IGNORE
		head.add_child(art)
	var titles := VBoxContainer.new()
	titles.add_theme_constant_override("separation", 2)
	titles.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	titles.mouse_filter = Control.MOUSE_FILTER_IGNORE
	head.add_child(titles)
	var heading := Label.new()
	heading.name = "Title"
	heading.text = title
	heading.theme_type_variation = "SectionHeading"
	heading.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY if disabled else Tokens.TEXT_PRIMARY)
	heading.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	titles.add_child(heading)
	var detail := Label.new()
	detail.name = "Subtitle"
	detail.text = subtitle
	detail.theme_type_variation = "MutedLabel"
	titles.add_child(detail)


func body() -> VBoxContainer:
	return _body


func _style() -> StyleBoxFlat:
	var style := (get_theme_stylebox("panel", theme_type_variation) as StyleBoxFlat)
	style = style.duplicate() if style != null else StyleBoxFlat.new()
	style.content_margin_left = Tokens.SPACE_LG
	style.content_margin_right = Tokens.SPACE_LG
	style.content_margin_top = Tokens.SPACE_MD + Tokens.RAIL_WIDTH
	style.content_margin_bottom = Tokens.SPACE_MD
	if selected:
		style.border_color = Tokens.ACCENT
	return style


func _draw() -> void:
	var rail := Color(accent, 0.35) if disabled else accent
	PanelChrome.draw_panel_chrome(self, Rect2(Vector2.ZERO, size), rail, false)
