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


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func refresh(next_snapshot: Dictionary) -> void:
	snapshot = next_snapshot.duplicate(true)
	queue_redraw()


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		queue_redraw()


func _panel(rect: Rect2, accent: Color) -> void:
	draw_rect(rect, PANEL, true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_line(rect.position + Vector2(1, 1), Vector2(rect.end.x - 1, rect.position.y + 1), accent, 2.0)


func _text(at: Vector2, value: String, font_size: int = 14, color: Color = INK) -> void:
	draw_string(ThemeDB.fallback_font, at, value, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)


func _metric(rect: Rect2, label: String, value: String, income: String, accent: Color) -> void:
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_rect(Rect2(rect.position, Vector2(3, rect.size.y)), accent, true)
	_text(rect.position + Vector2(9, 15), label, 9, MUTED)
	_text(rect.position + Vector2(9, 34), value, 16, INK)
	if not income.is_empty():
		_text(rect.position + Vector2(9, 49), income, 9, accent)
	else:
		_text(rect.position + Vector2(9, 49), "LIVE STORAGE", 8, MUTED)


func _command_cell(rect: Rect2, key: String, title: String, detail: String, accent: Color) -> void:
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_rect(Rect2(rect.position + Vector2(5, 7), Vector2(21, 21)), accent, true)
	_text(rect.position + Vector2(9, 22), key, 9, Color("#07131d"))
	_text(rect.position + Vector2(32, 16), title, 9, INK)
	_text(rect.position + Vector2(32, 30), detail, 8, MUTED)

func _command_icon(rect: Rect2, icon: String, tooltip: String, accent: Color) -> void:
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	_text(rect.position + Vector2(rect.size.x * 0.5 - 5.0, rect.size.y * 0.5 + 6.0), icon, 18, accent)
	if rect.has_point(get_local_mouse_position()):
		var tip := Rect2(rect.position + Vector2(0, rect.size.y + 4), Vector2(260, 19))
		draw_rect(tip, Color("#07131d", 0.97), true)
		draw_rect(tip, accent, false, 1.0)
		_text(tip.position + Vector2(6, 13), tooltip, 9, INK)


func _build_cell(rect: Rect2, entry: Dictionary, key: String) -> void:
	var available := bool(entry.get("available", false))
	var accent := GREEN if available else MUTED
	draw_rect(rect, Color("#0b1c27", 0.96), true)
	draw_rect(rect, accent, false, 1.0)
	_text(rect.position + Vector2(8, 14), key, 9, accent)
	_text(rect.position + Vector2(28, 14), String(entry.get("name", "UNIT")).to_upper(), 10, INK if available else MUTED)
	_text(rect.position + Vector2(8, 30), "M %.0f  E %.0f" % [float(entry.get("material", 0.0)), float(entry.get("energy", 0.0))], 9, AMBER if available else MUTED)
	_text(rect.position + Vector2(8, 44), "%.1f SEC  //  %s" % [float(entry.get("build_seconds", 0.0)), "READY" if available else "LOCKED"], 8, GREEN if available else RED)


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
	_text(identity.position + Vector2(11, 17), "NFR // TACTICAL COMMAND", 9, CYAN)
	_text(identity.position + Vector2(11, 37), String(snapshot.get("scenario", "SKIRMISH")).to_upper(), 15)

	var economy_width := 374.0
	var economy := Rect2(w - economy_width - 14.0, 12, economy_width, 56)
	_panel(economy, GREEN)
	_metric(Rect2(economy.position + Vector2(6, 5), Vector2(114, 46)), "MATERIAL", str(snapshot.get("material", "--")), String(snapshot.get("material_income", "")), AMBER)
	_metric(Rect2(economy.position + Vector2(126, 5), Vector2(114, 46)), "ENERGY", str(snapshot.get("energy", "--")), String(snapshot.get("energy_income", "")), CYAN)
	_metric(Rect2(economy.position + Vector2(246, 5), Vector2(122, 46)), "RESEARCH", str(snapshot.get("research", "--")), String(snapshot.get("research_income", "")), GREEN)

	var selected_count := int(snapshot.get("selected", 0))
	# Selection has no empty-state card; it appears only when it contains facts.
	if selected_count > 0:
		var selection := Rect2(14, h - 164.0, 270, 150)
		_panel(selection, AMBER)
		_text(selection.position + Vector2(11, 20), "SELECTION", 9, AMBER)
		var selection_name := "%d UNITS SELECTED" % selected_count
		if selected_count == 1:
			selection_name = String(snapshot.get("unit_name", "COMBAT UNIT"))
		_text(selection.position + Vector2(11, 45), selection_name, 15)
		var selection_line := String(snapshot.get("selection_detail", ""))
		if selected_count == 1:
			selection_line = "UNIT %s  •  COMMAND LINKED" % snapshot.get("unit_id", "--")
		_text(selection.position + Vector2(11, 65), selection_line, 9, MUTED)
		if selected_count == 1 and float(snapshot.get("unit_max_health", -1.0)) > 0.0:
			var health := float(snapshot.get("unit_health", 0.0))
			var max_health := float(snapshot.get("unit_max_health", 1.0))
			var health_ratio := clampf(health / max_health, 0.0, 1.0)
			_text(selection.position + Vector2(11, 91), "INTEGRITY  %.0f / %.0f" % [health, max_health], 9, MUTED)
			draw_rect(Rect2(selection.position + Vector2(11, 99), Vector2(248, 6)), Color("#132a35"), true)
			draw_rect(Rect2(selection.position + Vector2(11, 99), Vector2(248 * health_ratio, 6)), GREEN if health_ratio > 0.45 else RED, true)
			draw_rect(Rect2(selection.position + Vector2(11, 116), Vector2(248, 1)), PANEL_EDGE, true)
			_text(selection.position + Vector2(11, 133), "FRIENDLY FORCE", 9, MUTED)
			_text(selection.position + Vector2(11, 147), "%d  //  %s" % [snapshot.get("friendly", 0), snapshot.get("force_status", "READY")], 12, GREEN)
		else:
			draw_rect(Rect2(selection.position + Vector2(11, 86), Vector2(248, 1)), PANEL_EDGE, true)
			_text(selection.position + Vector2(11, 111), "FRIENDLY FORCE", 9, MUTED)
			_text(selection.position + Vector2(11, 136), "%d  //  %s" % [snapshot.get("friendly", 0), snapshot.get("force_status", "READY")], 15, GREEN)

	# Compact orders card aligns to Tactical Command and is absent with no unit.
	if selected_count > 0:
		var commands := Rect2(14, 68, 260, 78)
		_panel(commands, CYAN)
		_text(commands.position + Vector2(10, 14), "ORDERS", 9, CYAN)
		var icons := [["➜", "MOVE — right-click terrain", CYAN], ["✦", "ATTACK — Ctrl + right-click enemy", RED], ["■", "STOP — X", AMBER], ["⌑", "BUILD — choose a blueprint", GREEN], ["◇", "CLAIM — 2 then right-click facility", CYAN], ["×", "DEMOLISH — 3 then right-click facility", RED]]
		for index in range(6):
			var column := index % 3
			var row := index / 3
			_command_icon(Rect2(commands.position + Vector2(8 + column * 82, 20 + row * 27), Vector2(76, 23)), icons[index][0], icons[index][1], icons[index][2])

	# Production readout is sourced from the authoritative queue: the menu
	# exposes exact reservation costs and the active frame exposes time remaining.
	var build_catalog: Array = snapshot.get("build_catalog", [])
	if not build_catalog.is_empty() and bool(snapshot.get("can_build", false)):
		var build_menu := Rect2((w - 540.0) * 0.5, 12.0, 540.0, 114.0)
		_panel(build_menu, GREEN)
		_text(build_menu.position + Vector2(10, 15), "COMMAND WALKER // BUILD MENU", 9, GREEN)
		var units: Array = build_catalog.filter(func(item): return not bool(item.get("is_structure", false)))
		var structures: Array = build_catalog.filter(func(item): return bool(item.get("is_structure", false)))
		_text(build_menu.position + Vector2(10, 29), "UNITS", 8, MUTED)
		for index in range(mini(3, units.size())):
			_build_cell(Rect2(build_menu.position + Vector2(8 + index * 176, 34), Vector2(168, 32)), units[index], ["1", "4", "5"][index])
		_text(build_menu.position + Vector2(10, 76), "STRUCTURES", 8, MUTED)
		for index in range(mini(2, structures.size())):
			_build_cell(Rect2(build_menu.position + Vector2(8 + index * 176, 81), Vector2(168, 26)), structures[index], ["6", "7"][index])

	var queue: Array = snapshot.get("queue", [])
	if not queue.is_empty():
		var active: Dictionary = queue[0]
		var fabrication := Rect2((w - 540.0) * 0.5, h - 294.0, 540.0, 86.0)
		_panel(fabrication, CYAN)
		_text(fabrication.position + Vector2(10, 16), "ACTIVE FABRICATION // STRUCTURE FRAME + UNIT SKELETON", 9, CYAN)
		var progress := clampf(float(active.get("progress", 0.0)), 0.0, 1.0)
		draw_rect(Rect2(fabrication.position + Vector2(10, 27), Vector2(520, 8)), Color("#132a35"), true)
		draw_rect(Rect2(fabrication.position + Vector2(10, 27), Vector2(520 * progress, 8)), GREEN, true)
		_text(fabrication.position + Vector2(10, 55), "%s  %.0f%%  //  %.1f SEC REMAINING" % [String(active.get("name", "UNIT")).to_upper(), progress * 100.0, float(active.get("remaining_seconds", 0.0))], 10, INK)
		_text(fabrication.position + Vector2(10, 73), "RESERVED: M %.0f  E %.0f  R %.0f" % [float(active.get("reserved_material", 0.0)), float(active.get("reserved_energy", 0.0)), float(active.get("reserved_research", 0.0))], 9, AMBER)
