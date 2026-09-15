extends SceneTree

var checks := 0
var failures := 0


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures += 1
		push_error(message)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	view._update_hud()
	var snapshot: Dictionary = view.command_hud.snapshot
	check(snapshot.get("friendly", 0) == 1, "HUD aggregate must report the opening player force")
	check(float(snapshot.get("tick", -1.0)) >= 0.0, "HUD aggregate must report simulation timing")
	check(snapshot.get("queue", null) is Array and snapshot.get("build_catalog", []).size() > 0, "HUD aggregate must provide production queue and catalog")
	check(String(snapshot.get("material", "--")) != "--", "HUD aggregate must provide resource storage")
	var build_icons_resolve := true
	for entry in snapshot.get("build_catalog", []):
		if bool(entry.get("is_structure", false)):
			build_icons_resolve = build_icons_resolve and not String(entry.get("icon_id", "")).is_empty()
		else:
			build_icons_resolve = build_icons_resolve and not String(entry.get("nato_symbol_id", "")).is_empty()
	check(build_icons_resolve, "build catalog supplies semantic presentation IDs without HUD type branches")
	view._set_selected(view.player_entity_ids[0], true)
	view._update_hud()
	snapshot = view.command_hud.snapshot
	check(snapshot.get("selected", 0) == 1 and float(snapshot.get("unit_max_health", -1.0)) > 0.0 and float(snapshot.get("off_road_speed_multiplier", -1.0)) >= 0.0, "HUD aggregate must provide selected-unit health and off-road state")
	var build_catalog_view: Control = view.command_hud.get("build_catalog_view")
	check(build_catalog_view != null and build_catalog_view.visible and build_catalog_view.get("_entries").size() > 0, "Engineer catalog is rendered through the themed node-based build component")
	await process_frame
	var viewport_width := view.get_viewport().get_visible_rect().size.x
	check(build_catalog_view != null and build_catalog_view.size.x <= 540.0 and build_catalog_view.get_global_rect().position.x >= 0.0 and build_catalog_view.get_global_rect().end.x <= viewport_width, "Build catalog remains centered and fully inside the active viewport")
	view.queue_free()
	if failures > 0:
		quit(1)
		return
	print("HUD_BRIDGE checks=%d failures=0" % checks)
	quit(0)
