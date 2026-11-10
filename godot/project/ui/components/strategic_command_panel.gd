class_name StrategicCommandPanel
extends Control
## Strategic Command: where forces are ordered. Every card comes from the
## native unit data (get_build_catalog), so the list, prices and availability
## are the simulation's own. An ordered unit is paid for at once, then flies in
## and lands at the player's airfield; the panel shows what is still on its way.
## Light units can instead be dropped by parachute onto ground the player can
## see, with no airfield: PARADROP, then a click on the map.
##
## Layout: a centred modal window over a dimmed battlefield. Header (title,
## G/Esc hint, close), category tabs, a status line, a grid of unit cards, and a
## footer with forces in flight and logistics requests.

signal order_unit(unit_type: int, mode: int)
signal order_paradrop(unit_type: int)
signal request_fuel
signal request_recon
signal dismissed

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const CardComponent = preload("res://ui/components/mandate_card.gd")
const BadgeComponent = preload("res://ui/components/status_badge.gd")
const CostComponent = preload("res://ui/components/resource_cost_display.gd")
const TabsComponent = preload("res://ui/components/mandate_tabs.gd")
const HeaderComponent = preload("res://ui/components/section_header.gd")
const PanelComponent = preload("res://ui/components/mandate_panel.gd")
const KeycapComponent = preload("res://ui/components/keycap.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")
const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")
const BuildDescriptions = preload("res://ui/build_descriptions.gd")
const DESCRIPTION_HINT := "HOVER A CARD FOR THE UNIT'S ROLE AND WHAT IT ENGAGES"

const TAB_GROUND := 0
const TAB_AIR := 1
const WINDOW_WIDTH := 1120.0
const CARD_WIDTH := 252.0
## The status line above the cards already explains a shared refusal at length.
const SHORT_REASONS := {
	"AIRFIELD REQUIRED FOR REINFORCEMENTS": "NEEDS AIRFIELD",
	"INSUFFICIENT MATERIALS": "NOT ENOUGH MATERIALS",
	"ROAD ROUTE UNAVAILABLE": "NEEDS ROAD ROUTE",
	"RAIL ROUTE UNAVAILABLE": "NEEDS RAIL ROUTE",
}

var _tab := TAB_GROUND
var _ground_transport_mode := 0
var _catalog: Array = []
var _inbound: Array = []
var _unit_names: Dictionary = {}
var _status := ""
var _cards: GridContainer
var _inbound_label: Label
var _status_label: Label
var _status_rail: ColorRect
var _tabs: TabsComponent
var _transport_tabs: TabsComponent
var _window: PanelContainer
var _fingerprint := ""
## Detail line under the cards: the hovered unit's description.
var _description: Label


