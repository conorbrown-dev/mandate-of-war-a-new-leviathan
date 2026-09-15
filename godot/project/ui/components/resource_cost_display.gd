class_name ResourceCostDisplay
extends HBoxContainer
const Tokens = preload("res://ui/theme/ui_tokens.gd")
@export var material_cost := 420:
	set(value):
		material_cost = value
		_refresh()
@export var energy_cost := 180:
	set(value):
		energy_cost = value
		_refresh()
@export var insufficient := false:
	set(value):
		insufficient = value
		_refresh()
var _material: Label
var _energy: Label
func _ready() -> void:
	add_theme_constant_override("separation", Tokens.SPACE_MD)
	_material = Label.new(); _energy = Label.new(); add_child(_material); add_child(_energy); _refresh()
func _refresh() -> void:
	if not is_instance_valid(_material): return
	var color := Tokens.DANGER if insufficient else Tokens.TEXT_PRIMARY
	_material.text = "M %d" % material_cost; _energy.text = "E %d" % energy_cost
	_material.add_theme_color_override("font_color", color); _energy.add_theme_color_override("font_color", color)
