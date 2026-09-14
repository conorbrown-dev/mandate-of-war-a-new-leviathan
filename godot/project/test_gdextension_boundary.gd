extends SceneTree

func check(condition: bool, message: String, failures: Array[String]) -> void:
	if not condition:
		failures.append(message)


func _init() -> void:
	var failures: Array[String] = []
	OS.set_environment("RTS_DATA_ROOT", ProjectSettings.globalize_path("res://../../data").simplify_path())
	check(ClassDB.class_exists("RtsExtension"), "RtsExtension is not registered", failures)
	if failures.is_empty():
		var bridge: Object = ClassDB.instantiate("RtsExtension")
		check(bridge != null, "RtsExtension could not be instantiated", failures)
		if bridge != null:
			# The retained API is presentation-owned: lifecycle, batched state,
			# authoritative commands, and aggregate skirmish data.
			for method in [
				"start_simulation", "update_simulation", "get_unit_position", "get_unit_transforms", "get_unpresented_entities",
				"issue_move_commands", "queue_faction_structure", "get_build_catalog", "get_hud_state", "skirmish_state",
				"get_road_segments", "territory_get_installation_info"
			]:
				check(bridge.has_method(method), "required bridge method missing: %s" % method, failures)
			# These had no Godot consumer. Their removal prevents presentation code
			# from depending on test-only, duplicate, or CPU-renderer implementation details.
			for method in [
				"ai_reset", "economy_get_resource_node_info", "economy_get_queue_size",
				"economy_get_completed_build_count", "logistics_carrier_deck_occupancy",
				"logistics_takeoff_queue_size", "logistics_landing_queue_size",
				"logistics_active_runway_operations", "logistics_is_safe_return",
				"logistics_get_intelligence_age", "logistics_is_intelligence_stale",
				"skirmish_position_visible", "render_add_unit", "render_update",
				"render_get_instance_count", "set_debug_mode", "get_unit_x", "get_unit_y",
				"get_unit_positions", "get_unit_headings", "move_unit", "initialize_faction",
				"economy_add_extractor", "economy_enqueue_construction", "economy_update_all",
				"queue_structure", "get_faction_production_line", "destroy_unit",
				"issue_patrol_commands", "issue_return_commands", "issue_defend_commands",
				"get_simulation_tick_ms", "get_unit_off_road_state", "get_unit_is_dead",
				"apply_damage", "get_unit_faction_id", "ai_init",
				"ai_update", "ai_set_faction_id", "ai_get_visible_unit_count", "ai_get_enemy_unit_count",
				"get_production_queue"
			]:
				check(not bridge.has_method(method), "retired bridge method is still public: %s" % method, failures)
			bridge.call("start_simulation")
			var entity_id: int = bridge.call("create_unit", 1.0, 1.0)
			var hud_state: Dictionary = bridge.call("get_hud_state", 0, -1, entity_id, 0.0, 0.0, false)
			check(hud_state.has("tick_ms"), "HUD aggregate is missing tick timing", failures)
			check(hud_state.has("storage") and hud_state.has("production_queue"), "HUD aggregate is missing economy state", failures)
			check(hud_state.has("selected_health") and hud_state.has("selected_off_road"), "HUD aggregate is missing selected-unit state", failures)
			check(hud_state.has("build_catalog") and hud_state.has("fob_installation"), "HUD aggregate is missing presentation state", failures)
			var transforms: PackedFloat32Array = bridge.call("get_unit_transforms", PackedInt32Array([entity_id]))
			check(transforms.size() == 3 and is_equal_approx(transforms[0], 1.0) and is_equal_approx(transforms[1], 1.0), "transform snapshot must include position and heading", failures)
			var position: PackedFloat32Array = bridge.call("get_unit_position", entity_id)
			check(position.size() == 2 and is_equal_approx(position[0], 1.0) and is_equal_approx(position[1], 1.0), "single-unit position snapshot must include both coordinates", failures)
			var unpresented: Array = bridge.call("get_unpresented_entities", PackedInt32Array())
			check(unpresented.size() == 1 and int(unpresented[0].get("id", -1)) == entity_id, "entity snapshot must expose unpresented native entities", failures)
			check(bridge.call("get_unpresented_entities", PackedInt32Array([entity_id])).is_empty(), "entity snapshot must omit already-presented entities", failures)
			bridge.call("stop_simulation")
	if not failures.is_empty():
		for failure in failures:
			push_error(failure)
		quit(1)
		return
	print("GDEXTENSION_BOUNDARY checks=61 failures=0")
	quit(0)
