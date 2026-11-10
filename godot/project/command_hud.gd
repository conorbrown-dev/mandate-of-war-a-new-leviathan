extends Control

# The tactical HUD. Information sits where an RTS player looks for it:
#   top-left      operation and game speed
#   top-centre    objective and logistics status
#   top-right     resources (stock, capacity, fill)
#   bottom-left   the selection and an orders reference
#   bottom-centre the engineer build catalog and progress
#   bottom edge   the inspect strip and the key hotkeys
# Every number shown comes from Main's snapshot. Panels share the Mandate look
# (glass surface, hairline border, corner brackets, accent rail) with the
# node-based components, through MandatePanel.draw_panel_chrome and the tokens.

var snapshot: Dictionary = {}
signal build_requested(build_type: int)

const MandateTokens = preload("res://ui/theme/ui_tokens.gd")
const Fonts = preload("res://ui/theme/mandate_fonts.gd")
const PanelChrome = preload("res://ui/components/mandate_panel.gd")
const INK := MandateTokens.TEXT_PRIMARY
const MUTED := MandateTokens.TEXT_SECONDARY
const FAINT := MandateTokens.TEXT_MUTED
const PANEL := MandateTokens.SURFACE_GLASS
const PANEL_EDGE := MandateTokens.BORDER_DEFAULT
const CARD_SURFACE := Color(MandateTokens.SURFACE_RAISED, 0.9)
const INSET_SURFACE := Color(MandateTokens.SURFACE_INSET, 0.92)
const PROGRESS_TRACK := MandateTokens.SURFACE_INSET
const CYAN := MandateTokens.ACCENT
const AMBER := MandateTokens.WARNING
const RED := MandateTokens.DANGER
const GREEN := MandateTokens.SUCCESS
const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")
const ResponsiveUiScript = preload("res://ui/responsive_ui.gd")
const BuildCatalogScript = preload("res://ui/components/build_catalog.gd")
const EDGE := 16.0
const STRIP_HEIGHT := 34.0
const ORDERS := [
	[&"order.move", "MOVE", "RMB"],
	[&"combat.attack", "ATTACK", "CTRL RMB"],
	[&"order.waypoint", "QUEUE", "SHIFT RMB"],
	[&"order.stop", "STOP", "X"],
	[&"system.capture", "CLAIM SITE", "2"],
	[&"construction.demolish", "DEMOLISH", "3"],
]
const STRIP_HINTS := [["G", "STRATEGIC COMMAND"], ["M", "MAGNIFIER"], ["F1", "CONTROLS"], ["F2", "AUDIO"]]
var build_catalog_view: Control


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
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
		return build_catalog_view.get_hover_context(pointer)
	return {}


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED:
		queue_redraw()


# --- drawing primitives -----------------------------------------------------

func _panel(rect: Rect2, accent: Color) -> void:
	draw_rect(rect, PANEL, true)
	draw_rect(rect, PANEL_EDGE, false, 1.0)
	PanelChrome.draw_panel_chrome(self, rect, accent, true)


func _text(at: Vector2, value: String, font_size: int = 14, color: Color = INK, font: Font = null) -> void:
	draw_string(font if font != null else Fonts.display(), at, value, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)


func _caption(at: Vector2, value: String, color: Color = FAINT, font_size: int = MandateTokens.FONT_XS) -> void:
	draw_string(Fonts.label(), at, value.to_upper(), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, color)


