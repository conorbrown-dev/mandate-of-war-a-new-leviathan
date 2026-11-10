class_name ResourceCostDisplay
extends HBoxContainer
## A resource cost: the Materials icon and amount. Red when unaffordable.
const Tokens = preload("res://ui/theme/ui_tokens.gd")
const Fonts = preload("res://ui/theme/mandate_fonts.gd")
const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")
@export var material_cost := 420:
	set(value):
		material_cost = value
		_refresh()
@export var insufficient := false:
	set(value):
		insufficient = value
		_refresh()
var _material: Label
var _icon: TextureRect
func _ready() -> void:
	add_theme_constant_override("separation", Tokens.SPACE_XS)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_icon = TextureRect.new()
	_icon.texture = UiIconRegistry.get_icon(&"resource.material")
	_icon.custom_minimum_size = Vector2(16, 16)
	_icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_icon.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	add_child(_icon)
	_material = Label.new()
	_material.add_theme_font_override("font", Fonts.display())
	_material.add_theme_font_size_override("font_size", Tokens.FONT_LG)
	add_child(_material)
	_refresh()
func _refresh() -> void:
	if not is_instance_valid(_material): return
	var color := Tokens.DANGER if insufficient else Tokens.RESOURCE_MASS
	_material.text = "%d" % material_cost
	_material.add_theme_color_override("font_color", Tokens.DANGER if insufficient else Tokens.TEXT_PRIMARY)
	_icon.modulate = color
	tooltip_text = "Materials %d%s" % [material_cost, " (not enough)" if insufficient else ""]
