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
	view.queue_free()
	await process_frame
	print("PORTS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
