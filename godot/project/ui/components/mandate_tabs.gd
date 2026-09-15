class_name MandateTabs
extends HBoxContainer

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")
var _selected_index := 0
func _ready() -> void: add_theme_constant_override("separation", Tokens.SPACE_SM)
func set_tabs(labels: PackedStringArray, selected_index: int = 0) -> void:
	for child in get_children(): child.queue_free()
	_selected_index = clampi(selected_index, 0, maxi(labels.size() - 1, 0))
	for index in labels.size():
		var tab := ButtonComponent.new(); tab.text = labels[index]; tab.variant = "primary" if index == _selected_index else "ghost"; tab.pressed.connect(_select.bind(index)); add_child(tab)
func _select(index: int) -> void:
	_selected_index = index
	for child_index in get_child_count():
		var tab := get_child(child_index) as Button
		if tab != null: tab.set("variant", "primary" if child_index == _selected_index else "ghost")
