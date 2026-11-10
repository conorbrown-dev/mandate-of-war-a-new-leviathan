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

func _run() -> void:
	var view: Node2D = load("res://native_skirmish.tscn").instantiate()
	root.add_child(view)
	await process_frame
	check(view.load_error.is_empty(), "native route loads the validated deterministic scenario")
	print("native state units=%d result=%d" % [view.state.get("units", []).size(), int(view.state.get("result", -2))])
	check(int(view.state.get("result", -2)) == -1 and view.state.get("units", []).size() >= 24, "native route enters the authored two-landmass match with fog-limited state")
	check(view.state.has("materials") and view.state.has("fuel") and view.state.has("power_reserve_mw") and not view.state.has("research"), "native route exposes canonical tactical Materials, Fuel, and Power without a Research balance")
	var resources: Array = view.state.get("resources", [])
	var markers: Array = view.last_resource_markers
	check(resources.size() == 2 and markers.size() == resources.size(), "native map draws every authored resource node")
	for resource: Dictionary in resources:
		var marker: Dictionary = {}
		for candidate: Dictionary in markers:
			if int(candidate.id) == int(resource.id):
				marker = candidate
				break
		var expected_kind := "materials" if int(resource.type) == 0 else "fuel" if int(resource.type) == 1 else "unknown"
		check(not marker.is_empty() and String(marker.kind) == expected_kind and
			(marker.point as Vector2).distance_to(view._world_to_screen(float(resource.x), float(resource.y))) < 0.01,
			"native resource %d has a correctly placed %s map marker" % [int(resource.id), expected_kind])
	check(markers.any(func(marker: Dictionary) -> bool: return String(marker.kind) == "materials") and
		markers.any(func(marker: Dictionary) -> bool: return String(marker.kind) == "fuel"),
		"native map distinguishes Materials from Fuel")
	var aircraft_id := 0
	var vessel_id := 0
	for unit in view.state.get("units", []):
		if String(unit.get("kind", "")) == "air":
			aircraft_id = int(unit.get("id", 0))
		if String(unit.get("kind", "")) == "sea":
			vessel_id = int(unit.get("id", 0))
	check(aircraft_id > 0 and vessel_id > 0, "native route exposes conventional aircraft and naval vessel telemetry")
	var first_elite := 0
	var first_enemy := 0
	for unit in view.state.get("units", []):
		if int(unit.get("faction", -1)) == 0 and not bool(unit.get("base", false)) and first_elite == 0:
			first_elite = int(unit.get("id", 0))
		if int(unit.get("faction", -1)) == 1 and first_enemy == 0:
			first_enemy = int(unit.get("id", 0))
	check(first_elite > 0 and view.state.get("units", []).any(func(unit): return int(unit.get("faction", -1)) == 0), "native route exposes the identified human force while respecting fog of war")
	# The opposing command center is intentionally not visible at setup, but
	# its deterministic ID is still a useful ownership rejection fixture.
	if first_enemy == 0:
		var all_ids: PackedInt32Array = view.extension.call("get_entity_ids")
		first_enemy = int(all_ids[all_ids.size() - 1]) if not all_ids.is_empty() else 2
	# The logistics checks below need the vessel afloat, so the AI army holds until they finish.
	view.extension.call("skirmish_set_ai_enabled", false)
	view.selected_ids = PackedInt32Array([first_elite])
	var accepted_move := int(view.extension.call("skirmish_command", 0, view.selected_ids, -80.0, 0.0, 0))
	var rejected_empty := int(view.extension.call("skirmish_command", 0, PackedInt32Array(), -80.0, 0.0, 0))
	check(accepted_move == 1 and rejected_empty == 0, "native route submits player commands through authoritative validation and rejects empty selections")
	check(int(view.extension.call("skirmish_command", 0, PackedInt32Array([aircraft_id]), -80.0, 0.0, 0)) == 1, "native route accepts a conventional aircraft sortie order")
	for _tick in range(60):
		view.extension.call("skirmish_update", 50.0)
	var aircraft_state: Dictionary = {}
	for unit in view.extension.call("skirmish_state").get("units", []):
		if int(unit.get("id", 0)) == aircraft_id:
			aircraft_state = unit
	check(int(aircraft_state.get("status", 0)) >= 2 and float(aircraft_state.get("fuel", 0.0)) < float(aircraft_state.get("max_fuel", 1.0)), "native route exposes airborne endurance telemetry")
	for _tick in range(360):
		view.extension.call("skirmish_update", 50.0)
	var vessel_state: Dictionary = {}
	for unit in view.extension.call("skirmish_state").get("units", []):
		if int(unit.get("id", 0)) == vessel_id:
			vessel_state = unit
	check(bool(vessel_state.get("stranded", false)), "native route exposes naval endurance exhaustion and stranding")
	var material_before_resupply := float(view.extension.call("skirmish_state").get("materials", 0.0))
	check(int(view.extension.call("skirmish_command", 5, PackedInt32Array([vessel_id]), 0.0, 0.0, 0)) == 1, "native route accepts paid naval resupply through the return command")
	for _tick in range(2):
		view.extension.call("skirmish_update", 50.0)
	for unit in view.extension.call("skirmish_state").get("units", []):
		if int(unit.get("id", 0)) == vessel_id:
			vessel_state = unit
	check(not bool(vessel_state.get("stranded", true)) and float(vessel_state.get("fuel", 0.0)) > 90.0, "native route restores naval endurance from its existing vessel fuel system")
	check(float(view.extension.call("skirmish_state").get("materials", material_before_resupply)) < material_before_resupply, "native route deducts finite material stock for naval resupply")
	view.extension.call("skirmish_set_ai_enabled", true)
	# Seed one authoritative last-known contact, then prove it ages into a
	# stale record without exposing a hidden live position.
	view.extension.call("skirmish_update_intelligence", first_enemy, 105.0, 0.0, int(view.extension.call("skirmish_state").get("tick", 0)))
	var intel_now: Dictionary = {}
	for record in view.extension.call("skirmish_state").get("intelligence", []):
		if int(record.get("id", 0)) == first_enemy:
			intel_now = record
	check(not intel_now.is_empty() and int(intel_now.get("age", 9999)) == 0 and not bool(intel_now.get("stale", true)), "native route exposes a current last-known intelligence contact")
	view.extension.call("skirmish_archive_intelligence", first_enemy)
	for _tick in range(130):
		view.extension.call("skirmish_update", 50.0)
	var intel_later: Dictionary = {}
	for record in view.extension.call("skirmish_state").get("intelligence", []):
		if int(record.get("id", 0)) == first_enemy:
			intel_later = record
	check(bool(intel_later.get("stale", false)) and int(intel_later.get("age", 0)) >= 120, "native route marks aged intelligence stale while retaining last-known coordinates")
	for _tick in range(20000):
		if int(view.state.get("result", -1)) >= 0:
			break
		view.extension.call("skirmish_update", 50.0)
		view.state = view.extension.call("skirmish_state")
	check(int(view.state.get("result", -1)) >= 0, "native route reaches a deterministic terminal result")
	view._complete_match()
	print("native final result=%d label=%s" % [int(view.state.get("result", -1)), view.result_label.text])
	var result := int(view.state.get("result", -1))
	check(view.result_panel.visible and result in [0, 1, 2] and ["VICTORY", "DEFEAT", "DRAW"][result] in view.result_label.text,
		"native route presents the result it reached after movement and combat")
	var replay_error := String(view.extension.call("skirmish_verify_replay", "user://matches/latest-g08.replay"))
	check(replay_error.is_empty(), "native route saves and verifies its completed replay under user storage")
	var summary: Dictionary = view.extension.call("skirmish_stats_summary")
	check(int(summary.get("total_matches", 0)) >= 1, "native route persists the completed match in historical stats")
	view._rematch()
	check(view.load_error.is_empty() and int(view.state.get("result", -2)) == -1 and not view.result_panel.visible, "native route rematch starts a fresh authoritative match")
	print("GODOT_NATIVE_SKIRMISH checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)
