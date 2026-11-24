extends Control
## The F1 hotkey sheet: every control, grouped, in a centred command panel.
## Built once from hotkey_manager.gd when first shown; presentation only.

const Tokens := preload("res://ui/theme/ui_tokens.gd")
const MandatePanelScript := preload("res://ui/components/mandate_panel.gd")
const KeycapScript := preload("res://ui/components/keycap.gd")
const COLUMNS := 3

var hotkey_manager: Node = null

var _built := false


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	visible = false


func toggle() -> void:
	set_sheet_visible(not visible)


func set_sheet_visible(shown: bool) -> void:
	if shown and not _built:
		_build()
	visible = shown


func refresh() -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_built = false
	if visible:
		_build()


func _build() -> void:
	if not is_instance_valid(hotkey_manager):
		hotkey_manager = preload("res://hotkey_manager.gd").new()
		add_child(hotkey_manager)
	_built = true
	var backdrop := ColorRect.new()
	backdrop.color = Tokens.SURFACE_SCRIM
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(backdrop)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(center)

	var panel: PanelContainer = MandatePanelScript.new()
	panel.surface = "modal"
	panel.rail_color = Tokens.ACCENT
	panel.mouse_filter = Control.MOUSE_FILTER_STOP
	center.add_child(panel)

	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", 0)
	panel.add_child(layout)

	var header := _margin(Tokens.SPACE_XL, Tokens.SPACE_LG)
	layout.add_child(header)
	var title_row := HBoxContainer.new()
	title_row.add_theme_constant_override("separation", Tokens.SPACE_SM)
	header.add_child(title_row)
	var titles := VBoxContainer.new()
	titles.add_theme_constant_override("separation", 2)
	titles.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title_row.add_child(titles)
	var eyebrow := Label.new()
	eyebrow.text = "FIELD MANUAL"
	eyebrow.theme_type_variation = "EyebrowLabel"
	titles.add_child(eyebrow)
	var title := Label.new()
	title.text = "CONTROLS"
	title.theme_type_variation = "MajorHeading"
	titles.add_child(title)
	for key in ["F1", "ESC"]:
		var cap := KeycapScript.make(key)
		title_row.add_child(cap)
	var hint := Label.new()
	hint.text = "CLOSE"
	hint.theme_type_variation = "MutedLabel"
	hint.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	title_row.add_child(hint)
	layout.add_child(HSeparator.new())

	var body := _margin(Tokens.SPACE_XL, Tokens.SPACE_LG)
	layout.add_child(body)
	var columns := HBoxContainer.new()
	columns.add_theme_constant_override("separation", Tokens.SPACE_2XL + Tokens.SPACE_SM)
	body.add_child(columns)
	var column_boxes: Array[VBoxContainer] = []
	for _index in range(COLUMNS):
		var column := VBoxContainer.new()
		column.add_theme_constant_override("separation", Tokens.SPACE_XL)
		column.alignment = BoxContainer.ALIGNMENT_BEGIN
		columns.add_child(column)
		column_boxes.append(column)

	# Fill the shortest column with each section in turn, so the sheet stays even.
	var heights: Array[int] = []
	heights.resize(COLUMNS)
	heights.fill(0)
	for section in hotkey_manager.get_hotkey_sections():
		var target := heights.find(heights.min())
		column_boxes[target].add_child(_section(String(section.category), section.hotkeys))
		heights[target] += section.hotkeys.size() + 2


func _margin(horizontal: int, vertical: int) -> MarginContainer:
	var margin := MarginContainer.new()
	for side in ["left", "right"]:
		margin.add_theme_constant_override("margin_" + side, horizontal)
	for side in ["top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, vertical)
	return margin


## The key column: each key or mouse button as its own cap, joined by + / or.
func _keys(label: String) -> Control:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", Tokens.SPACE_XS)
	row.custom_minimum_size.x = 176
	var alternatives := label.split(" / ")
	for alt_index in range(alternatives.size()):
		if alt_index > 0:
			row.add_child(_joiner("/"))
		var parts := alternatives[alt_index].split(" + ")
		for part_index in range(parts.size()):
			if part_index > 0:
				row.add_child(_joiner("+"))
			row.add_child(KeycapScript.make(parts[part_index].strip_edges().to_upper()))
	return row


func _joiner(text: String) -> Label:
	var joiner := Label.new()
	joiner.text = text
	joiner.theme_type_variation = "MutedLabel"
	joiner.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	return joiner


func _section(category: String, hotkeys: Array) -> Control:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", Tokens.SPACE_SM)
	var heading := Label.new()
	heading.text = category.to_upper()
	heading.theme_type_variation = "EyebrowLabel"
	box.add_child(heading)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", Tokens.SPACE_MD)
	grid.add_theme_constant_override("v_separation", Tokens.SPACE_SM)
	box.add_child(grid)
	for hotkey in hotkeys:
		grid.add_child(_keys(String(hotkey.get("label", String(hotkey.get("key", "")).trim_prefix("KEY_")))))
		var description := Label.new()
		description.text = String(hotkey.get("description", ""))
		description.add_theme_font_size_override("font_size", Tokens.FONT_SM + 1)
		description.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY)
		description.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		description.custom_minimum_size.x = 300
		grid.add_child(description)
	return box
