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
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	check(view.scenario_description.text == String(view.scenario_definition.description), "briefing presents scenario intelligence in readable sentence case")
	check(view.scenario_objective.text == String(view.scenario_definition.victory.description).to_upper(), "briefing names the victory condition under OBJECTIVE")
	check(view.scenario_status.text.contains("airfield") and view.scenario_status.text.contains("Strategic Command (G)") and view.scenario_status.text.contains("fuel"), "briefing explains arrival and supply controls")
	view._on_start_skirmish_pressed()
	view.extension.call("stop_simulation")
	# Use real native entities without visual wrappers: names must also work at
	# strategic zoom, with batching, and for types absent from the old UI table.
	var spawn: Array = view.scenario_definition.player.spawn
	var spot := Vector2(float(spawn[0]), float(spawn[1]))
	for unit_type in [0, 8, 13, 14, 17]:
		var entity_id: int = view.extension.call("create_unit_with_type", spot.x, spot.y, unit_type, 0)
		check(entity_id > 0, "test entity created for type %d" % unit_type)
		var catalog: Array = view.extension.call("get_build_catalog", 0)
		var expected := ""
		for entry in catalog:
			if int(entry.type) == unit_type:
				expected = String(entry.name).to_upper()
		# Engineers and transports are not requisition cards; compare their
		# known native names as well as the catalog-backed combat/recon types.
		if unit_type == 8:
			expected = "ENGINEERING UNIT"
		if unit_type == 17:
			expected = "SKYBRIDGE AIRLIFTER"
		check(not expected.is_empty() and view._selected_unit_snapshot(entity_id).name == expected, "HUD uses the native name for type %d (expected %s, got %s)" % [unit_type, expected, view._selected_unit_snapshot(entity_id).name])
	check(String(view.extension.call("get_unit_display_name", -1)).is_empty(), "invalid entity has no name")
	check(String(view.extension.call("get_unit_display_name", 2147483648)).is_empty(), "oversized entity cannot wrap to a live ID")
	view.queue_free()
	await process_frame
	print("SLICE4_BRIEFING checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