func _text_width(value: String, font_size: int, font: Font = null) -> float:
	return (font if font != null else Fonts.display()).get_string_size(value, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x


func _text_right(right: Vector2, value: String, font_size: int, color: Color, font: Font = null) -> void:
	_text(right - Vector2(_text_width(value, font_size, font), 0), value, font_size, color, font)


func _bar(rect: Rect2, ratio: float, color: Color, segments: int = 0) -> void:
	draw_rect(rect, PROGRESS_TRACK, true)
	draw_rect(Rect2(rect.position, Vector2(rect.size.x * clampf(ratio, 0.0, 1.0), rect.size.y)), color, true)
	for index in range(1, segments):
		var x := rect.position.x + rect.size.x * float(index) / float(segments)
		draw_line(Vector2(x, rect.position.y), Vector2(x, rect.end.y), PANEL, 2.0)


## Draws a key cap with its text; returns its width.
func _keycap(at: Vector2, label: String) -> float:
	var font_size := 12
	var width := maxf(22.0, _text_width(label, font_size) + 12.0)
	var rect := Rect2(at, Vector2(width, 20))
	draw_rect(rect, MandateTokens.SURFACE_RAISED, true)
	draw_rect(rect, MandateTokens.BORDER_STRONG, false, 1.0)
	draw_line(Vector2(rect.position.x, rect.end.y), rect.end, MandateTokens.BORDER_STRONG, 2.0)
	_text(at + Vector2((width - _text_width(label, font_size)) * 0.5, 15), label, font_size, INK)
	return width


func _draw_ui_icon(rect: Rect2, icon_id: StringName, color: Color) -> void:
	var texture := UiIconRegistry.get_icon(icon_id)
	if texture != null:
		draw_texture_rect(texture, rect, false, color)
		return
	draw_rect(rect.grow(-1.0), color, false, 1.0)


func _status_color(text: String) -> Color:
	var upper := text.to_upper()
	if upper.begins_with("VICTORY") or upper.contains("ONLINE") or upper.contains("READY"):
		return GREEN
	if upper.begins_with("DEFEAT") or upper.contains("LOST") or upper.contains("OUT OF FUEL") or upper.contains("SHOT DOWN"):
		return RED
	if upper.contains("REQUIRED") or upper.contains("LOW") or upper.contains("CANNOT") or upper.contains("REJECT"):
		return AMBER
	return CYAN


# --- HUD sections -------------------------------------------------------------

func _draw_operation(w: float) -> void:
	var rect := Rect2(EDGE, EDGE, 300, 64)
	_panel(rect, CYAN)
	_caption(rect.position + Vector2(14, 20), "OPERATION", CYAN)
	_text(rect.position + Vector2(14, 47), String(snapshot.get("scenario", "SKIRMISH")).to_upper(), MandateTokens.FONT_XL, INK)
	# Game speed as a compact readout with its keys.
	var speed := "%d%%" % int(snapshot.get("game_speed", 100))
	var right := rect.end.x - 14
	if bool(snapshot.get("speed_locked", false)):
		_caption(Vector2(right - _text_width("MULTIPLAYER", 11, Fonts.label()), rect.position.y + 20), "MULTIPLAYER", FAINT)
		_text_right(Vector2(right, rect.position.y + 47), speed, MandateTokens.FONT_LG, MUTED)
		return
	_caption(Vector2(right - _text_width("SPEED", 11, Fonts.label()), rect.position.y + 20), "SPEED", FAINT)
	var plus_x := right - 22
	_keycap(Vector2(plus_x, rect.position.y + 32), "+")
	_keycap(Vector2(plus_x - 26, rect.position.y + 32), "-")
	_text_right(Vector2(plus_x - 34, rect.position.y + 47), speed, MandateTokens.FONT_LG, INK if speed == "100%" else AMBER)


func _draw_status_banner(w: float) -> void:
	var objective := String(snapshot.get("objective_status", ""))
	var logistics := String(snapshot.get("reinforcement_status", ""))
	if objective.is_empty() and logistics.is_empty():
		return
	var width := 600.0
	var lines := int(not objective.is_empty()) + int(not logistics.is_empty())
	var rect := Rect2((w - width) * 0.5, EDGE, width, 18.0 + 30.0 * lines)
	var accent := _status_color(objective) if not objective.is_empty() else _status_color(logistics)
	_panel(rect, accent)
	var y := rect.position.y + 28
	if not objective.is_empty():
		_draw_ui_icon(Rect2(rect.position.x + 14, y - 13, 16, 16), &"system.objective", accent)
		_caption(Vector2(rect.position.x + 38, y), "OBJECTIVE", FAINT)
		_text(Vector2(rect.position.x + 120, y + 1), objective.to_upper(), MandateTokens.FONT_MD, accent)
		y += 30
	if not logistics.is_empty():
		var tone := _status_color(logistics)
		_draw_ui_icon(Rect2(rect.position.x + 14, y - 13, 16, 16), &"logistics.airlift", tone)
		_caption(Vector2(rect.position.x + 38, y), "LOGISTICS", FAINT)
		_text(Vector2(rect.position.x + 120, y + 1), logistics.to_upper(), MandateTokens.FONT_MD, tone)


func _resource_values(index: int) -> Vector2:
	var resources: Array = snapshot.get("tactical_resources", [])
	if resources.size() <= index + 1:
		return Vector2(-1, -1)
	return Vector2(float(resources[index]), float(resources[index + 1]))


func _metric(rect: Rect2, label: String, icon_id: StringName, accent: Color, value: String, unit: String, ratio: float, footnote: String, footnote_color: Color) -> void:
	_draw_ui_icon(Rect2(rect.position + Vector2(0, 2), Vector2(14, 14)), icon_id, accent)
	_caption(rect.position + Vector2(20, 13), label, FAINT)
	_text(rect.position + Vector2(0, 40), value, MandateTokens.FONT_XL, INK)
	_text(rect.position + Vector2(_text_width(value, MandateTokens.FONT_XL) + 5, 40), unit, MandateTokens.FONT_SM, FAINT)
	if ratio >= 0.0:
		_bar(Rect2(rect.position + Vector2(0, 49), Vector2(rect.size.x, 3)), ratio, accent)
	if not footnote.is_empty():
		_text_right(Vector2(rect.end.x, rect.position.y + 13), footnote, MandateTokens.FONT_XS, footnote_color, Fonts.label())


func _draw_resources(w: float) -> void:
	var cell := 168.0
	var gap := 18.0
	var width := cell * 3 + gap * 2 + 28
	var rect := Rect2(w - width - EDGE, EDGE, width, 72)
	var deficit := bool(snapshot.get("power_deficit", false))
	_panel(rect, RED if deficit else MandateTokens.BORDER_STRONG)
	var origin := rect.position + Vector2(14, 10)
	var materials := _resource_values(0)
	var fuel := _resource_values(2)
	var power := _resource_values(4)
	if materials.x >= 0.0:
		_metric(Rect2(origin, Vector2(cell, 52)), "MATERIALS", &"resource.material", MandateTokens.RESOURCE_MASS, "%.0f" % materials.x, "/ %.0f" % materials.y, materials.x / maxf(materials.y, 1.0), "", FAINT)
	else:
		_metric(Rect2(origin, Vector2(cell, 52)), "MATERIALS", &"resource.material", MandateTokens.RESOURCE_MASS, str(snapshot.get("materials", "--")), "", -1.0, "", FAINT)
	origin.x += cell + gap
	var fuel_low := fuel.x >= 0.0 and fuel.x < fuel.y * 0.15
	if fuel.x >= 0.0:
		_metric(Rect2(origin, Vector2(cell, 52)), "FUEL", &"resource.fuel", MandateTokens.RESOURCE_FUEL, "%.0f" % fuel.x, "/ %.0f" % fuel.y, fuel.x / maxf(fuel.y, 1.0), "LOW" if fuel_low else "", RED)
	else:
		_metric(Rect2(origin, Vector2(cell, 52)), "FUEL", &"resource.fuel", MandateTokens.RESOURCE_FUEL, str(snapshot.get("fuel", "--")), "", -1.0, "", FAINT)
	origin.x += cell + gap
	var reserve := String(snapshot.get("power_reserve", "")).replace("RESERVE ", "")
	if power.x >= 0.0:
		var ratio := power.x / power.y if power.y > 0.0 else 0.0
		_metric(Rect2(origin, Vector2(cell, 52)), "POWER", &"resource.power", RED if deficit else MandateTokens.RESOURCE_POWER, "%.0f" % power.x, "/ %.0f MW" % power.y, ratio, reserve, RED if deficit else FAINT)
	else:
		_metric(Rect2(origin, Vector2(cell, 52)), "POWER", &"resource.power", MandateTokens.RESOURCE_POWER, str(snapshot.get("power", "--")), "", -1.0, reserve, FAINT)


func _draw_selection(h: float) -> float:
	var selected_count := int(snapshot.get("selected", 0))
	if selected_count <= 0:
		return h
	var single := selected_count == 1 and float(snapshot.get("unit_max_health", -1.0)) > 0.0
	var height := 156.0 if single else 128.0
	var rect := Rect2(EDGE, h - STRIP_HEIGHT - EDGE - height, 340, height)
	_panel(rect, GREEN)
	var x := rect.position.x + 16
	_caption(Vector2(x, rect.position.y + 22), "SELECTION", GREEN)
	var count := "%d UNIT%s" % [selected_count, "" if selected_count == 1 else "S"]
	_text_right(Vector2(rect.end.x - 16, rect.position.y + 22), count, MandateTokens.FONT_XS, MUTED, Fonts.label())
	var title := String(snapshot.get("unit_name", "COMBAT UNIT")) if selected_count == 1 else "%d UNITS" % selected_count
	_text(Vector2(x, rect.position.y + 52), title.to_upper(), MandateTokens.FONT_XL, INK)
	var detail := String(snapshot.get("selection_detail", ""))
	if selected_count == 1:
		var parts := Array(detail.split("•"))
		parts = parts.slice(1).map(func(part): return String(part).strip_edges())
		detail = "UNIT %s  ·  %s" % [snapshot.get("unit_id", "--"), "  ·  ".join(PackedStringArray(parts)) if not parts.is_empty() else "AWAITING ORDERS"]
	_text(Vector2(x, rect.position.y + 74), detail.to_upper(), MandateTokens.FONT_SM, MUTED, Fonts.body())
	var y := rect.position.y + 92
	if single:
		var health := float(snapshot.get("unit_health", 0.0))
		var max_health := float(snapshot.get("unit_max_health", 1.0))
		var ratio := clampf(health / max_health, 0.0, 1.0)
		var tone := GREEN if ratio > 0.6 else AMBER if ratio > 0.3 else RED
		_caption(Vector2(x, y + 10), "INTEGRITY", FAINT)
		_text_right(Vector2(rect.end.x - 16, y + 11), "%.0f / %.0f" % [health, max_health], MandateTokens.FONT_SM, tone)
		_bar(Rect2(x, y + 18, rect.size.x - 32, 6), ratio, tone, 10)
		y += 34
	draw_line(Vector2(x, y), Vector2(rect.end.x - 16, y), MandateTokens.BORDER_SUBTLE, 1.0)
	_caption(Vector2(x, y + 22), "FRIENDLY FORCE", FAINT)
	_text_right(Vector2(rect.end.x - 16, y + 23), "%d  ·  %s" % [int(snapshot.get("friendly", 0)), String(snapshot.get("force_status", "READY"))], MandateTokens.FONT_SM, GREEN)
	return rect.position.y


func _draw_orders(bottom: float) -> void:
	if int(snapshot.get("selected", 0)) <= 0:
		return
	# A reference of how to give orders, not buttons: the mouse and keys do it.
	# It stacks above the selection so the bottom centre stays free for building.
	var row_height := 24.0
	var height := 40.0 + row_height * ORDERS.size()
	var rect := Rect2(EDGE, bottom - 8 - height, 340, height)
	_panel(rect, CYAN)
	_caption(rect.position + Vector2(16, 22), "ORDERS", CYAN)
	_caption(Vector2(rect.end.x - 16 - _text_width("R  ROAD", 11, Fonts.label()), rect.position.y + 22), "R  ROAD", FAINT)
	for index in range(ORDERS.size()):
		var at := rect.position + Vector2(16, 34 + index * row_height)
		var order: Array = ORDERS[index]
		_draw_ui_icon(Rect2(at + Vector2(0, 2), Vector2(16, 16)), order[0], CYAN if index < 3 else (AMBER if index == 3 else MUTED))
		_text(at + Vector2(24, 15), String(order[1]), MandateTokens.FONT_SM, INK)
		var key_x := rect.size.x - 32
		var parts := Array(String(order[2]).split(" "))
		parts.reverse()
		for part in parts:
			var key_width := maxf(22.0, _text_width(String(part), 12) + 12.0)
			key_x -= key_width
			_keycap(at + Vector2(key_x, -1), String(part))
			key_x -= 4


func _draw_progress(w: float, h: float) -> void:
	var catalog_top := h - STRIP_HEIGHT - EDGE
	if build_catalog_view != null and build_catalog_view.visible:
		catalog_top = build_catalog_view.get_rect().position.y / ResponsiveUiScript.layout_scale(size)
	var y := catalog_top - 8
	var width := 540.0
	# Production readout is sourced from the authoritative queue.
	var queue: Array = snapshot.get("queue", [])
	if not queue.is_empty():
		var active: Dictionary = queue[0]
		var rect := Rect2((w - width) * 0.5, y - 76, width, 76)
		y = rect.position.y - 8
		var progress := clampf(float(active.get("progress", 0.0)), 0.0, 1.0)
		_panel(rect, CYAN)
		_caption(rect.position + Vector2(16, 22), "CONSTRUCTION", CYAN)
		_text_right(Vector2(rect.end.x - 16, rect.position.y + 22), "%.1f S REMAINING" % float(active.get("remaining_seconds", 0.0)), MandateTokens.FONT_XS, MUTED, Fonts.label())
		_text(rect.position + Vector2(16, 48), "%s" % String(active.get("name", "UNIT")).to_upper(), MandateTokens.FONT_LG, INK)
		_text_right(Vector2(rect.end.x - 16, rect.position.y + 48), "%.0f%%" % (progress * 100.0), MandateTokens.FONT_LG, CYAN)
		_bar(Rect2(rect.position + Vector2(16, 58), Vector2(width - 32, 5)), progress, CYAN)
	var fob_installation: Array = snapshot.get("fob_installation", [])
	if fob_installation.size() >= 6:
		var constructing := int(fob_installation[3]) == 1
		var fob_progress := clampf(float(fob_installation[4]), 0.0, 1.0)
		var tone := AMBER if constructing else GREEN
		var rect := Rect2((w - width) * 0.5, y - 76, width, 76)
		y = rect.position.y - 8
		_panel(rect, tone)
		_caption(rect.position + Vector2(16, 22), "FORWARD OPERATING BASE", tone)
		_text_right(Vector2(rect.end.x - 16, rect.position.y + 22), "MATERIALS %.0f" % float(fob_installation[5]), MandateTokens.FONT_XS, MUTED, Fonts.label())
		_text(rect.position + Vector2(16, 48), "CONSTRUCTING" if constructing else "ONLINE", MandateTokens.FONT_LG, INK)
		_text_right(Vector2(rect.end.x - 16, rect.position.y + 48), "%.0f%%" % (fob_progress * 100.0), MandateTokens.FONT_LG, tone)
		_bar(Rect2(rect.position + Vector2(16, 58), Vector2(width - 32, 5)), fob_progress, tone)
	var fob_notification := String(snapshot.get("fob_completion_notification", ""))
	if not fob_notification.is_empty():
		var rect := Rect2((w - width) * 0.5, y - 44, width, 44)
		_panel(rect, GREEN)
		_draw_ui_icon(Rect2(rect.position + Vector2(16, 14), Vector2(16, 16)), &"status.healthy", GREEN)
		_text(rect.position + Vector2(40, 28), fob_notification, MandateTokens.FONT_MD, GREEN)


func _draw_hover_strip(width: float, height: float) -> void:
	var strip := Rect2(0.0, height - STRIP_HEIGHT, width, STRIP_HEIGHT)
	draw_rect(strip, INSET_SURFACE, true)
	draw_line(strip.position, Vector2(strip.end.x, strip.position.y), PANEL_EDGE, 1.0)
	var title := String(snapshot.get("hover_title", "TACTICAL INSPECT"))
	var detail := String(snapshot.get("hover_detail", "HOVER A UNIT OR STRUCTURE TO INSPECT"))
	var accent := Color(snapshot.get("hover_accent", FAINT))
	var baseline := strip.position.y + 22
	draw_rect(Rect2(strip.position + Vector2(EDGE, 9), Vector2(3, 16)), accent, true)
	_caption(Vector2(EDGE + 12, baseline - 1), title, accent, 12)
	var detail_x := EDGE + 12 + maxf(260.0, _text_width(title.to_upper(), 12, Fonts.label()) + 24)
	_text(Vector2(detail_x, baseline), detail.to_upper(), MandateTokens.FONT_SM, MUTED, Fonts.body())
	# Always-visible reminders of the three keys a new player needs most.
	var x := width - EDGE
	for index in range(STRIP_HINTS.size() - 1, -1, -1):
		var hint: Array = STRIP_HINTS[index]
		var label_width := _text_width(String(hint[1]), 11, Fonts.label())
		x -= label_width
		_caption(Vector2(x, baseline - 1), String(hint[1]), FAINT)
		var key_width := maxf(22.0, _text_width(String(hint[0]), 12) + 12.0)
		x -= key_width + 6
		_keycap(Vector2(x, strip.position.y + 7), String(hint[0]))
		x -= 20


func _draw() -> void:
	if snapshot.is_empty():
		return
	var ui_scale := ResponsiveUiScript.layout_scale(size)
	draw_set_transform(Vector2.ZERO, 0.0, Vector2(ui_scale, ui_scale))
	var w := size.x / ui_scale
	var h := size.y / ui_scale
	if w < 700.0 or h < 420.0:
		return
	_draw_operation(w)
	_draw_status_banner(w)
	_draw_resources(w)
	_draw_orders(_draw_selection(h))
	_draw_progress(w, h)
	_draw_hover_strip(w, h)
