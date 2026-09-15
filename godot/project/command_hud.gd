extends Control

# A compact tactical deck inspired by the information placement of classic
# large-scale RTS interfaces: strategic information stays at the top, the
# selected force is anchored bottom-left, and commands remain bottom-center.
# This is an original presentation; every number shown comes from Main.

var snapshot: Dictionary = {}
signal build_requested(build_type: int)

const MandateTokens = preload("res://ui/theme/ui_tokens.gd")
const INK := MandateTokens.TEXT_PRIMARY
const MUTED := MandateTokens.TEXT_SECONDARY
const PANEL := Color(MandateTokens.SURFACE_BASE, 0.90)
const PANEL_EDGE := Color(MandateTokens.BORDER_DEFAULT, 0.96)
const CARD_SURFACE := Color(MandateTokens.SURFACE_PANEL, 0.96)
const INSET_SURFACE := Color(MandateTokens.SURFACE_INSET, 0.96)
const PROGRESS_TRACK := MandateTokens.SURFACE_SELECTED
const CYAN := MandateTokens.ACCENT
const AMBER := MandateTokens.WARNING
const RED := MandateTokens.DANGER
const GREEN := MandateTokens.SUCCESS
const BUILD_UNIT_SHORTCUTS := ["1", "4", "5", "9", "0", "P"]
const UiTypographyScript = preload("res://ui_typography.gd")
const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")
const ResponsiveUiScript = preload("res://ui/responsive_ui.gd")
const BuildCatalogScript = preload("res://ui/components/build_catalog.gd")
const DRAWN_FONT_SIZE_BUMP := 0
var ui_font: Font
var build_catalog_view: Control


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_font = UiTypographyScript.compact_tactical_font()
	build_catalog_view = BuildCatalogScript.new()
	build_catalog_view.build_requested.connect(func(build_type: int): build_requested.emit(build_type))
	add_child(build_catalog_view)


func refresh(next_snapshot: Dictionary) -> void:
	snapshot = next_snapshot.duplicate(true)
	if build_catalog_view != null:
		build_catalog_view.refresh_catalog(snapshot.get("build_catalog", []), bool(snapshot.get("can_build", false)))
	queue_redraw()


func get_build_hover_context() -> Dictionary:
	var ui_scale := ResponsiveUiScript.layout_scale(size)
	return get_build_hover_context_at(get_local_mouse_position() / ui_scale)


func get_build_hover_context_at(pointer: Vector2) -> Dictionary:
	if build_catalog_view != null:
		var node_context: Dictionary = build_catalog_view.get_hover_context(pointer)
		if not node_context.is_empty():
			return node_context
	if snapshot.is_empty() or not bool(snapshot.get("can_build", false)):
		return {}
	var ui_scale := ResponsiveUiScript.layout_scale(size)
	var ui_size := size / ui_scale
	if ui_size.x < 700.0 or ui_size.y < 420.0:
		return {}
	var build_catalog: Array = snapshot.get("build_catalog", [])
	if build_catalog.is_empty():
		return {}
	var build_origin := ResponsiveUiScript.centered_origin(size, ResponsiveUiScript.PANEL_MAX_WIDTH)
	var units: Array = build_catalog.filter(func(item): return not bool(item.get("is_structure", false)) and not bool(item.get("is_aircraft", false)))
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


