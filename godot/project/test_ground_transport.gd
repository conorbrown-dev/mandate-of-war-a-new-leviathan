extends SceneTree

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(reason)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	await process_frame
	var native: Object = view.extension
	native.call("skirmish_set_ai_enabled", false)
	var road: Dictionary = native.call("transport_route", 0, 1)
	var rail: Dictionary = native.call("transport_route", 0, 2)
	check(bool(road.get("enabled", false)) and bool(rail.get("enabled", false)), "Broken Strait configures both player transport corridors")
	check(view.transport_corridor_views.size() == 4, "both factions have drawn road and rail corridors")
	var rails: Node3D = view.transport_corridor_views.get("0:2")
	check(rails != null and rails.get_child_count() == 5 and rails.get_node_or_null("RailTies") != null,
		"rail corridor has ballast, sleepers, and two steel rails")
	check(not view.transport_corridor_views["1:1"].visible and not view.transport_corridor_views["1:2"].visible,
		"enemy transport corridors stay hidden until scouted")
	var enemy_road: Dictionary = native.call("transport_route", 1, 1)
	var enemy_terminal: Vector2 = enemy_road.get("terminal", Vector2.ZERO)
	var scout := int(native.call("create_unit_with_type", enemy_terminal.x, enemy_terminal.y, 13, 0))
	for _tick in range(20):
		native.call("update_simulation", 50.0)
	view._sync_vision()
	check(scout > 0 and view.transport_corridor_views["1:1"].visible,
		"a scout reveals the enemy road terminal and corridor")
	native.call("destroy_unit", scout)
	for _tick in range(10):
		native.call("update_simulation", 50.0)
	view._sync_vision()
	check(view.transport_corridor_views["1:1"].visible,
		"the scouted enemy road stays visible after the scout leaves")
	view._select_command_post()
	var panel: Control = view.strategic_command_view
	check(panel != null and panel.visible, "clicking the command post opens Strategic Command")
	panel._select_ground_transport(1)
	var card: Node = panel._cards.get_node_or_null("Card_0")
	check(card != null and not card.get_node("Order").disabled, "road transport is orderable before an airfield exists")
	var post := int(view.commander_ids.get(0, -1))
	var before_materials: Array = native.call("economy_get_tactical_resources", post)
	view._on_strategic_command_order_unit(0, 1)
	panel._select_ground_transport(2)
	card = panel._cards.get_node_or_null("Card_0")
	check(card != null and not card.get_node("Order").disabled, "rail transport is orderable before an airfield exists")
	view._on_strategic_command_order_unit(0, 2)
	native.call("update_simulation", 50.0)
	var inbound: Array = native.call("delivery_state", 0).get("inbound", [])
	check(inbound.size() == 2 and int(inbound[0].route_mode) == 2 and int(inbound[1].route_mode) == 1,
		"the native queue contains a faster rail delivery and a road delivery")
	var after_materials: Array = native.call("economy_get_tactical_resources", post)
	check(float(after_materials[0]) < float(before_materials[0]), "both ground transport orders are paid")
	view._close_strategic_command()
	for _tick in range(500):
		native.call("update_simulation", 50.0)
	view._sync_ground_transport_presentation()
	check(view.ground_transport_views.size() == 2, "a truck and train travel along their visible corridors")
	var rail_seen := false
	var road_seen := false
	for convoy in view.ground_transport_views.values():
		rail_seen = rail_seen or convoy.name == "RailConvoy"
		road_seen = road_seen or convoy.name == "RoadConvoy"
	check(rail_seen and road_seen, "the two modes have distinct convoy presentations")
	var starting_units := (native.call("get_entity_ids") as PackedInt32Array).size()
	for _tick in range(5000):
		if (native.call("delivery_state", 0).get("inbound", []) as Array).is_empty():
			break
		native.call("update_simulation", 50.0)
	view._sync_ground_transport_presentation()
	check((native.call("delivery_state", 0).get("inbound", []) as Array).is_empty(), "both deliveries unload at their terminals")
	check((native.call("get_entity_ids") as PackedInt32Array).size() == starting_units + 2, "both delivered units are playable ground forces")
	check(view.ground_transport_views.is_empty(), "convoy visuals clear after unloading")
	view.queue_free()
	await process_frame
	print("GROUND_TRANSPORT checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
