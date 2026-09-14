extends Control

# A compact tactical deck inspired by the information placement of classic
# large-scale RTS interfaces: strategic information stays at the top, the
# selected force is anchored bottom-left, and commands remain bottom-center.
# This is an original presentation; every number shown comes from Main.

var snapshot: Dictionary = {}

const INK := Color("#d9edf3")
const MUTED := Color("#7f9ba8")
const PANEL := Color("#07131d", 0.90)
const PANEL_EDGE := Color("#1f5266", 0.96)
const CYAN := Color("#47d9ff")
const AMBER := Color("#ffbd52")
const RED := Color("#ff6d65")
const GREEN := Color("#7ad99b")
const BUILD_UNIT_SHORTCUTS := ["1", "4", "5", "9", "0", "P"]
const RESOURCE_ICON_KINDS := ["wrench", "bolt", "research"]
const UiTypographyScript = preload("res://ui_typography.gd")
const DRAWN_FONT_SIZE_BUMP := 0
var ui_font: Font


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_font = UiTypographyScript.compact_tactical_font()


func refresh(next_snapshot: Dictionary) -> void:
	snapshot = next_snapshot.duplicate(true)
	queue_redraw()


func get_build_hover_context() -> Dictionary:
	var ui_scale := clampf(minf(size.x / 1920.0, size.y / 1080.0), 0.62, 1.0)
	return get_build_hover_context_at(get_local_mouse_position() / ui_scale)


func get_build_hover_context_at(pointer: Vector2) -> Dictionary:
	if snapshot.is_empty() or not bool(snapshot.get("can_build", false)):
		return {}
	var ui_scale := clampf(minf(size.x / 1920.0, size.y / 1080.0), 0.62, 1.0)
	var ui_size := size / ui_scale
	if ui_size.x < 700.0 or ui_size.y < 420.0:
		return {}
	var build_catalog: Array = snapshot.get("build_catalog", [])
	if build_catalog.is_empty():
		return {}
	var build_origin := Vector2((ui_size.x - 540.0) * 0.5, 12.0)
	var units: Array = build_catalog.filter(func(item): return not bool(item.get("is_structure", false)))
	units.sort_custom(func(left, right): return int(left.get("type", -1)) < int(right.get("type", -1)))
	var structures: Array = build_catalog.filter(func(item): return bool(item.get("is_structure", false)))
	for index in range(mini(4, units.size())):
		var context := _build_hover_context_for(Rect2(build_origin + Vector2(8 + index * 132, 34), Vector2(128, 36)), units[index], pointer)
		if not context.is_empty():
			return context
	for index in range(4, mini(BUILD_UNIT_SHORTCUTS.size(), units.size())):
		var context := _build_hover_context_for(Rect2(build_origin + Vector2(8 + (index - 4) * 132, 72), Vector2(128, 36)), units[index], pointer)
		if not context.is_empty():
			return context
	for index in range(mini(4, structures.size())):
		var context := _build_hover_context_for(Rect2(build_origin + Vector2(8 + index * 132, 124), Vector2(128, 28)), structures[index], pointer)
		if not context.is_empty():
			return context
	return {}


func _build_hover_context_for(rect: Rect2, entry: Dictionary, pointer: Vector2) -> Dictionary:
	if not rect.has_point(pointer):
		return {}
	return {
		"key": "build:%s:%s" % [entry.get("type", ""), entry.get("is_structure", false)],
		"title": "BUILD // %s" % String(entry.get("name", "UNIT")),
		"detail": "WRENCH %.0f  //  BOLT %.0f  //  %.0f SEC  //  %s" % [float(entry.get("material", 0.0)), float(entry.get("energy", 0.0)), float(entry.get("build_seconds", 0.0)), "READY" if bool(entry.get("available", false)) else "LOCKED"],
		"accent": AMBER,
	}


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		queue_redraw()


