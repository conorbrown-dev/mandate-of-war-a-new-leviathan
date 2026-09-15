class_name MandateBuildCatalog
extends MandatePanel

## Readable presentation for the engineer catalog.  Buttons only request a
## blueprint; Main still validates and issues the authoritative build command.
signal build_requested(build_type: int)

const CommandButton := preload("res://ui/components/command_button.gd")
const Tokens := preload("res://ui/theme/ui_tokens.gd")
const UiIconRegistry := preload("res://ui/icons/ui_icon_registry.gd")
const MAX_WIDTH := 540.0
const VIEWPORT_MARGIN := 16.0

var _fingerprint := ""
var _entries: Array[Dictionary] = []

func _ready() -> void:
	surface = "raised"
	set_anchors_preset(Control.PRESET_CENTER_TOP)
	mouse_filter = Control.MOUSE_FILTER_PASS
	_apply_viewport_layout()

func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		_apply_viewport_layout()

func _apply_viewport_layout() -> void:
	var viewport_width := get_viewport_rect().size.x
	var panel_width := minf(MAX_WIDTH, maxf(320.0, viewport_width - VIEWPORT_MARGIN * 2.0))
	offset_left = -panel_width * 0.5
	offset_right = panel_width * 0.5
	offset_top = 12.0
	# Five compact rows at the worst case: three unit rows and two structure
	# rows. This stays tactical rather than becoming a screen-sized catalog.
	offset_bottom = 374.0

func refresh_catalog(catalog: Array, can_build: bool) -> void:
	var next_fingerprint := JSON.stringify({"can_build": can_build, "catalog": catalog})
	if next_fingerprint == _fingerprint:
		return
	_fingerprint = next_fingerprint
	_entries.clear()
	for child in get_children():
		child.queue_free()
	visible = can_build and not catalog.is_empty()
	if not visible:
		return
	var content := VBoxContainer.new()
	content.add_theme_constant_override("separation", Tokens.SPACE_SM)
	add_child(content)
	var title := Label.new()
	title.text = "FIELD ENGINEER // BUILD CATALOG"
	title.theme_type_variation = "SectionHeading"
	content.add_child(title)
	var instruction := Label.new()
	instruction.text = "SELECT A BLUEPRINT OR USE ITS HOTKEY"
	instruction.theme_type_variation = "MutedLabel"
	content.add_child(instruction)
	_add_section(content, "UNITS", catalog.filter(func(entry): return not bool(entry.get("is_structure", false)) and not bool(entry.get("is_aircraft", false))), ["1", "4", "5", "9", "0", "P"])
	_add_section(content, "STRUCTURES", catalog.filter(func(entry): return bool(entry.get("is_structure", false))), ["6", "7", "A", "L"])

func get_hover_context(pointer: Vector2) -> Dictionary:
	for entry in _entries:
		var button: Button = entry.get("button")
		if button != null and button.get_global_rect().has_point(pointer):
			return {
				"key": "build:%s:%s" % [entry.get("type", ""), entry.get("is_structure", false)],
				"title": "BUILD // %s" % String(entry.get("name", "UNIT")),
				"detail": "WRENCH %.0f  //  BOLT %.0f  //  %.0f SEC  //  %s" % [float(entry.get("material", 0.0)), float(entry.get("energy", 0.0)), float(entry.get("build_seconds", 0.0)), "READY" if bool(entry.get("available", false)) else "LOCKED"],
				"accent": Tokens.ACCENT,
			}
	return {}

func _add_section(content: VBoxContainer, label: String, entries: Array, hotkeys: Array) -> void:
	if entries.is_empty():
		return
	var heading := Label.new()
	heading.text = label
	heading.theme_type_variation = "MutedLabel"
	content.add_child(heading)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", Tokens.SPACE_SM)
	grid.add_theme_constant_override("v_separation", Tokens.SPACE_SM)
	content.add_child(grid)
	for index in range(entries.size()):
		var entry: Dictionary = entries[index]
		var button := CommandButton.new()
		button.variant = "secondary"
		button.expand_icon = true
		button.hotkey_text = String(hotkeys[index]) if index < hotkeys.size() else ""
		button.icon_id = StringName(String(entry.get("icon_id", "construction.build")))
		button.text = "%s  [%s]" % [String(entry.get("name", "UNIT")).to_upper(), button.hotkey_text]
		button.tooltip_text = "%s\n%.0f sec" % [String(entry.get("name", "UNIT")), float(entry.get("build_seconds", 0.0))]
		button.disabled = not bool(entry.get("available", false))
		button.pressed.connect(func(): build_requested.emit(int(entry.get("type", -1))))
		grid.add_child(button)
		# MandateCommandButton establishes its compact default in _ready; apply
		# the catalog's readable two-line size after the node has entered the tree.
		button.custom_minimum_size = Vector2(244.0, 52.0)
		_add_resource_costs(button, float(entry.get("material", 0.0)), float(entry.get("energy", 0.0)))
		var record := entry.duplicate(true)
		record["button"] = button
		_entries.append(record)

func _add_resource_costs(button: Button, mass: float, energy: float) -> void:
	var costs := HBoxContainer.new()
	costs.name = "ResourceCosts"
	costs.mouse_filter = Control.MOUSE_FILTER_IGNORE
	costs.add_theme_constant_override("separation", Tokens.SPACE_SM)
	costs.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	costs.offset_left = -124.0
	costs.offset_top = -23.0
	costs.offset_right = -8.0
	costs.offset_bottom = -5.0
	_add_resource_cost(costs, &"resource.material", Tokens.RESOURCE_MASS, mass, "MassCost")
	_add_resource_cost(costs, &"resource.energy", Tokens.RESOURCE_ENERGY, energy, "EnergyCost")
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
	icon.modulate = color
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	group.add_child(icon)
	var value := Label.new()
	value.text = "%.0f" % amount
	value.add_theme_color_override("font_color", color)
	value.theme_type_variation = "MutedLabel"
	value.mouse_filter = Control.MOUSE_FILTER_IGNORE
	group.add_child(value)
	parent.add_child(group)
