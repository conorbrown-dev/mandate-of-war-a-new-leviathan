class_name MandateButton
extends Button

const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")

@export_enum("primary", "secondary", "ghost", "danger") var variant := "primary":
	set(value):
		variant = value
		_apply_style()

@export var icon_id: StringName:
	set(value):
		icon_id = value
		_apply_semantic_icon()

func _ready() -> void:
	custom_minimum_size.y = 34
	_apply_style()
	_apply_semantic_icon()

func _apply_style() -> void:
	theme_type_variation = {
		"primary": "PrimaryButton",
		"secondary": "SecondaryButton",
		"ghost": "GhostButton",
		"danger": "DangerButton",
	}.get(variant, "PrimaryButton")


func _apply_semantic_icon() -> void:
	if icon_id.is_empty():
		return
	var resolved := UiIconRegistry.get_icon(icon_id)
	if resolved != null:
		icon = resolved
