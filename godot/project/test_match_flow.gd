extends SceneTree

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _start_match() -> Node:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	await process_frame
	return view


# Reads the label on screen, not a stored value, so a stale header would fail.
func _title_path(view: Node) -> NodePath:
	return view.match_end_panel.get_path_to(view.match_end_panel.find_child("MatchTitle", true, false))


func _finish(view: Node) -> void:
	view.queue_free()
	await process_frame


# Runs the fixed tick the way the game does, without waiting on real frames.
func _tick(view: Node, ticks: int) -> void:
	for _tick_index in range(ticks):
		if view.match_finished:
			return
		view.extension.call("update_simulation", 50.0)
		view._update_match_state()


const RECORD_DIR := "user://test_match_records"


func _read_json(path: String) -> Variant:
	var file := FileAccess.open(path, FileAccess.READ)
	return null if file == null else JSON.parse_string(file.get_as_text())


func _test_victory() -> void:
	OS.set_environment("RTS_MATCH_RECORD_DIR", RECORD_DIR)
	var view: Node = await _start_match()
	check(String(view.ai_style_state.get("doctrine", "")) == "balanced" and String(view.ai_style_state.get("difficulty", "")) == "normal", "the scenario's AI style is applied")
	check(bool(view.extension.call("match_record_state").recording), "the match is being recorded")
	var spawn: Array = view.scenario_definition.ai.spawn
	var player: Array = view.scenario_definition.player.spawn
	var post := Vector2(float(spawn[0]), float(spawn[1]))
	var start := Vector2(float(player[0]), float(player[1]))
	check(bool(view.extension.call("is_land_position", post.x, post.y)) and bool(view.extension.call("is_land_position", start.x, start.y)), "both command posts stand on land")
	check(start.distance_to(post) < 8000.0, "the two sides start within 8 km of each other")
	check(view.command_post_views.size() == 2 and view.command_post_views[0].visible, "the player has a visible command post")
	check(not view.command_post_views[1].visible and not view.enemy_post_discovered, "the enemy command post starts hidden")
	check(view.seen_enemy_ids.is_empty(), "no enemy is known at the start")
	check(float(view.fog_view.material_override.get_shader_parameter("world_size").x) > 0.0, "the fog pass is running")
	check(not String(view.scenario_definition.description).contains("7 km"), "the briefing does not give away the enemy position")
	var enemy_post_id := int(view.commander_ids.get(1, -1))
	view.extension.call("update_simulation", 50.0)
	var enemy_post_position: PackedFloat32Array = view.extension.call("get_unit_position", enemy_post_id)
	check(Vector2(enemy_post_position[0], enemy_post_position[1]).distance_to(post) < 5.0, "the enemy command post stays where it was placed")
	check(String(view.control_point_battle_status).contains("COMMAND POST"), "the HUD states the objective")
	# The AI stays out of this match so the route and the win are measured alone.
	view.extension.call("skirmish_set_ai_enabled", false)
	var scout: int = view.extension.call("create_unit_with_type", start.x + 300.0, start.y - 300.0, 0, 0)
	var ordered: int = view.extension.call("issue_move_commands", PackedInt32Array([scout]), 0, post.x + 400.0, post.y + 400.0, 8.0)
	check(ordered == 1, "a tank accepts an order toward the enemy")
	var ticks_to_reach := -1
	for tick in range(20 * 420):
		view.extension.call("update_simulation", 50.0)
		var position: PackedFloat32Array = view.extension.call("get_unit_position", scout)
		if position.size() >= 2 and Vector2(position[0], position[1]).distance_to(post) < 1200.0:
			ticks_to_reach = tick
			break
	check(ticks_to_reach > 0, "a tank reaches the enemy by ground within seven minutes")
	print("MATCH_FLOW tank_reached_enemy_seconds=%.0f" % (ticks_to_reach / 20.0))
	var strike := PackedInt32Array()
	for index in range(12):
		var id: int = view.extension.call("create_unit_with_type", post.x + 40.0 + index * 6.0, post.y + 40.0, 0, 0)
		check(id > 0, "strike force unit %d is created" % index)
		strike.append(id)
	var enemy_post := int(view.commander_ids.get(1, -1))
	check(view._is_unit_shown(view.engineer_ids.get(0, -1)), "friendly units are always shown")
	check(not view._is_unit_shown(int(view.engineer_ids.get(1, -1))), "an unwatched enemy engineer stays hidden")
	for _vision_tick in range(10):
		view.extension.call("update_simulation", 50.0)
	view._sync_vision()
	check(view.enemy_post_discovered and view.command_post_views[1].visible, "units near the enemy command post discover it")
	check(view.seen_enemy_ids.has(enemy_post), "the discovered command post counts as seen")
	var attacking: int = view.extension.call("issue_attack_commands", strike, 0, enemy_post)
	print("MATCH_FLOW attackers=%d of %d" % [attacking, strike.size()])
	check(attacking > 0, "the strike force accepts the attack on the command post")
	_tick(view, 20 * 120)
	var state: Dictionary = view.extension.call("match_state")
	check(int(state.result) == 0, "destroying the enemy command post wins the match")
	check(view.match_finished and view.match_end_panel != null and view.match_end_panel.visible, "the end panel appears")
	if view.match_end_panel != null:
		check(view.match_end_panel.get_node(_title_path(view)).text == "VICTORY", "the end panel reports victory")
		check(view.match_end_panel._detail.text.contains("MATCH TIME"), "the end panel reports the match time")
		check(view.match_end_panel._detail.text.contains("ENEMY COMMANDER  BALANCED  //  NORMAL"), "the end panel names the enemy's style")
	check(view.match_record_path.begins_with(RECORD_DIR) and FileAccess.file_exists(view.match_record_path), "the finished match is saved")
	var record: Variant = _read_json(view.match_record_path)
	check(typeof(record) == TYPE_DICTIONARY, "the saved record is JSON")
	if typeof(record) == TYPE_DICTIONARY:
		check(int(record.format_version) == 1 and bool(record.finished) and int(record.result) == 0, "the record holds the result")
		check(String(record.info.get("scenario", "")) == String(view.scenario_definition.id), "the record names the scenario")
		check(String(record.info.get("ai_doctrine", "")) == "balanced" and String(record.info.get("ai_difficulty", "")) == "normal", "the record names the AI style")
		var player_orders := 0
		for command in record.commands:
			player_orders += 1 if int(command[1]) == 0 else 0
		check(player_orders >= strike.size(), "the player's orders are recorded")
		var created := 0
		for action in record.actions:
			created += 1 if String(action.name) == "create_unit_with_type" else 0
		check(created == strike.size() + 1, "units added mid-match are recorded as actions")
		check(record.state_hashes.size() > 100 and record.samples.size() > 10, "state hashes and samples are recorded")
	DirAccess.remove_absolute(view.match_record_path)
	var frozen: int = int(view.extension.call("get_entity_count"))
	view._process(0.05)
	check(int(view.extension.call("get_entity_count")) == frozen, "a finished match stops ticking")
	await _finish(view)


