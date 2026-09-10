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
	var view: Node3D = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame

	check(not view.scenario_definition.is_empty(), "validated scenario reaches the menu")
	check(view._scenario_unit_count(view.scenario_definition.player) == 1, "scenario declares one player Command Walker")
	check(view._scenario_unit_count(view.scenario_definition.ai) == 1, "scenario declares one AI Command Walker")
	check(view.terrain_world_width == 640.0 and view.terrain_world_height == 640.0, "starting theater covers four times the previous 320 by 320 area")
	check(view.terrain_view.mesh is ArrayMesh and view.terrain_view.mesh.get_surface_count() == 1, "one generated heightmap mesh is visible")
	var terrain_bounds: AABB = view.terrain_view.mesh.get_aabb()
	check(terrain_bounds.size.x > 630.0 and terrain_bounds.size.z > 630.0 and terrain_bounds.size.y > 1.0, "terrain is map-scale and visibly non-flat")
	check(not view.west_landmass.visible and not view.east_landmass.visible, "heightmap replaces fallback landmass planes without duplicates")
	check(view.terrain_view.material_override is ShaderMaterial, "terrain uses the textured shader material")
	var terrain_material: ShaderMaterial = view.terrain_view.material_override
	check(terrain_material.get_shader_parameter("ocean_texture") != null and terrain_material.get_shader_parameter("grassland_texture") != null, "terrain shader has ocean and grassland texture assets")
	check(terrain_material.get_shader_parameter("forest_texture") != null and terrain_material.get_shader_parameter("mud_texture") != null and terrain_material.get_shader_parameter("rocky_texture") != null, "terrain shader binds imported forest, mud, and rocky surfaces")
	check(view.terrain_trees.multimesh != null and view.terrain_trees.multimesh.instance_count == 520, "terrain has a batched forest layer")
	check(view.forest_landmarks.get_child_count() == 24, "opening view has visible forest silhouettes on both landmasses")

	view._on_start_skirmish_pressed()
	check(view.match_started, "menu starts the native skirmish")
	check(view.command_hud.visible, "tactical command HUD appears after the skirmish starts")
	check(not view.debug_panel.visible and not view.help_label.visible, "legacy debug and help overlays do not crowd the tactical HUD")
	check(view.player_entity_ids.size() == 1 and view.ai_entity_ids.size() == 1, "each side starts with one Command Walker")
	check(view.demo_unit_views.size() == 2 and view.unit_models.get_child_count() == 2, "each opening commander receives a visible demo model")
	check(view.resource_sites.get_child_count() == 4 and view.extension.call("economy_get_resource_node_count") == 4, "four visible material facilities are registered with the simulation")
	var first_model: Node3D = view.demo_unit_views.get(view.player_entity_ids[0], null)
	check(first_model != null and first_model.get_child_count() >= 4, "unit model has chassis, turret, weapon, and beacon geometry")
	var hull: MeshInstance3D = first_model.get_child(0) if first_model != null else null
	var hull_material: StandardMaterial3D = hull.material_override if hull != null else null
	check(hull_material != null and hull_material.albedo_texture != null, "tank hull uses the bound near-future armor texture")
	view._update_hud()
	check(not view.command_hud.snapshot.is_empty() and view.command_hud.snapshot.get("friendly", 0) == 1, "HUD receives live friendly-force telemetry")
	view._set_selected(view.player_entity_ids[0], true)
	view._update_hud()
	check(view.command_hud.snapshot.get("selected", 0) == 1, "HUD updates selection context")
	check(first_model.get_node("SelectionMarker").visible and hull.material_override.albedo_texture != null, "selection marker preserves textured armor")
	view._clear_selection()
	var click_position: Vector2 = view.camera.unproject_position(view._entity_world_position(view.player_entity_ids[0]))
	view.selecting = true
	view.selection_start = click_position
	view._finish_selection(click_position)
	check(view.selected_ids.size() == 1 and view.player_entity_ids.has(view.selected_ids[0]), "single-click selection acquires a player unit")
	check(view.command_hud.snapshot.get("unit_name", "") != "" and float(view.command_hud.snapshot.get("unit_max_health", -1.0)) > 0.0, "single-unit HUD exposes type and health")
	var commander_id: int = view.player_entity_ids[0]
	var stores_before: Array = view.extension.call("economy_get_storage_info", commander_id)
	view.extension.call("update_simulation", 50.0)
	var stores_after: Array = view.extension.call("economy_get_storage_info", commander_id)
	check(stores_after == stores_before, "Command Walker has no automatic local resource income")
	var neutral_site: Dictionary = view.material_site_state[900]
	var materials_before_claim := float(stores_after[0])
	view._set_material_order_mode(1)
	view._issue_material_site_order(view.camera.unproject_position(Vector3(neutral_site.position.x, 0.0, neutral_site.position.y)))
	view.extension.call("update_simulation", 50.0)
	var materials_after_claim: Array = view.extension.call("economy_get_storage_info", commander_id)
	check(int(view.material_site_state[900].owner) == 0 and float(materials_after_claim[0]) > materials_before_claim, "Command Walker captures a neutral facility into shared Materials")
	var enemy_site: Dictionary = view.material_site_state[902]
	view._set_material_order_mode(2)
	view._issue_material_site_order(view.camera.unproject_position(Vector3(enemy_site.position.x, 0.0, enemy_site.position.y)))
	await process_frame
	check(bool(view.material_site_state[902].destroyed) and view.resource_sites.get_child_count() == 3 and view.extension.call("economy_get_resource_node_count") == 3, "commander can demolish an enemy material facility")
	view._queue_commander_unit(0)
	view.extension.call("update_simulation", 50.0)
	view._update_hud()
	var production_queue: Array = view.command_hud.snapshot.get("queue", [])
	check(production_queue.size() == 1, "selected Command Walker queues a timed MBT build")
	var active_build: Dictionary = production_queue[0] if not production_queue.is_empty() else {}
	check(float(active_build.get("remaining_seconds", 0.0)) > 0.0 and float(active_build.get("reserved_material", 0.0)) > 0.0, "production queue exposes remaining time and authoritative reserved resources")
	check(view.command_hud.snapshot.get("build_catalog", []).size() >= 3 and view.construction_frames.get_child_count() == 1, "build menu and visible construction skeleton are driven by the queue")
	if OS.get_environment("RTS_CAPTURE_SCREENSHOT") == "1":
		await RenderingServer.frame_post_draw
		var production_image := view.get_viewport().get_texture().get_image()
		check(production_image.save_png("/tmp/near-future-rts-presentation.png") == OK, "GPU production HUD screenshot saved")
	for _tick in range(310):
		view.extension.call("update_simulation", 50.0)
		view._sync_new_entities()
	check(view.player_entity_ids.size() == 2, "completed Command Walker production adds a player unit")
	check(view.demo_unit_views.size() >= 3 and view.demo_unit_views.has(view.player_entity_ids[1]), "completed unit receives a visible 3D model")
	check(view.extension.call("get_unit_x", view.player_entity_ids[1]) != view.extension.call("get_unit_x", commander_id), "completed unit spawns at a visible factory rally point")
	view._clear_selection()
	var drag_bounds := Rect2(view.camera.unproject_position(view._entity_world_position(view.player_entity_ids[0])), Vector2.ZERO)
	for index in range(1, view.player_entity_ids.size()):
		drag_bounds = drag_bounds.expand(view.camera.unproject_position(view._entity_world_position(view.player_entity_ids[index])))
	drag_bounds = drag_bounds.grow(8.0)
	view.selecting = true
	view.selection_start = drag_bounds.position
	view._finish_selection(drag_bounds.end)
	var all_selected_are_player := true
	for entity_id in view.selected_ids:
		all_selected_are_player = all_selected_are_player and view.player_entity_ids.has(entity_id)
	check(view.selected_ids.size() >= 2 and all_selected_are_player, "drag-box selection acquires multiple player units")
	print("GODOT_SKIRMISH_PRESENTATION checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)
