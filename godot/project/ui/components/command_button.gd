class_name MandateCommandButton
extends MandateButton

## A compact, reusable command control for future node-based HUD surfaces.
## The current tactical HUD stays custom-drawn; this component keeps semantic
## icon sizing and hotkey presentation out of future command-panel call sites.

@export var hotkey_text := "":
	set(value):
		hotkey_text = value
		tooltip_text = "%s  [%s]" % [text, hotkey_text] if not hotkey_text.is_empty() else text


func _ready() -> void:
	super._ready()
	custom_minimum_size = Vector2(96.0, 44.0)
	variant = "ghost" if variant == "primary" else variant
	if not hotkey_text.is_empty():
		tooltip_text = "%s  [%s]" % [text, hotkey_text]
