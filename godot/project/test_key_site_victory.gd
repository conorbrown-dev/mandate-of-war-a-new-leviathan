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
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	await process_frame
	view._on_start_skirmish_pressed()
	await process_frame
	var state: Dictionary = view.extension.call("match_state")
	check(int(state.get("key_sites_required", 0)) == 2 and float(state.get("key_site_hold_target_seconds", 0.0)) == 120.0, "scenario configures two of three key sites for two minutes")
	check(int(state.get("victory_reason", -1)) == 0 and int(state.get("result", -2)) == -1, "match starts unresolved")
	view.extension.call("skirmish_set_ai_enabled", false)
	check(bool(view.extension.call("match_configure_key_sites", [900, 903, 913], 2, 1)), "short fixture keeps the same sites and required count")
	check(bool(view.extension.call("economy_assign_material_site", 900, 0)), "player claims the first material site")
	check(bool(view.extension.call("economy_assign_material_site", 903, 0)), "player claims the second material site")
	_tick(view, 10)
	state = view.extension.call("match_state")
	check(int(state.get("key_sites_player", 0)) == 2 and float(state.get("key_site_hold_player_seconds", 0.0)) > 0.0, "native hold timer advances with two sites")
	check(int(state.get("result", -2)) == -1, "a partial hold does not win")
	check(bool(view.extension.call("economy_assign_material_site", 903, 1)), "enemy interrupts the hold")
	_tick(view, 1)
	state = view.extension.call("match_state")
	check(int(state.get("key_sites_player", -1)) == 1 and float(state.get("key_site_hold_player_seconds", -1.0)) == 0.0, "losing a site resets the hold timer")
	check(bool(view.extension.call("economy_assign_material_site", 903, 0)), "player retakes the second site")
	_tick(view, 20)
	state = view.extension.call("match_state")
	check(int(state.get("result", -1)) == 0 and int(state.get("victory_reason", 0)) == 1, "continuous hold wins by key sites")
	check(view.match_finished and view.match_end_panel != null and view.match_end_panel.visible, "the victory panel appears")
	if view.match_end_panel != null:
		check(view.match_end_panel._subtitle.text == "KEY SITES HELD", "the victory panel names the site objective")
	var enemy_post := int(view.commander_ids.get(1, -1))
	check(float(Array(view.extension.call("get_unit_health", enemy_post))[0]) > 0.0 and view.command_post_views[1] != null, "enemy command post stays intact")
	print("KEY_SITE_VICTORY checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	await process_frame
	quit(1 if failures > 0 else 0)


func _tick(view: Node, ticks: int) -> void:
	for _index in range(ticks):
		view.extension.call("update_simulation", 50.0)
		view._update_match_state()
