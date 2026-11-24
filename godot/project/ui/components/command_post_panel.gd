extends MandatePanel
## The selected command post, docked bottom-left where a unit selection shows.
## Name, role and integrity, the forces and fuel in the air toward it, and the
## two things the post can do: open Strategic Command (G) and request a fuel
## shipment (J). Buttons only emit; Main issues the native requests. There is
## no move or attack here, because the post is a fixed installation.

signal open_strategic_command
signal request_fuel

const ButtonComponent := preload("res://ui/components/command_button.gd")
const Keycap := preload("res://ui/components/keycap.gd")
const Tokens := preload("res://ui/theme/ui_tokens.gd")
const WIDTH := 340.0
const LEFT := 16.0
const BOTTOM_CLEARANCE := 34.0 + 16.0
const MAX_INBOUND_ROWS := 4

var state: Dictionary = {}
var _title: Label
var _role: Label
var _integrity_value: Label
var _integrity_bar: ProgressBar
var _inbound: VBoxContainer
var _status: Label
var strategic_button: Button
var fuel_button: Button


func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	surface = "glass"
	rail_color = Tokens.SUCCESS
	set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	grow_vertical = Control.GROW_DIRECTION_BEGIN
	mouse_filter = Control.MOUSE_FILTER_STOP
	super._ready()
	var margin := MarginContainer.new()
	for side in ["margin_left", "margin_right", "margin_top", "margin_bottom"]:
		margin.add_theme_constant_override(side, Tokens.SPACE_MD)
	add_child(margin)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", Tokens.SPACE_XS)
	margin.add_child(column)
	var header := HBoxContainer.new()
	column.add_child(header)
	var eyebrow := Label.new()
	eyebrow.text = "COMMAND POST"
	eyebrow.theme_type_variation = "EyebrowLabel"
	eyebrow.add_theme_color_override("font_color", Tokens.SUCCESS)
	eyebrow.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(eyebrow)
	var fixed := Label.new()
	fixed.text = "FIXED INSTALLATION"
	fixed.theme_type_variation = "MutedLabel"
	header.add_child(fixed)
	_title = Label.new()
	_title.name = "PostName"
	_title.theme_type_variation = "SectionHeading"
	_title.add_theme_font_size_override("font_size", Tokens.FONT_XL)
	column.add_child(_title)
	_role = Label.new()
	_role.name = "PostRole"
	_role.theme_type_variation = "CaptionLabel"
	_role.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(_role)
	var integrity := HBoxContainer.new()
	column.add_child(integrity)
	var integrity_caption := Label.new()
	integrity_caption.text = "INTEGRITY"
	integrity_caption.theme_type_variation = "MutedLabel"
	integrity_caption.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	integrity.add_child(integrity_caption)
	_integrity_value = Label.new()
	_integrity_value.name = "PostIntegrity"
	_integrity_value.theme_type_variation = "CommandLabel"
	integrity.add_child(_integrity_value)
	_integrity_bar = ProgressBar.new()
	_integrity_bar.show_percentage = false
	_integrity_bar.custom_minimum_size = Vector2(0, 6)
	_integrity_bar.max_value = 1.0
	column.add_child(_integrity_bar)
	column.add_child(HSeparator.new())
	var inbound_caption := Label.new()
	inbound_caption.text = "INBOUND TO THEATRE"
	inbound_caption.theme_type_variation = "EyebrowLabel"
	column.add_child(inbound_caption)
	_inbound = VBoxContainer.new()
	_inbound.name = "Inbound"
	_inbound.add_theme_constant_override("separation", 0)
	column.add_child(_inbound)
	_status = Label.new()
	_status.name = "PostStatus"
	_status.theme_type_variation = "CaptionLabel"
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(_status)
	column.add_child(HSeparator.new())
	var actions := VBoxContainer.new()
	actions.add_theme_constant_override("separation", Tokens.SPACE_XS)
	column.add_child(actions)
	strategic_button = _action(actions, "StrategicCommand", "STRATEGIC COMMAND", "G", &"logistics.airlift", "primary")
	strategic_button.pressed.connect(open_strategic_command.emit)
	fuel_button = _action(actions, "RequestFuel", "REQUEST FUEL SHIPMENT", "J", &"resource.fuel", "secondary")
	fuel_button.pressed.connect(request_fuel.emit)
	custom_minimum_size.x = WIDTH
	_apply_layout()


func _action(parent: Control, name: String, label: String, key: String, icon: StringName, variant: String) -> Button:
	var button := ButtonComponent.new()
	button.name = name
	button.text = label
	button.icon_id = icon
	button.variant = variant
	button.hotkey_text = key
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.focus_mode = Control.FOCUS_NONE
	parent.add_child(button)
	button.add_child(_keycap(key))
	return button


