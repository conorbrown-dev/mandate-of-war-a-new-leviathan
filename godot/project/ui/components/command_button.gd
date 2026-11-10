class_name MandateCommandButton
extends MandateButton

## A compact command control with a semantic icon and an optional hotkey. The
## hotkey shows in the tooltip; callers can also show it as a MandateKeycap.

@export var hotkey_text := "":
	set(value):
		hotkey_text = value
		tooltip_text = "%s  [%s]" % [text, hotkey_text] if not hotkey_text.is_empty() else text


func _ready() -> void:
	super._ready()
	custom_minimum_size = Vector2(96.0, 44.0)
	variant = "ghost" if variant == "primary" else variant
	if not hotkey_text.is_empty() and tooltip_text.is_empty():
		tooltip_text = "%s  [%s]" % [text, hotkey_text]
