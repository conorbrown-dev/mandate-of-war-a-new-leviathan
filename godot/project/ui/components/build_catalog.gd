class_name MandateBuildCatalog
extends MandatePanel

## The Field Engineer's build catalog, docked bottom-centre above the inspect
## strip like an RTS command card. Buttons only request a blueprint; Main still
## validates and issues the authoritative build command.
signal build_requested(build_type: int)

const CommandButton := preload("res://ui/components/command_button.gd")
const Keycap := preload("res://ui/components/keycap.gd")
const Tokens := preload("res://ui/theme/ui_tokens.gd")
const UiIconRegistry := preload("res://ui/icons/ui_icon_registry.gd")
const BuildDescriptions := preload("res://ui/build_descriptions.gd")
const DESCRIPTION_HINT := "HOVER A STRUCTURE FOR WHAT IT DOES"
const MAX_WIDTH := 540.0
const VIEWPORT_MARGIN := 16.0
const BOTTOM_CLEARANCE := 34.0 + 16.0
const STRUCTURE_HOTKEYS := ["6", "7", "T", "L"]

var _fingerprint := ""
var _entries: Array[Dictionary] = []
## Detail line under the buttons: the hovered structure's description.
var _description: Label

func _ready() -> void:
	surface = "glass"
	rail_color = Tokens.WARNING
	set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	grow_vertical = Control.GROW_DIRECTION_BEGIN
	grow_horizontal = Control.GROW_DIRECTION_BOTH
	mouse_filter = Control.MOUSE_FILTER_STOP
	super._ready()
	_apply_viewport_layout()

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		_apply_viewport_layout()

func _apply_viewport_layout() -> void:
	var viewport_width := get_viewport_rect().size.x
	var panel_width := minf(MAX_WIDTH, maxf(320.0, viewport_width - VIEWPORT_MARGIN * 2.0))
	offset_left = -panel_width * 0.5
	offset_right = panel_width * 0.5
	# Height follows the content; the bottom edge sits above the inspect strip.
	offset_bottom = -BOTTOM_CLEARANCE
	offset_top = offset_bottom - get_combined_minimum_size().y

func refresh_catalog(catalog: Array, can_build: bool) -> void:
	var next_fingerprint := JSON.stringify({"can_build": can_build, "catalog": catalog})
	if next_fingerprint == _fingerprint:
		return
	_fingerprint = next_fingerprint
	_entries.clear()
	_description = null
	for child in get_children():
		remove_child(child)
		child.queue_free()
	visible = can_build and not catalog.is_empty()
	if not visible:
		return
	var margin := MarginContainer.new()
	for side in ["margin_left", "margin_right"]:
		margin.add_theme_constant_override(side, Tokens.SPACE_XS)
	margin.add_theme_constant_override("margin_top", Tokens.SPACE_XS)
	margin.add_theme_constant_override("margin_bottom", Tokens.SPACE_XS)
	add_child(margin)
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", Tokens.SPACE_SM)
	margin.add_child(content)
	var header := HBoxContainer.new()
	content.add_child(header)
	var title := Label.new()
	title.text = "FIELD ENGINEER  //  BUILD"
	title.theme_type_variation = "EyebrowLabel"
	title.add_theme_color_override("font_color", Tokens.WARNING)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	var instruction := Label.new()
	instruction.text = "CLICK OR PRESS A KEY, THEN PLACE  //  SHIFT QUEUES"
	instruction.theme_type_variation = "MutedLabel"
	header.add_child(instruction)
	# The engineer builds structures and earthworks only: forces are ordered from
	# Strategic Command (G) and flown in.
	_add_section(content, catalog.filter(func(entry): return bool(entry.get("is_structure", false))), STRUCTURE_HOTKEYS)
	_description = Label.new()
	_description.name = "Description"
	_description.theme_type_variation = "CaptionLabel"
	_description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	# Room for three lines, so hovering never resizes the panel.
	_description.custom_minimum_size = Vector2(0.0, 48.0)
	_description.mouse_filter = Control.MOUSE_FILTER_IGNORE
	content.add_child(_description)
	show_description({})
	_apply_viewport_layout.call_deferred()

## The description line shows `entry`, or the hint when entry is empty.
func show_description(entry: Dictionary) -> void:
	if _description == null:
		return
	var text := BuildDescriptions.for_entry(entry) if not entry.is_empty() else ""
	_description.text = text if not text.is_empty() else DESCRIPTION_HINT
	_description.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY if not text.is_empty() else Tokens.TEXT_MUTED)