func _keycap(key: String) -> Control:
	var cap: Control = Keycap.make(key)
	cap.set_anchors_preset(Control.PRESET_CENTER_RIGHT)
	cap.offset_left = -32
	cap.offset_right = -8
	cap.offset_top = -11
	cap.offset_bottom = 11
	return cap


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED or what == NOTIFICATION_VISIBILITY_CHANGED:
		_apply_layout.call_deferred()


func _apply_layout() -> void:
	offset_left = LEFT
	offset_right = LEFT + WIDTH
	offset_bottom = -BOTTOM_CLEARANCE
	offset_top = offset_bottom - get_combined_minimum_size().y


## state: Main._command_post_snapshot() for the player's post.
func set_state(next_state: Dictionary) -> void:
	state = next_state
	if not is_instance_valid(_title):
		return
	_title.text = String(state.get("name", "COMMAND POST"))
	_role.text = String(state.get("role", ""))
	var health := float(state.get("health", -1.0))
	var max_health := float(state.get("max_health", -1.0))
	var ratio := clampf(health / max_health, 0.0, 1.0) if max_health > 0.0 else 0.0
	var tone := Tokens.SUCCESS if ratio > 0.6 else Tokens.WARNING if ratio > 0.3 else Tokens.DANGER
	_integrity_value.text = "%.0f / %.0f" % [health, max_health] if max_health > 0.0 else "--"
	_integrity_value.add_theme_color_override("font_color", tone)
	_integrity_bar.value = ratio
	_integrity_bar.modulate = tone
	var rows: Array = state.get("inbound", [])
	var lines: PackedStringArray = []
	for index in range(mini(rows.size(), MAX_INBOUND_ROWS)):
		var row: Dictionary = rows[index]
		lines.append("%s|%.0f S|%s" % [String(row.get("name", "UNIT")), float(row.get("seconds", 0.0)), str(bool(row.get("fuel", false)))])
	if rows.size() > MAX_INBOUND_ROWS:
		lines.append("+%d MORE|||false" % (rows.size() - MAX_INBOUND_ROWS))
	_sync_inbound(lines)
	_status.text = String(state.get("status", "")).to_upper()
	_status.visible = not _status.text.is_empty()
	var fuel_inbound := bool(state.get("fuel_inbound", false))
	fuel_button.disabled = fuel_inbound
	fuel_button.tooltip_text = "A FUEL SHIPMENT IS ALREADY INBOUND" if fuel_inbound else "SHIP IN %.0f FUEL FOR %.0f MATERIALS  [J]" % [float(state.get("fuel_load", 0.0)), float(state.get("fuel_cost", 0.0))]
	strategic_button.tooltip_text = "ORDER FORCES, RECON AND PARADROPS  [G]"
	_apply_layout()


func _sync_inbound(lines: PackedStringArray) -> void:
	if lines.is_empty():
		lines = PackedStringArray(["NONE IN FLIGHT||false"])
	while _inbound.get_child_count() > lines.size():
		var extra := _inbound.get_child(_inbound.get_child_count() - 1)
		_inbound.remove_child(extra)
		extra.queue_free()
	while _inbound.get_child_count() < lines.size():
		var row := HBoxContainer.new()
		var name_label := Label.new()
		name_label.theme_type_variation = "CommandLabel"
		name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		name_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		row.add_child(name_label)
		var time_label := Label.new()
		time_label.theme_type_variation = "CommandLabel"
		row.add_child(time_label)
		_inbound.add_child(row)
	for index in range(lines.size()):
		var parts := lines[index].split("|")
		var row := _inbound.get_child(index)
		var name_label := row.get_child(0) as Label
		var time_label := row.get_child(1) as Label
		name_label.text = parts[0]
		time_label.text = parts[1] if parts.size() > 1 else ""
		var is_fuel := parts.size() > 2 and parts[parts.size() - 1] == "true"
		var empty := parts.size() > 1 and parts[1].is_empty()
		var color := Tokens.TEXT_MUTED if empty else Tokens.RESOURCE_FUEL if is_fuel else Tokens.ACCENT_STRONG
		name_label.add_theme_color_override("font_color", color)
		time_label.add_theme_color_override("font_color", color)


## Plain text of what the panel shows, for tests and accessibility.
func describe() -> String:
	var parts: PackedStringArray = [_title.text, _role.text, "INTEGRITY " + _integrity_value.text]
	for row in _inbound.get_children():
		parts.append("%s %s" % [(row.get_child(0) as Label).text, (row.get_child(1) as Label).text])
	if _status.visible:
		parts.append(_status.text)
	return "  //  ".join(parts)
