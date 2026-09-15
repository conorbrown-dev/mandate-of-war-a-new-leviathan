extends Control

signal select_delivery_zone
signal request_package
signal dismissed

const ButtonComponent = preload("res://ui/components/mandate_button.gd")
const CardComponent = preload("res://ui/components/mandate_card.gd")
const BadgeComponent = preload("res://ui/components/status_badge.gd")
const CostComponent = preload("res://ui/components/resource_cost_display.gd")
const TabsComponent = preload("res://ui/components/mandate_tabs.gd")
const HeaderComponent = preload("res://ui/components/section_header.gd")
const TooltipComponent = preload("res://ui/components/mandate_tooltip.gd")
const Tokens = preload("res://ui/theme/ui_tokens.gd")

var _request_button: Button
var _select_zone_button: Button
var _status_label: Label

func _ready() -> void:
	theme = preload("res://ui/theme/mandate_theme.tres")
	var background := ColorRect.new(); background.name = "Backdrop"; background.color = Tokens.SURFACE_BASE; background.mouse_filter = Control.MOUSE_FILTER_IGNORE; background.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); add_child(background)
	var margin := MarginContainer.new(); margin.name = "ResponsiveMargin"; margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); margin.add_theme_constant_override("margin_left", 32); margin.add_theme_constant_override("margin_right", 32); margin.add_theme_constant_override("margin_top", 28); margin.add_theme_constant_override("margin_bottom", 28); add_child(margin)
	var root := VBoxContainer.new(); root.name = "StrategicCommand"; root.add_theme_constant_override("separation", Tokens.SPACE_LG); margin.add_child(root)
	var header := HeaderComponent.new(); header.title = "STRATEGIC COMMAND"; header.detail = "THEATER REINFORCEMENT & DEPLOYMENT"; root.add_child(header)
	var tabs := TabsComponent.new(); tabs.set_tabs(PackedStringArray(["AIR", "GROUND", "NAVAL", "LOGISTICS", "RESEARCH"])); root.add_child(tabs)
	var cards := HBoxContainer.new(); cards.add_theme_constant_override("separation", Tokens.SPACE_MD); root.add_child(cards)
	for entry in [["T1 INTERCEPTOR", "Air Superiority Fighter", false, false], ["T2 STRIKE FIGHTER", "Precision Strike", true, false], ["T3 STRATEGIC RECON", "Long-Range Reconnaissance", false, true]]:
		var card := CardComponent.new(); card.title = entry[0]; card.subtitle = entry[1]; card.selected = entry[2]; card.disabled = entry[3]; card.custom_minimum_size = Vector2(230, 112); cards.add_child(card)
		var cost := CostComponent.new(); cost.insufficient = entry[3]; card.get_child(0).add_child(cost)
		var badge := BadgeComponent.new(); badge.text = "RESEARCH REQUIRED" if entry[3] else "AVAILABLE"; badge.state = "locked" if entry[3] else "available"; card.get_child(0).add_child(badge)
	var actions := HBoxContainer.new(); actions.add_theme_constant_override("separation", Tokens.SPACE_SM); root.add_child(actions)
	_select_zone_button = ButtonComponent.new(); _select_zone_button.text = "SELECT AIRFIELD"; _select_zone_button.variant = "secondary"; _select_zone_button.pressed.connect(select_delivery_zone.emit); actions.add_child(_select_zone_button)
	_request_button = ButtonComponent.new(); _request_button.text = "REQUEST PACKAGE"; _request_button.variant = "primary"; _request_button.pressed.connect(request_package.emit); actions.add_child(_request_button)
	var cancel_button := ButtonComponent.new(); cancel_button.text = "CLOSE"; cancel_button.variant = "danger"; cancel_button.pressed.connect(dismissed.emit); actions.add_child(cancel_button)
	var locked_button := ButtonComponent.new(); locked_button.text = "RESEARCH LOCKED"; locked_button.variant = "ghost"; locked_button.disabled = true; actions.add_child(locked_button)
	_status_label = Label.new(); _status_label.name = "DeliveryStatus"; _status_label.theme_type_variation = "StatLabel"; root.add_child(_status_label)
	var tooltip := TooltipComponent.new(); tooltip.custom_minimum_size.x = 360; tooltip.set_content("DELIVERY WINDOW", "Packages arrive through a friendly airfield. With one airfield, it is selected automatically; with several, select the desired airfield on the map. Resource costs are validated by the native simulation before a request begins."); root.add_child(tooltip)
	set_delivery_state(0, "DELIVERY ACCESS UNAVAILABLE")


func set_delivery_state(delivery_state: int, status: String) -> void:
	if not is_instance_valid(_request_button):
		return
	_select_zone_button.visible = delivery_state == 1
	_select_zone_button.disabled = delivery_state != 1
	_request_button.disabled = delivery_state != 2
	_status_label.text = status.to_upper()
