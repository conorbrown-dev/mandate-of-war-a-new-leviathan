class_name StrategicCommandPanel
extends Control
## Strategic Command: where forces are ordered. Every card comes from the
## native unit data (get_build_catalog), so the list, prices and availability
## are the simulation's own. An ordered unit is paid for at once, then flies in
## and lands at the player's airfield; the panel shows what is still on its way.

signal order_unit(unit_type: int)
signal request_fuel
signal request_recon
signal dismissed

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const CardComponent = preload("res://ui/components/mandate_card.gd")
const BadgeComponent = preload("res://ui/components/status_badge.gd")
const CostComponent = preload("res://ui/components/resource_cost_display.gd")
const TabsComponent = preload("res://ui/components/mandate_tabs.gd")
const HeaderComponent = preload("res://ui/components/section_header.gd")
const TooltipComponent = preload("res://ui/components/mandate_tooltip.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")

const TAB_GROUND := 0
const TAB_AIR := 1

var _tab := TAB_GROUND
var _catalog: Array = []
var _inbound: Array = []
var _unit_names: Dictionary = {}
var _status := ""
var _cards: HBoxContainer
var _inbound_label: Label
var _status_label: Label
var _tabs: TabsComponent
var _fingerprint := ""


func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	var background := ColorRect.new()
	background.name = "Backdrop"
	background.color = Tokens.SURFACE_BASE
	background.mouse_filter = Control.MOUSE_FILTER_STOP
	background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(background)
	var margin := MarginContainer.new()
	margin.name = "ResponsiveMargin"
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right"]:
		margin.add_theme_constant_override("margin_" + side, 32)
	for side in ["top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 28)
	add_child(margin)
	var root := VBoxContainer.new()
	root.name = "StrategicCommand"
	root.add_theme_constant_override("separation", Tokens.SPACE_LG)
	margin.add_child(root)
	var header := HeaderComponent.new()
	header.title = "STRATEGIC COMMAND"
	header.detail = "THEATER REINFORCEMENT & DEPLOYMENT"
	root.add_child(header)
	_tabs = TabsComponent.new()
	_tabs.name = "Tabs"
	root.add_child(_tabs)
	_tabs.set_tabs(PackedStringArray(["GROUND", "AIR"]))
	# Tabs repaint themselves; the panel only needs to know which was pressed.
	_tabs.ready.connect(_watch_tabs)
	_watch_tabs()
	_cards = HBoxContainer.new()
	_cards.name = "Cards"
	_cards.add_theme_constant_override("separation", Tokens.SPACE_MD)
	root.add_child(_cards)
	_inbound_label = Label.new()
	_inbound_label.name = "Inbound"
	_inbound_label.theme_type_variation = "StatLabel"
	root.add_child(_inbound_label)
	var actions := HBoxContainer.new()
	actions.add_theme_constant_override("separation", Tokens.SPACE_SM)
	root.add_child(actions)
	var fuel_button := ButtonComponent.new()
	fuel_button.name = "RequestFuel"
	fuel_button.text = "REQUEST FUEL SHIPMENT"
	fuel_button.variant = "secondary"
	fuel_button.pressed.connect(request_fuel.emit)
	actions.add_child(fuel_button)
	var recon_button := ButtonComponent.new()
	recon_button.name = "RequestRecon"
	recon_button.text = "PLOT RECON FLIGHT"
	recon_button.variant = "secondary"
	recon_button.pressed.connect(request_recon.emit)
	actions.add_child(recon_button)
	var close_button := ButtonComponent.new()
	close_button.name = "Close"
	close_button.text = "CLOSE"
	close_button.variant = "danger"
	close_button.pressed.connect(dismissed.emit)
	actions.add_child(close_button)
	_status_label = Label.new()
	_status_label.name = "DeliveryStatus"
	_status_label.theme_type_variation = "StatLabel"
	root.add_child(_status_label)
	var tooltip := TooltipComponent.new()
	tooltip.custom_minimum_size.x = 360
	tooltip.set_content("DELIVERY WINDOW", "Forces are not built in the theater. An order is paid for at once, then flies in and lands beside your airfield after its build and flight time. Build an airfield first. Costs and availability come from the native simulation.")
	root.add_child(tooltip)
	_refresh()


func _watch_tabs() -> void:
	for index in range(_tabs.get_child_count()):
		var button := _tabs.get_child(index) as Button
		if button != null and not button.pressed.is_connected(_select_tab.bind(index)):
			button.pressed.connect(_select_tab.bind(index))


func _select_tab(index: int) -> void:
	_tab = index
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


func _refresh() -> void:
	if not is_instance_valid(_cards):
		return
	_status_label.text = _status.to_upper()
	var lines: PackedStringArray = []
	for force in _inbound:
		lines.append("%s  %.0f S" % [String(_unit_names.get(int(force.type), "UNIT")).to_upper(), float(force.remaining_ms) / 1000.0])
	_inbound_label.text = "INBOUND  //  " + ("  |  ".join(lines) if not lines.is_empty() else "NONE")
	for entry in _catalog:
		_unit_names[int(entry.get("type", -1))] = String(entry.get("name", "UNIT"))
	var entries := tab_entries()
	var next := "%d|%s" % [_tab, JSON.stringify(entries)]
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
	var holder := VBoxContainer.new()
	holder.name = "Card_%d" % int(entry.get("type", -1))
	holder.add_theme_constant_override("separation", Tokens.SPACE_XS)
	_cards.add_child(holder)
	var card := CardComponent.new()
	card.title = String(entry.get("name", "UNIT")).to_upper()
	card.subtitle = "%.0f S BUILD  +  FLIGHT" % float(entry.get("build_seconds", 0.0))
	card.disabled = not available
	card.custom_minimum_size = Vector2(230, 112)
	holder.add_child(card)
	var cost := CostComponent.new()
	cost.material_cost = int(float(entry.get("material", 0.0)))
	cost.insufficient = String(entry.get("reason", "")) == "INSUFFICIENT MATERIALS"
	card.get_child(0).add_child(cost)
	var badge := BadgeComponent.new()
	var reason := String(entry.get("reason", ""))
	badge.text = "AVAILABLE" if available else (reason if not reason.is_empty() else "UNAVAILABLE")
	badge.state = "available" if available else "locked"
	card.get_child(0).add_child(badge)
	var order := ButtonComponent.new()
	order.name = "Order"
	order.text = "ORDER"
	order.variant = "primary"
	order.disabled = not available
	order.pressed.connect(func(): order_unit.emit(int(entry.get("type", -1))))
	holder.add_child(order)
