class_name MandateButton
extends Button
## Themed action. Variants: primary (the one main action), secondary, ghost
## (low-emphasis), danger (destructive only). Text reads as uppercase command
## text; pass a hotkey to show it in the tooltip.

const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")
const ButtonTokens = preload("res://ui/theme/ui_tokens.gd")
const UiAudio = preload("res://audio/ui_audio.gd")

@export_enum("primary", "secondary", "ghost", "danger") var variant := "primary":
	set(value):
		variant = value
		_apply_style()

@export var icon_id: StringName:
	set(value):
		icon_id = value
		_apply_semantic_icon()

func _ready() -> void:
	custom_minimum_size.y = maxf(custom_minimum_size.y, 36)
	mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
	focus_mode = Control.FOCUS_ALL
	_apply_style()
	_apply_semantic_icon()
	mouse_entered.connect(_on_hover_entered)


# One quiet tick each time the pointer enters; a disabled button stays silent.
func _on_hover_entered() -> void:
	if not disabled:
		UiAudio.hover(self)

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
		expand_icon = false
		add_theme_constant_override("icon_max_width", 18)
