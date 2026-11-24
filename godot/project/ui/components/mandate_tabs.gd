class_name MandateTabs
extends HBoxContainer
## A segmented tab strip. The selected tab is filled and underlined in the
## command accent; the others are quiet until hovered.

signal tab_selected(index: int)

const Tokens = preload("res://ui/theme/ui_tokens.gd")
const UiAudio = preload("res://audio/ui_audio.gd")
var _selected_index := 0

func _ready() -> void:
	add_theme_constant_override("separation", 0)

func set_tabs(labels: PackedStringArray, selected_index: int = 0) -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_selected_index = clampi(selected_index, 0, maxi(labels.size() - 1, 0))
	for index in labels.size():
		var tab := Button.new()
		tab.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		tab.name = "Tab_%s" % labels[index]
		tab.text = labels[index]
		tab.custom_minimum_size = Vector2(112, 38)
		tab.pressed.connect(_select.bind(index))
		tab.mouse_entered.connect(_on_tab_hovered.bind(tab))
		add_child(tab)
	_restyle()

func _on_tab_hovered(tab: Button) -> void:
	if not tab.disabled:
		UiAudio.hover(tab)

func selected_index() -> int:
	return _selected_index

func _select(index: int) -> void:
	_selected_index = index
	_restyle()
	tab_selected.emit(index)

func _restyle() -> void:
	for child_index in get_child_count():
		var tab := get_child(child_index) as Button
		if tab != null:
			tab.theme_type_variation = "TabSelected" if child_index == _selected_index else "TabButton"
			tab.set_meta("selected", child_index == _selected_index)