func _panel(rect: Rect2, accent: Color) -> void:
	draw_rect(rect, PANEL, true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_line(rect.position + Vector2(1, 1), Vector2(rect.end.x - 1, rect.position.y + 1), accent, 2.0)


func _text(at: Vector2, value: String, font_size: int = 14, color: Color = INK, ui_scale: float = 1.0) -> void:
	draw_string(ui_font if ui_font != null else ThemeDB.fallback_font, at, value.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, floor(font_size * ui_scale) + DRAWN_FONT_SIZE_BUMP, color)


func _metric(rect: Rect2, label: String, value: String, income: String, accent: Color, icon_kind: String, ui_scale: float = 1.0) -> void:
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_rect(Rect2(rect.position, Vector2(3, rect.size.y)), accent, true)
	_draw_resource_icon(rect.position + Vector2(9, 7), icon_kind, 12.0, accent)
	_text(rect.position + Vector2(28, 15), label, 9, MUTED, ui_scale)
	_text(rect.position + Vector2(9, 34), value, 16, INK, ui_scale)
	if not income.is_empty():
		_text(rect.position + Vector2(9, 49), income, 9, accent, ui_scale)
	else:
		_text(rect.position + Vector2(9, 49), "LIVE STORAGE", 8, MUTED, ui_scale)


func _command_cell(rect: Rect2, key: String, title: String, detail: String, accent: Color, ui_scale: float = 1.0) -> void:
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_rect(Rect2(rect.position + Vector2(5, 7), Vector2(21, 21)), accent, true)
	_text(rect.position + Vector2(9, 22), key, 9, Color("#07131d"), ui_scale)
	_text(rect.position + Vector2(32, 16), title, 9, INK, ui_scale)
	_text(rect.position + Vector2(32, 30), detail, 8, MUTED, ui_scale)

func _command_icon(rect: Rect2, icon: String, tooltip: String, accent: Color, ui_scale: float = 1.0) -> void:
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	_text(rect.position + Vector2(rect.size.x * 0.5 - 5.0, rect.size.y * 0.5 + 6.0), icon, 18, accent, ui_scale)


func _build_cell(rect: Rect2, entry: Dictionary, key: String, ui_scale: float = 1.0) -> void:
	var available := bool(entry.get("available", false))
	var accent := GREEN if available else MUTED
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, accent, false, 1.0)
	var icon_rect := Rect2(rect.position + Vector2(6, 6), Vector2(25, 25))
	_draw_build_icon(icon_rect, int(entry.get("type", -1)), bool(entry.get("is_structure", false)), accent)
	draw_rect(Rect2(rect.position + Vector2(35, 5), Vector2(16, 14)), accent, true)
	_text(rect.position + Vector2(39, 16), key, 8, PANEL, ui_scale)
	_draw_wrench(rect.position + Vector2(58, 11), 8.0, Color("#47b9ff") if available else MUTED)
	_text(rect.position + Vector2(69, 17), "%.0f" % float(entry.get("material", 0.0)), 9, INK if available else MUTED, ui_scale)
	_draw_bolt(rect.position + Vector2(58, 25), 9.0, Color("#ff9a3d") if available else MUTED)
	_text(rect.position + Vector2(69, 31), "%.0f" % float(entry.get("energy", 0.0)), 9, INK if available else MUTED, ui_scale)


func _draw_wrench(at: Vector2, size: float, color: Color) -> void:
	# Shared vector geometry: a compact open-jaw wrench remains readable at
	# both 8 px build-card scale and 12 px economy-strip scale.
	var handle_start := at + Vector2(size * 0.18, size * 0.82)
	var handle_end := at + Vector2(size * 0.67, size * 0.33)
	draw_line(handle_start, handle_end, color, maxf(2.0, size * 0.20), true)
	draw_circle(handle_start, size * 0.19, color, true)
	draw_circle(handle_start, size * 0.08, PANEL, true)
	var jaw_center := at + Vector2(size * 0.75, size * 0.24)
	draw_arc(jaw_center, size * 0.27, -PI * 0.82, PI * 0.18, 8, color, maxf(1.6, size * 0.14), true)
	draw_line(jaw_center, jaw_center + Vector2(size * 0.22, -size * 0.08), color, maxf(1.6, size * 0.14), true)


