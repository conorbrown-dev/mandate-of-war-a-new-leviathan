extends Control
## The F1 hotkey sheet: every control, grouped, in a centred command panel.
## Built once from hotkey_manager.gd when first shown; presentation only.

const Tokens := preload("res://ui/theme/ui_tokens.gd")
const MandatePanelScript := preload("res://ui/components/mandate_panel.gd")
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
	backdrop.color = Color(Tokens.SURFACE_BASE, 0.55)
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(backdrop)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(center)

	var panel: PanelContainer = MandatePanelScript.new()
	panel.surface = "command"
	panel.mouse_filter = Control.MOUSE_FILTER_STOP
	center.add_child(panel)

	var margin := MarginContainer.new()
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, Tokens.SPACE_XL)
	panel.add_child(margin)

	var layout := VBoxContainer.new()
	layout.add_theme_constant_override("separation", Tokens.SPACE_LG)
	margin.add_child(layout)

	var title_row := HBoxContainer.new()
	layout.add_child(title_row)
	var title := Label.new()
	title.text = "HOTKEYS"
	title.add_theme_font_size_override("font_size", 20)
	title.add_theme_color_override("font_color", Tokens.TEXT_PRIMARY)
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title_row.add_child(title)
	var hint := Label.new()
	hint.text = "F1 OR ESC TO CLOSE"
	hint.add_theme_font_size_override("font_size", 12)
	hint.add_theme_color_override("font_color", Tokens.TEXT_MUTED)
	title_row.add_child(hint)

	var columns := HBoxContainer.new()
	columns.add_theme_constant_override("separation", Tokens.SPACE_2XL)
	layout.add_child(columns)
	var column_boxes: Array[VBoxContainer] = []
	for _index in range(COLUMNS):
		var column := VBoxContainer.new()
		column.add_theme_constant_override("separation", Tokens.SPACE_LG)
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


func _section(category: String, hotkeys: Array) -> Control:
	var box := VBoxContainer.new()
	box.add_theme_constant_override("separation", Tokens.SPACE_XS)
	var heading := Label.new()
	heading.text = category.to_upper()
	heading.add_theme_font_size_override("font_size", 12)
	heading.add_theme_color_override("font_color", Tokens.ACCENT)
	box.add_child(heading)
	var grid := GridContainer.new()
	grid.columns = 2
	grid.add_theme_constant_override("h_separation", Tokens.SPACE_MD)
	grid.add_theme_constant_override("v_separation", Tokens.SPACE_XS)
	box.add_child(grid)
	for hotkey in hotkeys:
		var key := Label.new()
		key.text = String(hotkey.get("label", String(hotkey.get("key", "")).trim_prefix("KEY_")))
		key.add_theme_font_size_override("font_size", 14)
		key.add_theme_color_override("font_color", Tokens.TEXT_PRIMARY)
		key.custom_minimum_size.x = 132
		grid.add_child(key)
		var description := Label.new()
		description.text = String(hotkey.get("description", ""))
		description.add_theme_font_size_override("font_size", 14)
		description.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY)
		grid.add_child(description)
	return box