func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	var background := ColorRect.new()
	background.name = "Backdrop"
	background.color = Tokens.SURFACE_SCRIM
	background.mouse_filter = Control.MOUSE_FILTER_STOP
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	var center := CenterContainer.new()
	center.name = "ResponsiveMargin"
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(center)
	_window = PanelComponent.new()
	_window.name = "Window"
	_window.surface = "modal"
	_window.rail_color = Tokens.ACCENT
	_window.mouse_filter = Control.MOUSE_FILTER_STOP
	center.add_child(_window)
	var root := VBoxContainer.new()
	root.name = "StrategicCommand"
	root.add_theme_constant_override("separation", 0)
	_window.add_child(root)

	# Header: title on the left, how to close on the right.
	var header_margin := _margin(Tokens.SPACE_XL, Tokens.SPACE_LG, Tokens.SPACE_XL, Tokens.SPACE_LG)
	root.add_child(header_margin)
	var header_row := HBoxContainer.new()
	header_row.add_theme_constant_override("separation", Tokens.SPACE_MD)
	header_margin.add_child(header_row)
	var header := HeaderComponent.new()
	header.title = "STRATEGIC COMMAND"
	header.detail = "THEATER REINFORCEMENT & DEPLOYMENT"
	header.display = true
	header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header_row.add_child(header)
	var hint := HBoxContainer.new()
	hint.add_theme_constant_override("separation", Tokens.SPACE_XS)
	hint.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	header_row.add_child(hint)
	hint.add_child(KeycapComponent.make("G"))
	hint.add_child(KeycapComponent.make("ESC"))
	var close_button := ButtonComponent.new()
	close_button.name = "Close"
	close_button.text = "CLOSE"
	close_button.variant = "ghost"
	close_button.icon_id = &"controls.close"
	close_button.tooltip_text = "Close Strategic Command  [G / Esc]"
	close_button.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	close_button.pressed.connect(dismissed.emit)
	header_row.add_child(close_button)
	root.add_child(HSeparator.new())

	var body := _margin(Tokens.SPACE_XL, Tokens.SPACE_LG, Tokens.SPACE_XL, Tokens.SPACE_XL)
	root.add_child(body)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", Tokens.SPACE_LG)
	body.add_child(column)
	var tab_row := HBoxContainer.new()
	tab_row.add_theme_constant_override("separation", Tokens.SPACE_LG)
	column.add_child(tab_row)
	_tabs = TabsComponent.new()
	_tabs.name = "Tabs"
	tab_row.add_child(_tabs)
	_tabs.set_tabs(PackedStringArray(["GROUND", "AIR"]))
	_tabs.tab_selected.connect(_select_tab)
	_transport_tabs = TabsComponent.new()
	_transport_tabs.name = "GroundTransportTabs"
	_transport_tabs.set_tabs(PackedStringArray(["AIRLIFT", "ROAD", "RAIL"]))
	_transport_tabs.tab_selected.connect(_select_ground_transport)
	column.add_child(_transport_tabs)
	var status_box := HBoxContainer.new()
	status_box.add_theme_constant_override("separation", Tokens.SPACE_SM)
	status_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status_box.alignment = BoxContainer.ALIGNMENT_END
	tab_row.add_child(status_box)
	_status_rail = ColorRect.new()
	_status_rail.custom_minimum_size = Vector2(3, 20)
	_status_rail.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	status_box.add_child(_status_rail)
	_status_label = Label.new()
	_status_label.name = "DeliveryStatus"
	_status_label.theme_type_variation = "CommandLabel"
	_status_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	status_box.add_child(_status_label)
	_cards = GridContainer.new()
	_cards.name = "Cards"
	_cards.columns = 4
	_cards.add_theme_constant_override("h_separation", Tokens.SPACE_MD)
	_cards.add_theme_constant_override("v_separation", Tokens.SPACE_MD)
	column.add_child(_cards)
	_description = Label.new()
	_description.name = "UnitDescription"
	_description.theme_type_variation = "CaptionLabel"
	_description.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_description.custom_minimum_size = Vector2(WINDOW_WIDTH - Tokens.SPACE_XL * 2, 36.0)
	_description.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_child(_description)
	show_description({})

	# Footer: forces in flight, then logistics requests.
	root.add_child(HSeparator.new())
	var footer_margin := _margin(Tokens.SPACE_XL, Tokens.SPACE_MD, Tokens.SPACE_XL, Tokens.SPACE_MD)
	root.add_child(footer_margin)
	var footer := HBoxContainer.new()
	footer.add_theme_constant_override("separation", Tokens.SPACE_LG)
	footer_margin.add_child(footer)
	var inbound_column := VBoxContainer.new()
	inbound_column.add_theme_constant_override("separation", 2)
	inbound_column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	inbound_column.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	footer.add_child(inbound_column)
	var inbound_title := Label.new()
	inbound_title.text = "IN FLIGHT"
	inbound_title.theme_type_variation = "EyebrowLabel"
	inbound_column.add_child(inbound_title)
	_inbound_label = Label.new()
	_inbound_label.name = "Inbound"
	_inbound_label.theme_type_variation = "CommandLabel"
	_inbound_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_inbound_label.custom_minimum_size.x = 420
	inbound_column.add_child(_inbound_label)
	var fuel_button := ButtonComponent.new()
	fuel_button.name = "RequestFuel"
	fuel_button.text = "REQUEST FUEL SHIPMENT"
	fuel_button.variant = "secondary"
	fuel_button.icon_id = &"resource.fuel"
	fuel_button.tooltip_text = "Fly in a fuel shipment on an unarmed cargo aircraft. It can be shot down.  [J]"
	fuel_button.pressed.connect(request_fuel.emit)
	footer.add_child(fuel_button)
	var recon_button := ButtonComponent.new()
	recon_button.name = "RequestRecon"
	recon_button.text = "PLOT RECON FLIGHT"
	recon_button.variant = "secondary"
	recon_button.icon_id = &"intel.recon"
	recon_button.tooltip_text = "Plot a reconnaissance flight over the map."
	recon_button.pressed.connect(request_recon.emit)
	footer.add_child(recon_button)
	var help := Label.new()
	help.name = "Help"
	help.theme_type_variation = "CaptionLabel"
	help.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	help.custom_minimum_size.x = WINDOW_WIDTH - Tokens.SPACE_XL * 2
	help.text = "Forces arrive from outside the theater. AIRLIFT needs an airfield; ROAD and RAIL use their marked corridors and unload at separate terminals. RAIL is faster. PARADROP sends light units to ground you can see."
	var help_margin := _margin(Tokens.SPACE_XL, 0, Tokens.SPACE_XL, Tokens.SPACE_LG)
	root.add_child(help_margin)
	help_margin.add_child(help)
	_refresh()