func _draw_bolt(at: Vector2, size: float, color: Color) -> void:
	var bolt := PackedVector2Array([
		at + Vector2(size * 0.58, 0),
		at + Vector2(size * 0.12, size * 0.54),
		at + Vector2(size * 0.46, size * 0.54),
		at + Vector2(size * 0.30, size),
		at + Vector2(size, size * 0.34),
		at + Vector2(size * 0.60, size * 0.34),
	])
	draw_colored_polygon(bolt, color)
	draw_polyline(PackedVector2Array([bolt[0], bolt[1], bolt[2], bolt[3], bolt[4], bolt[5], bolt[0]]), color.darkened(0.22), maxf(0.7, size * 0.06), true)


func _draw_research(at: Vector2, size: float, color: Color) -> void:
	var center := at + Vector2(size * 0.5, size * 0.5)
	var points := PackedVector2Array()
	for index in range(6):
		var angle := -PI * 0.5 + float(index) * TAU / 6.0
		points.append(center + Vector2(cos(angle), sin(angle)) * size * 0.43)
	draw_colored_polygon(points, color)
	draw_circle(center, size * 0.14, PANEL, true)


func _draw_resource_icon(at: Vector2, kind: String, size: float, color: Color) -> void:
	match kind:
		"wrench":
			_draw_wrench(at, size, color)
		"bolt":
			_draw_bolt(at, size, color)
		"research":
			_draw_research(at, size, color)


func _draw_build_icon(rect: Rect2, type: int, is_structure: bool, color: Color) -> void:
	var center := rect.get_center()
	if is_structure:
		match type:
			100: # forward outpost
				draw_rect(Rect2(center - Vector2(8, 7), Vector2(16, 14)), color, false, 2.0)
				draw_line(center + Vector2(-4, 0), center + Vector2(4, 0), color, 2.0)
			101: # radar mast
				draw_line(center + Vector2(0, 9), center + Vector2(0, -8), color, 2.0)
				draw_arc(center + Vector2(0, -4), 7, PI, TAU, 10, color, 1.6)
			102: # airfield
				draw_rect(Rect2(center - Vector2(3, 10), Vector2(6, 20)), color, false, 1.8)
				draw_line(center + Vector2(0, -8), center + Vector2(0, 8), color, 1.3)
			103: # floodlight
				draw_line(center + Vector2(0, 9), center + Vector2(0, -3), color, 2.0)
				draw_circle(center + Vector2(0, -5), 3.0, color)
				draw_arc(center + Vector2(0, -5), 7.0, 0, TAU, 12, color, 1.2)
		return
	match type:
		9: # fighter: acute triangle
			draw_colored_polygon(PackedVector2Array([center + Vector2(0, -11), center + Vector2(-8, 9), center + Vector2(8, 9)]), color)
		13: # bomber: obtuse triangle
			draw_colored_polygon(PackedVector2Array([center + Vector2(-11, -6), center + Vector2(11, -6), center + Vector2(0, 10)]), color)
		8: # engineer: hexagonal strategic marker
			draw_arc(center, 9.0, 0, TAU, 6, color, 2.3)
		11: # patrol boat
			draw_colored_polygon(PackedVector2Array([center + Vector2(-11, 5), center + Vector2(11, 5), center + Vector2(6, 10), center + Vector2(-6, 10)]), color)
		_: # armored ground unit
			draw_rect(Rect2(center - Vector2(9, 6), Vector2(18, 12)), color, false, 2.2)
			draw_line(center + Vector2(0, -6), center + Vector2(0, 6), color, 1.8)


