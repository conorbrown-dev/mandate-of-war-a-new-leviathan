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
	check(view.state.get("resources", []).size() == 3, "native route exposes authored Material, Energy, and Research fields")
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
	# Seed one authoritative last-known contact, then prove it ages into a
	# stale record without exposing a hidden live position.
	view.extension.call("skirmish_update_intelligence", first_enemy, 105.0, 0.0, int(view.state.get("tick", 0)))
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
	var material_before_resupply := float(view.extension.call("skirmish_state").get("material", 0.0))
	check(int(view.extension.call("skirmish_command", 5, PackedInt32Array([vessel_id]), 0.0, 0.0, 0)) == 1, "native route accepts paid naval resupply through the return command")
	for _tick in range(2):
		view.extension.call("skirmish_update", 50.0)
	for unit in view.extension.call("skirmish_state").get("units", []):
		if int(unit.get("id", 0)) == vessel_id:
			vessel_state = unit
	check(not bool(vessel_state.get("stranded", true)) and float(vessel_state.get("fuel", 0.0)) > 90.0, "native route restores naval endurance from owned energy")
	check(float(view.extension.call("skirmish_state").get("material", material_before_resupply)) < material_before_resupply, "native route deducts finite material stock for naval resupply")
	for _tick in range(20000):
		if int(view.state.get("result", -1)) >= 0:
			break
		view.extension.call("skirmish_update", 50.0)
		view.state = view.extension.call("skirmish_state")
	check(int(view.state.get("result", -1)) >= 0, "native route reaches a deterministic terminal result")
	view._complete_match()
	check(view.result_panel.visible and "DRAW" in view.result_label.text, "native route presents an unambiguous match result")
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
