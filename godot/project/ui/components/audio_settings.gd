extends Control
## Reachable before, during and after a match. The match keeps running.
const Mix := preload("res://audio/audio_mix.gd")
const PanelComponent := preload("res://ui/components/mandate_panel.gd")
const Action := preload("res://ui/components/mandate_button.gd")
const Tokens := preload("res://ui/theme/ui_tokens.gd")
var mix := Mix.new()
var sliders: Dictionary = {}
var credits: RichTextLabel
var _previous_focus: WeakRef
var _status: Label

func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var backdrop := ColorRect.new()
	backdrop.color = Tokens.SURFACE_SCRIM
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(backdrop)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(center)
	var panel := PanelComponent.new()
	panel.surface = "modal"
	panel.rail_color = Tokens.ACCENT
	panel.custom_minimum_size.x = 680
	center.add_child(panel)
	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, Tokens.SPACE_XL)
	panel.add_child(margin)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", Tokens.SPACE_MD)
	margin.add_child(column)
	var title := Label.new()
	title.text = "AUDIO & CREDITS"
	title.theme_type_variation = "MajorHeading"
	column.add_child(title)
	var hint := Label.new()
	hint.text = "Volume changes apply immediately. Zero mutes a channel."
	hint.theme_type_variation = "BodyLabel"
	column.add_child(hint)
	for bus: String in Mix.BUSES:
		var row := HBoxContainer.new()
		row.add_theme_constant_override("separation", Tokens.SPACE_MD)
		column.add_child(row)
		var caption := Label.new()
		caption.text = bus.to_upper()
		caption.custom_minimum_size.x = 100
		caption.theme_type_variation = "CommandLabel"
		row.add_child(caption)
		var slider := HSlider.new()
		slider.name = bus + "Volume"
		slider.min_value = 0
		slider.max_value = 100
		slider.step = 1
		slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		slider.custom_minimum_size = Vector2(270, 28)
		slider.tooltip_text = "%s volume; left/right arrows adjust, Home mutes" % bus
		row.add_child(slider)
		sliders[bus] = slider
		var value := Label.new()
		value.custom_minimum_size.x = 52
		value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		value.theme_type_variation = "StatLabel"
		row.add_child(value)
		slider.value_changed.connect(func(percent: float) -> void:
			mix.set_level(bus, percent / 100.0)
			value.text = "%d%%" % int(percent)
			_status.text = "" if mix.save() == OK else "Could not save audio preferences.")
	column.add_child(HSeparator.new())
	credits = RichTextLabel.new()
	credits.name = "AudioCredits"
	credits.custom_minimum_size = Vector2(580, 220)
	credits.bbcode_enabled = true
	credits.selection_enabled = true
	credits.text = "[b]SOUND CREDITS[/b]\n\nEngine-loop heavy vehicle/tank by Nayckron, based on qubodup.\nhttps://opengameart.org/content/engine-loop-heavy-vehicletank\nLicensed under CC BY 3.0: https://creativecommons.org/licenses/by/3.0/\nSource unchanged; loop playback, gain and speed-based pitch in game.\n\nInterface, Impact and Sci-fi Sounds by Kenney — CC0.\nhttps://kenney.nl/assets\nBreaking / falling / hit SFX by rubberduck — CC0.\nhttps://opengameart.org/content/75-cc0-breaking-falling-hit-sfx\nFree Firearm Sound Library by Ben Jaszczak, Brian Nelson, Kevin Heras and Matthew Nanney — CC0.\nhttps://opengameart.org/node/21826\nAK-47 and SMG cues: trimmed, mono, downsampled, normalized and faded.\n\nAircraft, cannon and missile cues are stylized layers.\nFull sources, checksums and licenses accompany the game in audio/.\n\n[b]MODEL CREDITS[/b]\nWarehouse by Lavender Harmony — CC BY 3.0.\nhttps://poly.pizza/m/fsxbktzB1zo\nhttps://creativecommons.org/licenses/by/3.0/\nResidential Block and Office Tower by Kenney; Water Tower by Quaternius — CC0.\nSee docs/BUILDING_ASSETS.md for exact model pages and checksums."
	column.add_child(credits)
	_status = Label.new()
	_status.theme_type_variation = "CaptionLabel"
	column.add_child(_status)
	var footer := HBoxContainer.new()
	footer.alignment = BoxContainer.ALIGNMENT_END
	footer.add_theme_constant_override("separation", Tokens.SPACE_SM)
	column.add_child(footer)
	var reset := Action.new()
	reset.text = "RESTORE DEFAULTS"
	reset.variant = "ghost"
	reset.pressed.connect(func() -> void:
		for bus: String in Mix.BUSES:
			sliders[bus].value = float(Mix.DEFAULTS[bus]) * 100.0)
	footer.add_child(reset)
	var close := Action.new()
	close.name = "CloseAudio"
	close.text = "CLOSE  [F2 / ESC]"
	close.variant = "secondary"
	close.pressed.connect(dismiss)
	footer.add_child(close)
	visible = false

func toggle() -> void:
	if visible:
		dismiss()
	else:
		var focused := get_viewport().gui_get_focus_owner()
		_previous_focus = weakref(focused) if focused != null else null
		for bus: String in Mix.BUSES:
			sliders[bus].value = roundf(mix.level(bus) * 100.0)
		visible = true
		sliders.Master.grab_focus()

func dismiss() -> void:
	visible = false
	var focused: Control = _previous_focus.get_ref() if _previous_focus != null else null
	if is_instance_valid(focused) and focused.is_visible_in_tree():
		focused.grab_focus()
	else:
		get_viewport().gui_release_focus()

func _input(event: InputEvent) -> void:
	if visible and event is InputEventKey and event.pressed and not event.echo and not event.ctrl_pressed and not event.meta_pressed and not event.alt_pressed and event.keycode in [KEY_F2, KEY_ESCAPE]:
		dismiss()
		get_viewport().set_input_as_handled()