func _test_defeat() -> void:
	var view: Node = await _start_match()
	var first_wave_seconds := -1.0
	var first_contact_seconds := -1.0
	var player_post := int(view.commander_ids.get(0, -1))
	var full_health := float(Array(view.extension.call("get_unit_health", player_post))[0])
	for second in range(60 * 30):
		_tick(view, 20)
		if view.match_finished:
			break
		var state: Dictionary = view.extension.call("match_state")
		if first_wave_seconds < 0.0 and int(state.waves) > 0:
			first_wave_seconds = float(second)
		if first_contact_seconds < 0.0 and float(Array(view.extension.call("get_unit_health", player_post))[0]) < full_health:
			first_contact_seconds = float(second)
	var final_state: Dictionary = view.extension.call("match_state")
	print("MATCH_FLOW first_wave_seconds=%.0f first_contact_seconds=%.0f defeat_seconds=%.0f waves=%d" % [first_wave_seconds, first_contact_seconds, float(final_state.seconds), int(final_state.waves)])
	check(first_wave_seconds >= 0.0 and first_wave_seconds < 120.0, "the first wave leaves within two minutes")
	# The AI knows only the briefing sector, so it has to find the player with
	# scouts and recon flights first. Measured 2026-11-07: it sights the post at
	# about 17 minutes and its first wave hits at about 22.
	check(first_contact_seconds >= 0.0 and first_contact_seconds < 1500.0, "the AI finds and reaches an idle player within twenty-five minutes")
	check(int(final_state.result) == 1, "an idle player loses the match")
	check(view.match_finished and view.match_end_panel != null and view.match_end_panel.get_node(_title_path(view)).text == "DEFEAT", "the end panel reports defeat")
	await _finish(view)