func _margin(left: int, top: int, right: int, bottom: int) -> MarginContainer:
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", left)
	margin.add_theme_constant_override("margin_top", top)
	margin.add_theme_constant_override("margin_right", right)
	margin.add_theme_constant_override("margin_bottom", bottom)
	return margin


func _notification(what: int) -> void:
	if what == NOTIFICATION_RESIZED and is_instance_valid(_cards):
		# Fewer columns on a narrow window rather than overflowing the screen.
		var usable := size.x - Tokens.SPACE_2XL * 2 - Tokens.SPACE_XL * 2
		_cards.columns = clampi(int((usable + Tokens.SPACE_MD) / (CARD_WIDTH + Tokens.SPACE_MD)), 2, 4)


func _select_tab(index: int) -> void:
	_tab = index
	_fingerprint = ""
	_refresh()


func _select_ground_transport(index: int) -> void:
	_ground_transport_mode = index
	_fingerprint = ""
	_refresh()


## catalog: native get_build_catalog entries. inbound: delivery_state().inbound.
## names: unit type -> display name, for the inbound list.
func set_state(catalog: Array, inbound: Array, status: String) -> void:
	_catalog = catalog
	_inbound = inbound
	_status = status
	_refresh()


# The units this tab offers: ground units that are flown in, or airfield aircraft.
func tab_entries() -> Array:
	var entries: Array = []
	for entry in _catalog:
		if bool(entry.get("is_structure", false)):
			continue
		var is_air := bool(entry.get("is_aircraft", false))
		var delivered := bool(entry.get("delivered", false))
		if (_tab == TAB_AIR and is_air) or (_tab == TAB_GROUND and delivered and not is_air):
			entries.append(entry)
	entries.sort_custom(func(left, right): return int(left.get("type", -1)) < int(right.get("type", -1)))
	return entries


static func status_tone(status: String) -> Color:
	var upper := status.to_upper()
	if upper.contains("REQUIRED") or upper.contains("INSUFFICIENT") or upper.contains("LOW") or upper.contains("REJECT") or upper.contains("CANNOT") or upper.contains("NO "):
		return Tokens.WARNING
	if upper.contains("LOST") or upper.contains("SHOT DOWN") or upper.contains("DESTROYED") or upper.contains("OUT OF FUEL"):
		return Tokens.DANGER
	return Tokens.ACCENT


## The description line shows `entry`'s unit, or the hint when entry is empty.
func show_description(entry: Dictionary) -> void:
	if not is_instance_valid(_description):
		return
	var text := BuildDescriptions.for_entry(entry) if not entry.is_empty() else ""
	_description.text = "%s  //  %s" % [String(entry.get("name", "UNIT")).to_upper(), text] if not text.is_empty() else DESCRIPTION_HINT
	_description.add_theme_color_override("font_color", Tokens.TEXT_SECONDARY if not text.is_empty() else Tokens.TEXT_MUTED)


func description_text() -> String:
	return _description.text if is_instance_valid(_description) else ""


func _refresh() -> void:
	if not is_instance_valid(_cards):
		return
	_status_label.text = _status.to_upper()
	_transport_tabs.visible = _tab == TAB_GROUND
	var tone := status_tone(_status)
	_status_label.add_theme_color_override("font_color", tone)
	_status_rail.color = tone
	_status_rail.visible = not _status.is_empty()
	for entry in _catalog:
		_unit_names[int(entry.get("type", -1))] = String(entry.get("name", "UNIT"))
	var lines: PackedStringArray = []
	for force in _inbound:
		var mode := int(force.get("route_mode", 0))
		var drop := "PARADROP " if bool(force.get("airdrop", false)) else ("ROAD " if mode == 1 else "RAIL " if mode == 2 else "AIR ")
		lines.append("%s%s  %.0f S" % [drop, String(_unit_names.get(int(force.type), "UNIT")).to_upper(), float(force.remaining_ms) / 1000.0])
	_inbound_label.text = "INBOUND  //  " + ("   ·   ".join(lines) if not lines.is_empty() else "NONE")
	_inbound_label.add_theme_color_override("font_color", Tokens.ACCENT_STRONG if not lines.is_empty() else Tokens.TEXT_MUTED)
	var entries := tab_entries()
	var next := "%d|%d|%s" % [_tab, _ground_transport_mode, JSON.stringify(entries)]
	if next == _fingerprint:
		return
	_fingerprint = next
	for child in _cards.get_children():
		_cards.remove_child(child)
		child.queue_free()
	if entries.is_empty():
		var empty := Label.new()
		empty.text = "NO FORCES AVAILABLE" if _tab == TAB_GROUND else "NO AIRCRAFT AVAILABLE"
		empty.theme_type_variation = "MutedLabel"
		_cards.add_child(empty)
		return
	for entry in entries:
		_add_card(entry)