func description_text() -> String:
	return _description.text if _description != null else ""


func get_hover_context(pointer: Vector2) -> Dictionary:
	for entry in _entries:
		var button: Button = entry.get("button")
		if button != null and is_instance_valid(button) and button.get_global_rect().has_point(pointer):
			return {
				"key": "build:%s:%s" % [entry.get("type", ""), entry.get("is_structure", false)],
				"title": "BUILD // %s" % String(entry.get("name", "UNIT")).to_upper(),
				"detail": "MATERIALS %.0f  //  %.0f SEC  //  %s" % [float(entry.get("material", 0.0)), float(entry.get("build_seconds", 0.0)), "READY" if bool(entry.get("available", false)) else "LOCKED"],
				"accent": Tokens.WARNING,
				"description": BuildDescriptions.for_entry(entry),
			}
	return {}

func _add_section(content: VBoxContainer, entries: Array, hotkeys: Array) -> void:
	if entries.is_empty():
		return
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", Tokens.SPACE_SM)
	grid.add_theme_constant_override("v_separation", Tokens.SPACE_SM)
	content.add_child(grid)
	for index in range(entries.size()):
		var entry: Dictionary = entries[index]
		var available := bool(entry.get("available", false))
		var button := CommandButton.new()
		button.variant = "secondary"
		button.hotkey_text = String(hotkeys[index]) if index < hotkeys.size() else ""
		button.icon_id = StringName(String(entry.get("icon_id", "construction.build")))
		button.text = String(entry.get("name", "UNIT")).to_upper()
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.disabled = not available
		button.pressed.connect(func(): build_requested.emit(int(entry.get("type", -1))))
		grid.add_child(button)
		# MandateCommandButton establishes its compact default in _ready; apply
		# the catalog's readable size after the node has entered the tree.
		button.custom_minimum_size = Vector2(246.0, 52.0)
		button.add_theme_constant_override("icon_max_width", 22)
		var description := BuildDescriptions.for_entry(entry)
		button.tooltip_text = "%s\n%.0f Materials  ·  %.0f s to build%s%s" % [String(entry.get("name", "UNIT")), float(entry.get("material", 0.0)), float(entry.get("build_seconds", 0.0)), "" if button.hotkey_text.is_empty() else "\nHotkey: %s" % button.hotkey_text, "" if description.is_empty() else "\n\n" + description]
		button.mouse_entered.connect(show_description.bind(entry))
		button.mouse_exited.connect(show_description.bind({}))
		_add_resource_costs(button, float(entry.get("material", 0.0)), available)
		var record := entry.duplicate(true)
		record["button"] = button
		_entries.append(record)

func _add_resource_costs(button: Button, mass: float, available: bool) -> void:
	var costs := HBoxContainer.new()
	costs.name = "ResourceCosts"
	costs.mouse_filter = Control.MOUSE_FILTER_IGNORE
	costs.add_theme_constant_override("separation", Tokens.SPACE_SM)
	costs.alignment = BoxContainer.ALIGNMENT_END
	costs.set_anchors_preset(Control.PRESET_RIGHT_WIDE)
	costs.offset_left = -112.0
	costs.offset_right = -10.0
	_add_resource_cost(costs, &"resource.material", Tokens.RESOURCE_MASS if available else Tokens.TEXT_DISABLED, mass, "MassCost")
	if not button.hotkey_text.is_empty():
		var cap := Keycap.make(button.hotkey_text)
		cap.name = "Hotkey"
		costs.add_child(cap)
	button.add_child(costs)

func _add_resource_cost(parent: HBoxContainer, icon_id: StringName, color: Color, amount: float, node_name: String) -> void:
	var group := HBoxContainer.new()
	group.name = node_name
	group.mouse_filter = Control.MOUSE_FILTER_IGNORE
	group.add_theme_constant_override("separation", Tokens.SPACE_XS)
	var icon := TextureRect.new()
	icon.texture = UiIconRegistry.get_icon(icon_id)
	icon.custom_minimum_size = Vector2(14.0, 14.0)
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	icon.modulate = color
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	group.add_child(icon)
	var value := Label.new()
	value.text = "%.0f" % amount
	value.add_theme_color_override("font_color", color if color != Tokens.RESOURCE_MASS else Tokens.TEXT_PRIMARY)
	value.theme_type_variation = "CommandLabel"
	value.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	value.mouse_filter = Control.MOUSE_FILTER_IGNORE
	group.add_child(value)
	parent.add_child(group)