# Forces land only at an airfield, so the AI has to build one, as the player does.
func _test_ai_builds_its_airfield() -> void:
	var view: Node = await _start_match()
	var at_start: Dictionary = view.extension.call("delivery_state", 1)
	check(bool(at_start.get("enabled", false)) and not bool(at_start.get("has_site", true)), "the AI starts with nowhere to land forces: no free landing zone")
	var built_seconds := -1.0
	for second in range(240):
		_tick(view, 20)
		if bool(view.extension.call("delivery_state", 1).get("has_site", false)):
			built_seconds = float(second + 1)
			break
	print("MATCH_FLOW ai_airfield_seconds=%.0f" % built_seconds)
	check(built_seconds > 0.0, "the AI builds its own airfield within four minutes")
	var record: Variant = JSON.parse_string(String(view.extension.call("match_record_json")))
	var engineer_orders := 0
	if typeof(record) == TYPE_DICTIONARY:
		for command in record.commands:
			if int(command[1]) == 1 and int(command[2]) == 3 and (int(command[6]) & 0x40000000) != 0 and int(command[3]) == int(view.engineer_ids.get(1, -1)):
				engineer_orders += 1
	check(engineer_orders == 1, "its Field Engineer gave the order, and the record holds it")
	view._sync_vision()
	check(view.enemy_airfield_views.is_empty(), "an enemy airfield nobody has seen is not drawn")
	var site: Dictionary = view.extension.call("delivery_state", 1)
	var landing := Vector2(float(site.get("x", 0.0)), float(site.get("y", 0.0)))
	var player: Array = view.scenario_definition.player.spawn
	var observer: int = view.extension.call("create_unit_with_type", landing.x + 30.0, landing.y, 0, 0)
	check(observer > 0, "a player unit reaches the enemy landing site")
	for _vision_tick in range(10):
		view.extension.call("update_simulation", 50.0)
	view._sync_vision()
	check(view.enemy_airfield_views.size() == 1, "the enemy airfield is drawn once the player sees it")
	check(String(view.fuel_supply_status) == "ENEMY AIRFIELD LOCATED", "and the player is told")
	view.extension.call("issue_move_commands", PackedInt32Array([observer]), 0, float(player[0]), float(player[1]), 8.0)
	view.extension.call("skirmish_set_ai_enabled", false)
	for _away_tick in range(20 * 90):
		view.extension.call("update_simulation", 50.0)
	view._sync_vision()
	check(view.enemy_airfield_views.size() == 1, "it stays on the map after the player looks away")
	await _finish(view)


const SCOUT_CAR := 13
const CARGO_AIRCRAFT := 17


