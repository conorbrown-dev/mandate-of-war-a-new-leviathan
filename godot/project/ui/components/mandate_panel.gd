class_name MandatePanel
extends PanelContainer

@export_enum("command", "raised", "inset") var surface := "command":
	set(value):
		surface = value
		_apply_surface()

func _ready() -> void:
	_apply_surface()

func _apply_surface() -> void:
	theme_type_variation = {"command": "CommandPanel", "raised": "RaisedPanel", "inset": "InsetPanel"}.get(surface, "CommandPanel")