func _draw_hover_strip(width: float, height: float, ui_scale: float = 1.0) -> void:
	var strip := Rect2(0.0, height - 28.0, width, 28.0)
	draw_rect(strip, Color("#020609", 0.96), true)
	draw_line(strip.position, Vector2(strip.end.x, strip.position.y), PANEL_EDGE, 1.0)
	var title := String(snapshot.get("hover_title", "TACTICAL INSPECT"))
	var detail := String(snapshot.get("hover_detail", "HOVER A UNIT OR STRUCTURE TO INSPECT"))
	var accent := Color(snapshot.get("hover_accent", MUTED))
	_text(strip.position + Vector2(14, 18), title, 10, accent, ui_scale)
	_text(strip.position + Vector2(minf(280.0, width * 0.28), 18), detail, 10, INK, ui_scale)


func _draw() -> void:
	if snapshot.is_empty():
		return

	var ui_scale := clampf(minf(size.x / 1920.0, size.y / 1080.0), 0.62, 1.0)
	draw_set_transform(Vector2.ZERO, 0.0, Vector2(ui_scale, ui_scale))
	var w := size.x / ui_scale
	var h := size.y / ui_scale
	if w < 700.0 or h < 420.0:
		return

	# Top strategic identity and economy strip.
	var identity := Rect2(14, 12, 260, 48)
	_panel(identity, CYAN)
	_text(identity.position + Vector2(11, 17), "NFR // TACTICAL COMMAND", 9, CYAN, ui_scale)
	_text(identity.position + Vector2(11, 37), String(snapshot.get("scenario", "SKIRMISH")).to_upper(), 15, INK, ui_scale)
	var objective_status := String(snapshot.get("objective_status", ""))
	if not objective_status.is_empty():
		var objective := Rect2((w - 560.0) * 0.5, 12.0, 560.0, 40.0)
		var objective_accent := GREEN if objective_status.begins_with("VICTORY") else RED if objective_status.begins_with("DEFEAT") else AMBER
		_panel(objective, objective_accent)
		_text(objective.position + Vector2(10, 25), objective_status, 11, objective_accent, ui_scale)
	var reinforcement_status := String(snapshot.get("reinforcement_status", ""))
	if not reinforcement_status.is_empty():
		var reinforcement := Rect2((w - 560.0) * 0.5, 56.0, 560.0, 34.0)
		_panel(reinforcement, CYAN)
		_text(reinforcement.position + Vector2(10, 22), reinforcement_status, 10, CYAN, ui_scale)

	var economy_width := 374.0
	var economy := Rect2(w - economy_width - 14.0, 12, economy_width, 56)
	_panel(economy, GREEN)
	_metric(Rect2(economy.position + Vector2(6, 5), Vector2(114, 46)), "MATERIAL", str(snapshot.get("material", "--")), String(snapshot.get("material_income", "")), Color("#47b9ff"), "wrench", ui_scale)
	_metric(Rect2(economy.position + Vector2(126, 5), Vector2(114, 46)), "ENERGY", str(snapshot.get("energy", "--")), String(snapshot.get("energy_income", "")), Color("#ff9a3d"), "bolt", ui_scale)
	_metric(Rect2(economy.position + Vector2(246, 5), Vector2(122, 46)), "RESEARCH", str(snapshot.get("research", "--")), String(snapshot.get("research_income", "")), GREEN, "research", ui_scale)

	var selected_count := int(snapshot.get("selected", 0))
	# Selection has no empty-state card; it appears only when it contains facts.
	if selected_count > 0:
		var selection := Rect2(14, h - 196.0, 270, 150)
		_panel(selection, AMBER)
		_text(selection.position + Vector2(11, 20), "SELECTION", 9, AMBER, ui_scale)
		var selection_name := "%d UNITS SELECTED" % selected_count
		if selected_count == 1:
			selection_name = String(snapshot.get("unit_name", "COMBAT UNIT"))
		_text(selection.position + Vector2(11, 45), selection_name, 15, INK, ui_scale)
		var selection_line := String(snapshot.get("selection_detail", ""))
		if selected_count == 1:
			selection_line = "UNIT %s  •  COMMAND LINKED" % snapshot.get("unit_id", "--")
		_text(selection.position + Vector2(11, 65), selection_line, 9, MUTED, ui_scale)
		if selected_count == 1 and float(snapshot.get("unit_max_health", -1.0)) > 0.0:
			var health := float(snapshot.get("unit_health", 0.0))
			var max_health := float(snapshot.get("unit_max_health", 1.0))
			var health_ratio := clampf(health / max_health, 0.0, 1.0)
			_text(selection.position + Vector2(11, 91), "INTEGRITY  %.0f / %.0f" % [health, max_health], 9, MUTED, ui_scale)
			draw_rect(Rect2(selection.position + Vector2(11, 99), Vector2(248, 6)), Color("#132a35"), true)
			draw_rect(Rect2(selection.position + Vector2(11, 99), Vector2(248 * health_ratio, 6)), GREEN if health_ratio > 0.45 else RED, true)
			draw_rect(Rect2(selection.position + Vector2(11, 116), Vector2(248, 1)), PANEL_EDGE, true)
			_text(selection.position + Vector2(11, 133), "FRIENDLY FORCE", 9, MUTED, ui_scale)
			_text(selection.position + Vector2(11, 147), "%d  //  %s" % [snapshot.get("friendly", 0), snapshot.get("force_status", "READY")], 12, GREEN, ui_scale)
		else:
			draw_rect(Rect2(selection.position + Vector2(11, 86), Vector2(248, 1)), PANEL_EDGE, true)
			_text(selection.position + Vector2(11, 111), "FRIENDLY FORCE", 9, MUTED, ui_scale)
			_text(selection.position + Vector2(11, 136), "%d  //  %s" % [snapshot.get("friendly", 0), snapshot.get("force_status", "READY")], 15, GREEN, ui_scale)

	# Compact orders card aligns to Tactical Command and is absent with no unit.
	if selected_count > 0:
		var commands := Rect2(14, 68, 260, 78)
		_panel(commands, CYAN)
		_text(commands.position + Vector2(10, 14), "ORDERS", 9, CYAN, ui_scale)
		var icons := [["➜", "MOVE — right-click terrain", CYAN], ["✦", "ATTACK — Ctrl + right-click enemy", RED], ["■", "STOP — X", AMBER], ["⌑", "BUILD — blueprint / ROAD: R", GREEN], ["◇", "CLAIM — 2 then right-click facility", CYAN], ["×", "DEMOLISH — 3 then right-click facility", RED]]
		for index in range(6):
			var column := index % 3
			var row := index / 3
			_command_icon(Rect2(commands.position + Vector2(8 + column * 82, 20 + row * 27), Vector2(76, 23)), icons[index][0], icons[index][1], icons[index][2], ui_scale)

	# Production readout is sourced from the authoritative queue: the menu
	# exposes exact reservation costs and the active frame exposes time remaining.
	var build_catalog: Array = snapshot.get("build_catalog", [])
	if not build_catalog.is_empty() and bool(snapshot.get("can_build", false)):
		var build_menu := Rect2((w - 540.0) * 0.5, 12.0, 540.0, 158.0)
		_panel(build_menu, GREEN)
		_text(build_menu.position + Vector2(10, 15), "FIELD ENGINEER // BUILD MENU", 9, GREEN, ui_scale)
		var units: Array = build_catalog.filter(func(item): return not bool(item.get("is_structure", false)))
		units.sort_custom(func(left, right): return int(left.get("type", -1)) < int(right.get("type", -1)))
		var structures: Array = build_catalog.filter(func(item): return bool(item.get("is_structure", false)))
		_text(build_menu.position + Vector2(10, 29), "UNITS", 8, MUTED, ui_scale)
		for index in range(mini(4, units.size())):
			_build_cell(Rect2(build_menu.position + Vector2(8 + index * 132, 34), Vector2(128, 36)), units[index], BUILD_UNIT_SHORTCUTS[index], ui_scale)
		# The deck has six authored unit shortcuts. Never index beyond that
		# contract if a mod or future catalog adds more unit definitions.
		for index in range(4, mini(BUILD_UNIT_SHORTCUTS.size(), units.size())):
			_build_cell(Rect2(build_menu.position + Vector2(8 + (index - 4) * 132, 72), Vector2(128, 36)), units[index], BUILD_UNIT_SHORTCUTS[index], ui_scale)
		_text(build_menu.position + Vector2(10, 120), "STRUCTURES", 8, MUTED, ui_scale)
		for index in range(mini(4, structures.size())):
			_build_cell(Rect2(build_menu.position + Vector2(8 + index * 132, 124), Vector2(128, 28)), structures[index], ["6", "7", "A", "L"][index], ui_scale)
	var queue: Array = snapshot.get("queue", [])
	if not queue.is_empty():
		var active: Dictionary = queue[0]
		var fabrication := Rect2((w - 540.0) * 0.5, h - 294.0, 540.0, 86.0)
		_panel(fabrication, CYAN)
		_text(fabrication.position + Vector2(10, 16), "ACTIVE FABRICATION // STRUCTURE FRAME + UNIT SKELETON", 9, CYAN, ui_scale)
		var progress := clampf(float(active.get("progress", 0.0)), 0.0, 1.0)
		draw_rect(Rect2(fabrication.position + Vector2(10, 27), Vector2(520, 8)), Color("#132a35"), true)
		draw_rect(Rect2(fabrication.position + Vector2(10, 27), Vector2(520 * progress, 8)), GREEN, true)
		_text(fabrication.position + Vector2(10, 55), "%s  %.0f%%  //  %.1f SEC REMAINING" % [String(active.get("name", "UNIT")).to_upper(), progress * 100.0, float(active.get("remaining_seconds", 0.0))], 10, INK, ui_scale)
		_text(fabrication.position + Vector2(10, 73), "RESERVED: M %.0f  E %.0f  R %.0f" % [float(active.get("reserved_material", 0.0)), float(active.get("reserved_energy", 0.0)), float(active.get("reserved_research", 0.0))], 9, AMBER, ui_scale)

	var fob_installation: Array = snapshot.get("fob_installation", [])
	if fob_installation.size() >= 6:
		var fob_card := Rect2((w - 540.0) * 0.5, h - 392.0, 540.0, 78.0)
		var constructing := int(fob_installation[3]) == 1
		var fob_progress := clampf(float(fob_installation[4]), 0.0, 1.0)
		_panel(fob_card, AMBER if constructing else GREEN)
		_text(fob_card.position + Vector2(10, 16), "FORWARD OPERATING BASE // %s" % ("ASSEMBLY" if constructing else "ACTIVE"), 9, AMBER if constructing else GREEN, ui_scale)
		draw_rect(Rect2(fob_card.position + Vector2(10, 27), Vector2(520, 8)), Color("#132a35"), true)
		draw_rect(Rect2(fob_card.position + Vector2(10, 27), Vector2(520 * fob_progress, 8)), AMBER if constructing else GREEN, true)
		_text(fob_card.position + Vector2(10, 55), "%.0f%%  //  M %.0f  //  %s" % [fob_progress * 100.0, float(fob_installation[5]), "CONSTRUCTING" if constructing else "ONLINE"], 10, INK, ui_scale)

	var fob_notification := String(snapshot.get("fob_completion_notification", ""))
	if not fob_notification.is_empty():
		var notification_card := Rect2((w - 540.0) * 0.5, h - 450.0, 540.0, 42.0)
		_panel(notification_card, GREEN)
		_text(notification_card.position + Vector2(10, 26), fob_notification, 11, GREEN, ui_scale)

	_draw_hover_strip(w, h, ui_scale)