# A light unit dropped by parachute onto watched ground, with no airfield.
func _test_paradrop() -> void:
	var view: Node = await _start_match()
	view.extension.call("skirmish_set_ai_enabled", false)
	var funds_before: Array = view.extension.call("economy_get_tactical_resources", int(view.commander_ids.get(0, -1)))
	view._toggle_strategic_command()
	await process_frame
	var card: Node = view.strategic_command_view.find_child("Card_%d" % SCOUT_CAR, true, false)
	var order_button: Button = card.get_node_or_null("Order") if card != null else null
	var drop_button: Button = card.get_node_or_null("Paradrop") if card != null else null
	check(order_button != null and order_button.disabled, "with no airfield the scout car cannot be flown in to land")
	check(drop_button != null and not drop_button.disabled, "but its PARADROP button is open")
	var tank_card: Node = view.strategic_command_view.find_child("Card_0", true, false)
	check(tank_card != null and tank_card.get_node_or_null("Paradrop") == null, "a main battle tank has no PARADROP button")
	if drop_button != null:
		drop_button.pressed.emit()
	check(view.paradrop_targeting_type == SCOUT_CAR and not view.strategic_command_view.visible, "PARADROP closes the panel and waits for a drop zone")
	var enemy: Array = view.scenario_definition.ai.spawn
	check(not view._order_paradrop(Vector2(float(enemy[0]), float(enemy[1]))), "a drop onto ground nobody watches is refused")
	check(String(view.fuel_supply_status) == "PARADROP REFUSED // DROP ZONE NOT SCOUTED", "and the player is told why")
	check(view.paradrop_targeting_type == -1, "the refused click ends the targeting")
	var engineer := int(view.engineer_ids.get(0, -1))
	var engineer_at: PackedFloat32Array = view.extension.call("get_unit_position", engineer)
	var zone := Vector2(engineer_at[0] + 60.0, engineer_at[1] + 60.0)
	check(bool(view.extension.call("can_see", 0, zone.x, zone.y)), "the engineer watches the chosen drop zone")
	view._begin_paradrop_targeting(SCOUT_CAR)
	check(view._order_paradrop(zone), "a drop onto watched ground is accepted")
	check(String(view.fuel_supply_status) == "PARADROP INBOUND", "and the player is told it is inbound")
	view.extension.call("update_simulation", 50.0)
	var inbound: Array = view.extension.call("delivery_state", 0).get("inbound", [])
	check(inbound.size() == 1 and bool(inbound[0].get("airdrop", false)) and Vector2(float(inbound[0].x), float(inbound[0].y)).distance_to(zone) < 2.0, "the drop is in the air for that zone")
	var funds_after: Array = view.extension.call("economy_get_tactical_resources", int(view.commander_ids.get(0, -1)))
	check(float(funds_after[0]) < float(funds_before[0]), "and was paid for")
	view._update_hud()
	check(view.paradrop_markers.size() == 1, "the drop zone is marked on the map")
	var known: PackedInt32Array = view.extension.call("get_entity_ids")
	var landed: Dictionary = {}
	var transport_seen := false
	for _second in range(150):
		_tick(view, 20)
		for entity in view.extension.call("get_unpresented_entities", known):
			if int(entity.unit_type) == SCOUT_CAR and int(entity.faction_id) == 0:
				landed = entity
			if int(entity.unit_type) == CARGO_AIRCRAFT and int(entity.faction_id) == 0:
				transport_seen = true
		if not landed.is_empty():
			break
	check(transport_seen, "the drop flies in on a cargo aircraft")
	check(not landed.is_empty(), "the scout car lands within two and a half minutes")
	if not landed.is_empty():
		check(Vector2(float(landed.x), float(landed.y)).distance_to(zone) <= 151.0, "around the drop zone")
	view._update_hud()
	check(view.paradrop_markers.is_empty(), "the marker goes once it has landed")
	check(not bool(view.extension.call("delivery_state", 0).get("has_site", true)), "all without an airfield")
	# A second drop over enemy surface-to-air missiles under its route.
	for index in range(8):
		var sam: int = view.extension.call("create_unit_with_type", -17000.0 + index * 60.0, zone.y + 30.0, 5, 1)
		view.extension.call("issue_stop_commands", PackedInt32Array([sam]), 1)
	view._begin_paradrop_targeting(SCOUT_CAR)
	check(view._order_paradrop(zone), "a second drop is ordered")
	for _second in range(150):
		_tick(view, 20)
		if (view.extension.call("delivery_state", 0).get("inbound", []) as Array).is_empty():
			break
	view._update_hud()
	check(int(view.extension.call("delivery_state", 0).get("airdrops_lost", 0)) == 1, "its transport is shot down over the enemy missiles")
	check(String(view.fuel_supply_status) == "PARADROP TRANSPORT SHOT DOWN", "and the player is told")
	await _finish(view)


func _style_for(doctrine: String, difficulty: String) -> Dictionary:
	OS.set_environment("RTS_AI_DOCTRINE", doctrine)
	OS.set_environment("RTS_AI_DIFFICULTY", difficulty)
	var view: Node = await _start_match()
	var style: Dictionary = view.extension.call("ai_style")
	var record: Variant = JSON.parse_string(String(view.extension.call("match_record_json")))
	style["recorded"] = String(record.info.get("ai_doctrine", "")) + "/" + String(record.info.get("ai_difficulty", ""))
	style["panel"] = String(view.ai_style_state.get("doctrine", ""))
	await _finish(view)
	return style


func _test_ai_style_choice() -> void:
	var chosen := await _style_for("turtle", "hard")
	check(String(chosen.doctrine) == "turtle" and String(chosen.difficulty) == "hard", "RTS_AI_DOCTRINE and RTS_AI_DIFFICULTY choose the enemy's style")
	check(String(chosen.recorded) == "turtle/hard" and String(chosen.panel) == "turtle", "the record and the HUD agree on the style")
	var unknown := await _style_for("lavish", "hard")
	check(String(unknown.doctrine) == "balanced" and String(unknown.difficulty) == "normal", "an unknown style falls back to the defaults")
	OS.set_environment("RTS_AI_DOCTRINE", "")
	OS.set_environment("RTS_AI_DIFFICULTY", "")


func _run() -> void:
	await _test_ai_style_choice()
	await _test_ai_builds_its_airfield()
	await _test_paradrop()
	await _test_victory()
	await _test_defeat()
	print("GODOT_MATCH_FLOW checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
