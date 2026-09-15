class_name MandateButton
extends Button

@export_enum("primary", "secondary", "ghost", "danger") var variant := "primary":
	set(value):
		variant = value
		_apply_style()

func _ready() -> void:
	custom_minimum_size.y = 34
	_apply_style()

func _apply_style() -> void:
	theme_type_variation = {
		"primary": "PrimaryButton",
		"secondary": "SecondaryButton",
		"ghost": "GhostButton",
		"danger": "DangerButton",
	}.get(variant, "PrimaryButton")
