extends SceneTree

const SkirmishConfigLoader := preload("res://skirmish_config.gd")

func _init() -> void:
	var scenario_result: Dictionary = SkirmishConfigLoader.load_definition("res://scenarios/two_landmass_skirmish.json")
	if not scenario_result.get("ok", false):
		push_error("Goal 08 scenario definition failed validation: %s" % scenario_result.get("error", "unknown error"))
		quit(1)
		return
	var scenario_definition: Dictionary = scenario_result.definition
	if scenario_definition.id != "two_landmass_skirmish_v1":
		push_error("Goal 08 scenario definition returned the wrong id")
		quit(1)
		return
	var invalid_definition: Dictionary = scenario_definition.duplicate(true)
	invalid_definition.ai.faction_id = invalid_definition.player.faction_id
	if SkirmishConfigLoader.validate_definition(invalid_definition) == "":
		push_error("Goal 08 scenario validation accepted identical player and AI factions")
		quit(1)
		return
	invalid_definition = scenario_definition.duplicate(true)
	invalid_definition.player.spawn = [0, 0]
	if SkirmishConfigLoader.validate_definition(invalid_definition) == "":
		push_error("Goal 08 scenario validation accepted a player spawn in the sea")
		quit(1)
		return
	invalid_definition = scenario_definition.duplicate(true)
	invalid_definition.player.units[0].unit_type = 3
	if SkirmishConfigLoader.validate_definition(invalid_definition) == "":
		push_error("Goal 08 scenario validation accepted an enemy-faction unit in the player force")
		quit(1)
		return
	invalid_definition = scenario_definition.duplicate(true)
	invalid_definition.ai.spawn = [-105, 0]
	if SkirmishConfigLoader.validate_definition(invalid_definition) == "":
		push_error("Goal 08 scenario validation accepted both sides on one landmass")
		quit(1)
		return
	invalid_definition = scenario_definition.duplicate(true)
	invalid_definition.theater.landmasses[1].id = invalid_definition.theater.landmasses[0].id
	if SkirmishConfigLoader.validate_definition(invalid_definition) == "":
		push_error("Goal 08 scenario validation accepted duplicate landmass ids")
		quit(1)
		return
	invalid_definition = scenario_definition.duplicate(true)
	invalid_definition.theater.landmasses[0].center = [-150, 0]
	if SkirmishConfigLoader.validate_definition(invalid_definition) == "":
		push_error("Goal 08 scenario validation accepted a landmass outside the theater")
		quit(1)
		return

	if not ClassDB.class_exists("RtsExtension"):
		push_error("RtsExtension is not registered")
		quit(1)
		return

	var extension: Object = ClassDB.instantiate("RtsExtension")
	if extension == null:
		push_error("RtsExtension could not be instantiated")
		quit(1)
		return

	# Test 1: Basic simulation
	extension.call("start_simulation")
	var entity_id: int = extension.call("create_unit", 2.0, 3.0)
	var count: int = extension.call("get_entity_count")
	var move_ids := PackedInt32Array([entity_id])
	var accepted_moves: int = extension.call("issue_move_commands", move_ids, 0, 8.0, 3.0, 3.0)
	for _tick in range(10):
		extension.call("update_simulation", 50.0)
	var positions: PackedFloat32Array = extension.call("get_unit_positions", move_ids)
	var x: float = positions[0] if positions.size() == 2 else NAN
	var y: float = positions[1] if positions.size() == 2 else NAN
	var second_move_count: int = extension.call("issue_move_commands", move_ids, 0, 30.0, 3.0, 3.0)
	extension.call("update_simulation", 50.0)
	var stop_count: int = extension.call("issue_stop_commands", move_ids, 0)
	extension.call("update_simulation", 50.0)
	var stopped_x: float = extension.call("get_unit_x", entity_id)
	for _tick in range(3):
		extension.call("update_simulation", 50.0)
	var after_stop_x: float = extension.call("get_unit_x", entity_id)
	var tick_ms: float = extension.call("get_simulation_tick_ms")
	extension.call("stop_simulation")

	if count != 1:
		push_error("count check failed: %d" % count)
		quit(1)
		return
	if accepted_moves != 1:
		push_error("accepted_moves check failed: %d" % accepted_moves)
		quit(1)
		return
	if second_move_count != 1:
		push_error("second_move_count check failed: %d" % second_move_count)
		quit(1)
		return
	if stop_count != 1:
		push_error("stop_count check failed: %d" % stop_count)
		quit(1)
		return
	if not is_equal_approx(x, 8.0):
		push_error("x check failed: %f" % x)
		quit(1)
		return
	if not is_equal_approx(y, 3.0):
		push_error("y check failed: %f" % y)
		quit(1)
		return
	if not is_equal_approx(stopped_x, after_stop_x):
		push_error("stopped_x check failed: %f vs %f" % [stopped_x, after_stop_x])
		quit(1)
		return
	if tick_ms < 0.0:
		push_error("tick_ms check failed: %f" % tick_ms)
		quit(1)
		return
	
	extension.call("stop_simulation")
	
	# Test 2: Reset and verify fresh world
	extension.call("reset_simulation")
	var count_after_reset: int = extension.call("get_entity_count")
	extension.call("start_simulation")
	var new_entity_id: int = extension.call("create_unit", 10.0, 20.0)
	extension.call("update_simulation", 50.0)
	var new_x: float = extension.call("get_unit_x", new_entity_id)
	var new_y: float = extension.call("get_unit_y", new_entity_id)

	if count_after_reset != 0 or new_x != 10.0 or new_y != 20.0:
		push_error("RtsExtension reset test failed: count_after_reset=%d new_x=%f new_y=%f" % [count_after_reset, new_x, new_y])
		quit(1)
		return

	# Test 3: Health visibility (Goal 08-COMBAT)
	var health_result: Array = extension.call("get_unit_health", new_entity_id)
	if health_result.size() != 2:
		push_error("Health getter failed: expected array of size 2, got %d" % health_result.size())
		quit(1)
		return
	var current_health: float = health_result[0]
	var max_health: float = health_result[1]
	if not is_equal_approx(current_health, 100.0):
		push_error("Initial current health check failed: %f" % current_health)
		quit(1)
		return
	if not is_equal_approx(max_health, 100.0):
		push_error("Initial max health check failed: %f" % max_health)
		quit(1)
		return
	var is_dead: bool = extension.call("get_unit_is_dead", new_entity_id)
	if is_dead:
		push_error("Fresh unit should not be dead")
		quit(1)
		return
	
	var damage_result: bool = extension.call("apply_damage", new_entity_id, 150.0)
	if not damage_result:
		push_error("apply_damage returned false for valid target")
		quit(1)
		return
	extension.call("update_simulation", 50.0)
	is_dead = extension.call("get_unit_is_dead", new_entity_id)
	if not is_dead:
		push_error("Unit with 150 damage to 100 max health should be dead")
		quit(1)
		return
	
	health_result = extension.call("get_unit_health", new_entity_id)
	current_health = health_result[0] if health_result.size() == 2 else -1.0
	if current_health >= 0.0:
		push_error("Dead unit health getter should return empty array, got current=%f" % current_health)
		quit(1)
		return
	
	extension.call("stop_simulation")
	
	print("RtsExtension smoke test passed")
	quit(0)