# The card builds its layout when it enters the tree, so each piece is added
# to a parent that is already in the tree before anything is put inside it.
func _add_card(entry: Dictionary) -> void:
	var available := bool(entry.get("available", false))
	var reason := String(entry.get("reason", ""))
	if _tab == TAB_GROUND and _ground_transport_mode > 0:
		reason = String(entry.get("road_reason" if _ground_transport_mode == 1 else "rail_reason", ""))
		available = reason.is_empty()
	var holder := VBoxContainer.new()
	holder.name = "Card_%d" % int(entry.get("type", -1))
	holder.add_theme_constant_override("separation", 0)
	holder.custom_minimum_size.x = CARD_WIDTH
	_cards.add_child(holder)
	var card := CardComponent.new()
	card.title = String(entry.get("name", "UNIT")).to_upper()
	var is_air := bool(entry.get("is_aircraft", false))
	card.subtitle = ("AIRCRAFT" if is_air else "GROUND FORCE") + ("  ·  PARADROP CAPABLE" if bool(entry.get("airdrop", false)) else "")
	card.disabled = not available
	card.accent = Tokens.ACCENT if available else Tokens.BORDER_STRONG
	var symbol := StringName(String(entry.get("nato_symbol_id", "")))
	if not symbol.is_empty():
		card.icon = UiIconRegistry.get_nato_symbol(symbol, &"friendly")
	card.custom_minimum_size = Vector2(CARD_WIDTH, 136)
	card.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var description := BuildDescriptions.for_entry(entry)
	card.tooltip_text = description
	for hover_target: Control in [holder, card]:
		hover_target.mouse_entered.connect(show_description.bind(entry))
	holder.add_child(card)
	var stats := HBoxContainer.new()
	stats.add_theme_constant_override("separation", Tokens.SPACE_LG)
	stats.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.body().add_child(stats)
	var cost := CostComponent.new()
	cost.material_cost = int(float(entry.get("material", 0.0)))
	cost.insufficient = reason == "INSUFFICIENT MATERIALS"
	stats.add_child(cost)
	var time := Label.new()
	time.text = "%.0f S + %s" % [float(entry.get("build_seconds", 0.0)), "FLIGHT" if is_air or _ground_transport_mode == 0 else "ROAD" if _ground_transport_mode == 1 else "RAIL"]
	time.theme_type_variation = "MutedLabel"
	time.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	time.tooltip_text = "Build time before the transport departs, then its flight time."
	time.mouse_filter = Control.MOUSE_FILTER_PASS
	stats.add_child(time)
	var badge := BadgeComponent.new()
	badge.text = "READY TO ORDER" if available else SHORT_REASONS.get(reason, reason if not reason.is_empty() else "UNAVAILABLE")
	badge.tooltip_text = reason
	badge.mouse_filter = Control.MOUSE_FILTER_PASS
	badge.state = "available" if available else ("danger" if reason == "INSUFFICIENT MATERIALS" else "warning")
	card.body().add_child(badge)
	var order := ButtonComponent.new()
	order.name = "Order"
	order.text = "ORDER"
	order.variant = "primary"
	order.disabled = not available
	order.custom_minimum_size.y = 40
	order.tooltip_text = "Order %s for %d Materials via %s." % [String(entry.get("name", "unit")), int(float(entry.get("material", 0.0))), "air" if is_air or _ground_transport_mode == 0 else "road" if _ground_transport_mode == 1 else "rail"]
	order.pressed.connect(func(): order_unit.emit(int(entry.get("type", -1)), 0 if is_air else _ground_transport_mode))
	order.mouse_entered.connect(show_description.bind(entry))
	holder.add_child(order)
	if bool(entry.get("airdrop", false)):
		# A drop needs no airfield, so only the airfield refusal leaves it open.
		# Whether the chosen ground is watched is checked when it is clicked.
		var drop_open := available or reason == "AIRFIELD REQUIRED FOR REINFORCEMENTS"
		var paradrop := ButtonComponent.new()
		paradrop.name = "Paradrop"
		paradrop.text = "PARADROP"
		paradrop.variant = "secondary"
		paradrop.icon_id = &"logistics.airlift"
		paradrop.disabled = not drop_open
		paradrop.custom_minimum_size.y = 40
		paradrop.tooltip_text = "Drop by parachute onto ground you can see. No airfield needed; then click the map."
		paradrop.pressed.connect(func(): order_paradrop.emit(int(entry.get("type", -1))))
		paradrop.mouse_entered.connect(show_description.bind(entry))
		holder.add_child(paradrop)
	else:
		# Keeps every ORDER button on one line when a neighbour has PARADROP.
		var spacer := Control.new()
		spacer.name = "Spacer"
		spacer.custom_minimum_size.y = 40
		spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
		holder.add_child(spacer)
