extends SceneTree
## Ports and cargo ships on Broken Strait, through the real match scene: every
## port is on land with a sailable lane, a side ships in through a port it
## holds, and a sunk ship loses its load.
## Run: godot --headless --path godot/project --script res://test_ports.gd

var checks := 0
var failures := 0
const PLAYER := 0
const ENEMY := 1
const ELITE_MBT := 0
const MASS_SWARM_TANK := 3


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _tick(view: Node, ticks: int) -> void:
	for _tick_index in range(ticks):
		view.extension.call("update_simulation", 50.0)
		view._update_match_state()


func _entities_of_type(view: Node, unit_type: int) -> Array:
	var found: Array = []
	for entity: Dictionary in view.extension.call("get_unpresented_entities", PackedInt32Array()):
		if int(entity.unit_type) == unit_type:
			found.append(entity)
	return found


func _ships(view: Node, unit_type: int) -> Array:
	var ids: Array = []
	for entity: Dictionary in _entities_of_type(view, unit_type):
		ids.append(int(entity.id))
	return ids


func _units_near(view: Node, unit_type: int, point: Vector2, radius: float) -> int:
	var count := 0
	for entity: Dictionary in _entities_of_type(view, unit_type):
		if Vector2(float(entity.x), float(entity.y)).distance_to(point) <= radius:
			count += 1
	return count


func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	view.set_process(false)
	await process_frame
	var native: Object = view.extension
	native.call("skirmish_set_ai_enabled", false)
	check(view.ports_view != null and view.ports_view.active, "Broken Strait has ports")
	var states: Array = native.call("ports_state")
	check(states.size() == 5, "five ports are in the simulation")
	for state: Dictionary in states:
		check(bool(native.call("is_land_position", float(state.x), float(state.y))), "port %d has its quay on land" % int(state.id))
	check(int(states[3].owner) == PLAYER and int(states[4].owner) == ENEMY, "each side starts with one port")
	for state: Dictionary in states:
		check(float(state.hp) == float(state.max_hp) and not bool(state.destroyed), "port %d starts undamaged" % int(state.id))
	# The player's panel offers sea delivery, and says why when it cannot.
	var sea_reason := ""
	var player_catalog: Array = native.call("get_build_catalog", PLAYER)
	for entry: Dictionary in player_catalog:
		if int(entry.get("type", -1)) == ELITE_MBT:
			sea_reason = String(entry.get("sea_reason", "?"))
	check(sea_reason.is_empty(), "the player can ship a tank by sea (reason: '%s')" % sea_reason)

	# The player orders a tank by sea; it is paid for, sails and lands at the quay.
	var player_quay := Vector2(float(states[3].x), float(states[3].y))
	var tanks_before := _units_near(view, ELITE_MBT, player_quay, 300.0)
	check(bool(native.call("order_ground_transport", PLAYER, ELITE_MBT, 3)), "the sea order is accepted")
	_tick(view, 20 * 25)
	var ships := _ships(view, 21)
	check(ships.size() == 1, "a cargo ship is on the water (%d)" % ships.size())
	_tick(view, 20 * 420)
	check(_ships(view, 21).is_empty(), "the ship has unloaded and left")
	check(_units_near(view, ELITE_MBT, player_quay, 300.0) == tanks_before + 1, "the tank is ashore beside the player's port")

	# The enemy's west-coast lane is the narrow one: its ship must still arrive.
	var enemy_quay := Vector2(float(states[4].x), float(states[4].y))
	var swarm_before := _units_near(view, MASS_SWARM_TANK, enemy_quay, 300.0)
	check(bool(native.call("order_ground_transport", ENEMY, MASS_SWARM_TANK, 3)), "the enemy's sea order is accepted")
	_tick(view, 20 * 420)
	check(_ships(view, 22).is_empty(), "the enemy's ship has unloaded and left")
	check(_units_near(view, MASS_SWARM_TANK, enemy_quay, 300.0) == swarm_before + 1, "the swarm tank is ashore at West Quay")

	# A neutral port cannot be shipped through until it is taken.
	# A ship that is sunk loses its load and the order is not refunded.
	check(bool(native.call("order_ground_transport", PLAYER, ELITE_MBT, 3)), "a second sea order is accepted")
	_tick(view, 20 * 25)
	ships = _ships(view, 21)
	check(ships.size() == 1, "the second ship sails")
	if ships.size() == 1:
		native.call("destroy_unit", ships[0])
	_tick(view, 20 * 10)
	check(int(native.call("delivery_state", PLAYER).get("ships_lost", 0)) == 1, "the sinking is counted")
	_tick(view, 20 * 420)
	check(_units_near(view, ELITE_MBT, player_quay, 300.0) == tanks_before + 1, "the sunk ship's tank never arrives")

	# Orders for one port share a ship: three tanks, one hull.
	var tanks_now := _units_near(view, ELITE_MBT, player_quay, 300.0)
	for _order in range(3):
		check(bool(native.call("order_ground_transport", PLAYER, ELITE_MBT, 3)), "a convoy order is accepted")
	_tick(view, 20 * 25)
	check(_ships(view, 21).size() == 1, "three orders sail in one cargo ship (%d)" % _ships(view, 21).size())
	_tick(view, 20 * 420)
	check(_units_near(view, ELITE_MBT, player_quay, 300.0) == tanks_now + 3, "all three tanks come ashore together")

	# Fuel by sea needs no airfield, only a held port.
	var catalog: Array = native.call("get_build_catalog", PLAYER)
	var warships := 0
	for entry: Dictionary in catalog:
		if bool(entry.get("is_warship", false)):
			warships += 1
			check(String(entry.get("naval_reason", "x")).is_empty(), "the player can order the %s" % String(entry.get("name", "")))
	check(warships >= 1, "Strategic Command lists a warship for the player")
	check(bool(native.call("fuel_shipment_request", PLAYER, true)), "a sea fuel shipment is accepted")
	check(_ships(view, 21).size() == 1, "a fuel ship is on the water")
	_tick(view, 20 * 420)
	check(String(native.call("fuel_supply_state", PLAYER).get("reason", "")) == "FUEL SHIPMENT DELIVERED", "the fuel arrives by sea")

	# A patrol boat is built at the port and launched at its berth.
	var boats_before := _entities_of_type(view, 11).size()
	check(bool(native.call("order_naval_unit", PLAYER, 11)), "a patrol boat is ordered")
	_tick(view, 20 * 25)
	check(_entities_of_type(view, 11).size() == boats_before + 1, "the patrol boat is afloat")

	# A held port can be shelled to rubble, and the player is told.
	var enemy_post := int(view.commander_ids.get(1, -1))
	check(enemy_post > 0, "the enemy commander exists")
	for index in range(6):
		var gun := int(native.call("create_unit_with_type", enemy_quay.x + 165.0 + float(index) * 4.0, enemy_quay.y + float(index) * 6.0, 1, PLAYER))
		check(gun > 0, "an artillery piece is placed")
	_tick(view, 20 * 200)
	var after: Array = native.call("ports_state")
	# The player cannot attack a port it cannot see; a scout that sees it makes it a target.
	var picked := int(native.call("port_attack_target", PLAYER, enemy_quay.x, enemy_quay.y))
	check(picked == -1 or bool(after[4].destroyed), "a port is only a target while the player can see it and it stands (%d)" % picked)
	check(bool(after[4].destroyed), "the shelled enemy port is rubble")
	check(int(after[4].owner) == ENEMY, "rubble still belongs to its owner")
	check(not bool(after[3].destroyed), "the player's own port stands")
	view.queue_free()
	await process_frame
	print("PORTS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