func _metric(rect: Rect2, label: String, value: String, income: String, accent: Color, icon_id: StringName, ui_scale: float = 1.0) -> void:
	draw_rect(rect, CARD_SURFACE, true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_rect(Rect2(rect.position, Vector2(3, rect.size.y)), accent, true)
	_draw_ui_icon(Rect2(rect.position + Vector2(9, 7), Vector2(12, 12)), icon_id, accent)
	_text(rect.position + Vector2(28, 15), label, 9, MUTED, ui_scale)
	_text(rect.position + Vector2(9, 34), value, 16, INK, ui_scale)
	if not income.is_empty():
		_text(rect.position + Vector2(9, 49), income, 9, accent, ui_scale)
	else:
		_text(rect.position + Vector2(9, 49), "LIVE STORAGE", 8, MUTED, ui_scale)


func _command_cell(rect: Rect2, key: String, title: String, detail: String, accent: Color, ui_scale: float = 1.0) -> void:
	draw_rect(rect, CARD_SURFACE, true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	draw_rect(Rect2(rect.position + Vector2(5, 7), Vector2(21, 21)), accent, true)
	_text(rect.position + Vector2(9, 22), key, 9, MandateTokens.SURFACE_BASE, ui_scale)
	_text(rect.position + Vector2(32, 16), title, 9, INK, ui_scale)
	_text(rect.position + Vector2(32, 30), detail, 8, MUTED, ui_scale)

func _command_icon(rect: Rect2, icon_id: StringName, tooltip: String, accent: Color, ui_scale: float = 1.0) -> void:
	draw_rect(rect, CARD_SURFACE, true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	_draw_ui_icon(Rect2(rect.position + Vector2(5, 4), Vector2(16, 16)), icon_id, accent)


func _build_cell(rect: Rect2, entry: Dictionary, key: String, ui_scale: float = 1.0) -> void:
	var available := bool(entry.get("available", false))
	var accent := GREEN if available else MUTED
	draw_rect(rect, CARD_SURFACE, true)
	draw_rect(rect, accent, false, 1.0)
	var icon_rect := Rect2(rect.position + Vector2(6, 6), Vector2(25, 25))
	var nato_symbol_id := StringName(String(entry.get("nato_symbol_id", "")))
	if not nato_symbol_id.is_empty():
		_draw_nato_symbol(icon_rect, nato_symbol_id, &"friendly")
	else:
		_draw_ui_icon(icon_rect, StringName(String(entry.get("icon_id", "construction.build"))), accent)
	draw_rect(Rect2(rect.position + Vector2(35, 5), Vector2(16, 14)), accent, true)
	_text(rect.position + Vector2(39, 16), key, 8, PANEL, ui_scale)
	_draw_ui_icon(Rect2(rect.position + Vector2(58, 5), Vector2(12, 12)), &"resource.material", Color("#47b9ff") if available else MUTED)
	_text(rect.position + Vector2(69, 17), "%.0f" % float(entry.get("material", 0.0)), 9, INK if available else MUTED, ui_scale)
	_draw_ui_icon(Rect2(rect.position + Vector2(58, 19), Vector2(12, 12)), &"resource.energy", Color("#ff9a3d") if available else MUTED)
	_text(rect.position + Vector2(69, 31), "%.0f" % float(entry.get("energy", 0.0)), 9, INK if available else MUTED, ui_scale)


func _draw_ui_icon(rect: Rect2, icon_id: StringName, color: Color) -> void:
	var texture := UiIconRegistry.get_icon(icon_id)
	if texture != null:
		draw_texture_rect(texture, rect, false, color)
		return
	_draw_missing_icon(rect, color)


func _draw_nato_symbol(rect: Rect2, symbol_id: StringName, affiliation: StringName) -> void:
	var texture := UiIconRegistry.get_nato_symbol(symbol_id, affiliation)
	if texture != null:
		draw_texture_rect(texture, rect, false, Color.WHITE)
		return
	_draw_missing_icon(rect, MUTED)


func _draw_missing_icon(rect: Rect2, color: Color) -> void:
	draw_rect(rect.grow(-1.0), color, false, 1.0)
	draw_line(rect.position + Vector2(2, 2), rect.end - Vector2(2, 2), color, 1.0)
	draw_line(Vector2(rect.end.x - 2, rect.position.y + 2), Vector2(rect.position.x + 2, rect.end.y - 2), color, 1.0)


func _draw_hover_strip(width: float, height: float, ui_scale: float = 1.0) -> void:
	var strip := Rect2(0.0, height - 28.0, width, 28.0)
	draw_rect(strip, INSET_SURFACE, true)
	draw_line(strip.position, Vector2(strip.end.x, strip.position.y), PANEL_EDGE, 1.0)
	var title := String(snapshot.get("hover_title", "TACTICAL INSPECT"))
	var detail := String(snapshot.get("hover_detail", "HOVER A UNIT OR STRUCTURE TO INSPECT"))
	var accent := Color(snapshot.get("hover_accent", MUTED))
	_text(strip.position + Vector2(14, 18), title, 10, accent, ui_scale)
	_text(strip.position + Vector2(minf(280.0, width * 0.28), 18), detail, 10, INK, ui_scale)


func _draw() -> void:
	if snapshot.is_empty():
		return

	var ui_scale := ResponsiveUiScript.layout_scale(size)
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
	_metric(Rect2(economy.position + Vector2(6, 5), Vector2(114, 46)), "MATERIAL", str(snapshot.get("material", "--")), String(snapshot.get("material_income", "")), Color("#47b9ff"), &"resource.material", ui_scale)
	_metric(Rect2(economy.position + Vector2(126, 5), Vector2(114, 46)), "ENERGY", str(snapshot.get("energy", "--")), String(snapshot.get("energy_income", "")), Color("#ff9a3d"), &"resource.energy", ui_scale)
	_metric(Rect2(economy.position + Vector2(246, 5), Vector2(122, 46)), "RESEARCH", str(snapshot.get("research", "--")), String(snapshot.get("research_income", "")), GREEN, &"resource.research", ui_scale)

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
			draw_rect(Rect2(selection.position + Vector2(11, 99), Vector2(248, 6)), PROGRESS_TRACK, true)
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
		var icons := [[&"order.move", "MOVE — right-click terrain", CYAN], [&"combat.attack", "ATTACK — Ctrl + right-click enemy", RED], [&"order.stop", "STOP — X", AMBER], [&"construction.build", "BUILD — blueprint / ROAD: R", GREEN], [&"system.capture", "CLAIM — 2 then right-click facility", CYAN], [&"construction.demolish", "DEMOLISH — 3 then right-click facility", RED]]
		for index in range(6):
			var column := index % 3
			var row := index / 3
			_command_icon(Rect2(commands.position + Vector2(8 + column * 82, 20 + row * 27), Vector2(76, 23)), icons[index][0], icons[index][1], icons[index][2], ui_scale)

	# Production readout is sourced from the authoritative queue. The build
	# catalog itself is a themed node-based component above, not drawn here.
	var queue: Array = snapshot.get("queue", [])
	if not queue.is_empty():
		var active: Dictionary = queue[0]
		var fabrication := Rect2((w - 540.0) * 0.5, h - 294.0, 540.0, 86.0)
		_panel(fabrication, CYAN)
		_text(fabrication.position + Vector2(10, 16), "ACTIVE FABRICATION // STRUCTURE FRAME + UNIT SKELETON", 9, CYAN, ui_scale)
		var progress := clampf(float(active.get("progress", 0.0)), 0.0, 1.0)
		draw_rect(Rect2(fabrication.position + Vector2(10, 27), Vector2(520, 8)), PROGRESS_TRACK, true)
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
		draw_rect(Rect2(fob_card.position + Vector2(10, 27), Vector2(520, 8)), PROGRESS_TRACK, true)
		draw_rect(Rect2(fob_card.position + Vector2(10, 27), Vector2(520 * fob_progress, 8)), AMBER if constructing else GREEN, true)
		_text(fob_card.position + Vector2(10, 55), "%.0f%%  //  M %.0f  //  %s" % [fob_progress * 100.0, float(fob_installation[5]), "CONSTRUCTING" if constructing else "ONLINE"], 10, INK, ui_scale)

	var fob_notification := String(snapshot.get("fob_completion_notification", ""))
	if not fob_notification.is_empty():
		var notification_card := Rect2((w - 540.0) * 0.5, h - 450.0, 540.0, 42.0)
		_panel(notification_card, GREEN)
		_text(notification_card.position + Vector2(10, 26), fob_notification, 11, GREEN, ui_scale)

	_draw_hover_strip(w, h, ui_scale)
