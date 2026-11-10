class_name MandateKeycap
extends PanelContainer
## A keyboard key, drawn as a small raised cap. Use for every hotkey hint so
## shortcuts read the same in the HUD, catalogs and the F1 sheet.

@export var key_text := "G":
	set(value):
		key_text = value
		if is_instance_valid(_label):
			_label.text = value

var _label: Label


static func make(text: String) -> PanelContainer:
	var cap: PanelContainer = load("res://ui/components/keycap.gd").new()
	cap.key_text = text
	return cap


func _ready() -> void:
	theme_type_variation = "KeycapPanel"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	size_flags_vertical = Control.SIZE_SHRINK_CENTER
	size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	_label = Label.new()
	_label.theme_type_variation = "KeycapLabel"
	_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_label.custom_minimum_size.x = 14
	_label.text = key_text
	add_child(_label)
