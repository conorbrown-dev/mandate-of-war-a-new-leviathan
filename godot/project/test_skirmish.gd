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
	check(view._scenario_unit_count(view.scenario_definition.player) == 1, "scenario declares the player start")
	check(view._scenario_unit_count(view.scenario_definition.ai) == 1, "scenario declares the AI start")
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
	check(view.player_entity_ids.size() == 1 and view.ai_entity_ids.size() == 1 and view.player_entity_ids[0] == view._player_engineer_id(), "each side starts with a visible Field Engineer while production bases stay presentation-hidden")
	check(view.prototype_visual_views.size() == 2, "each opening Field Engineer receives its registered imported model")
	check(view.resource_sites.get_child_count() == 4 and view.extension.call("economy_get_resource_node_count") == 4, "four visible material facilities are registered with the simulation")
	var first_model: Node3D = view.prototype_visual_views.get(view.player_entity_ids[0], null)
	check(first_model != null and first_model.get_node("ModelRoot").get_child_count() > 0, "Field Engineer has an imported visual model")
	view._update_hud()
	check(not view.command_hud.snapshot.is_empty() and view.command_hud.snapshot.get("friendly", 0) == 1, "HUD receives live friendly-force telemetry")
	view._set_selected(view.player_entity_ids[0], true)
	view._update_hud()
	check(view.command_hud.snapshot.get("selected", 0) == 1, "HUD updates selection context")
	check(first_model.get_node("ModelRoot").get_child_count() > 0, "selection leaves the Field Engineer imported model intact")
	view._clear_selection()
	var click_position: Vector2 = view.camera.unproject_position(view._entity_world_position(view.player_entity_ids[0]))
	view.selecting = true
	view.selection_start = click_position
	view._finish_selection(click_position)
	check(view.selected_ids.size() == 1 and view.player_entity_ids.has(view.selected_ids[0]), "single-click selection acquires a player unit")
	check(view.command_hud.snapshot.get("unit_name", "") != "" and float(view.command_hud.snapshot.get("unit_max_health", -1.0)) > 0.0, "single-unit HUD exposes type and health")
	var engineer_id: int = view.player_entity_ids[0]
	var commander_id: int = int(view.commander_ids.get(0, -1))
	var stores_before: Array = view.extension.call("economy_get_storage_info", commander_id)
	view.extension.call("update_simulation", 50.0)
	var stores_after: Array = view.extension.call("economy_get_storage_info", commander_id)
	check(stores_after == stores_before, "hidden production base has no automatic local resource income")
	var neutral_site: Dictionary = view.material_site_state[900]
	var materials_before_claim := float(stores_after[0])
	view._set_material_order_mode(1)
	view._issue_material_site_order(view.camera.unproject_position(Vector3(neutral_site.position.x, 0.0, neutral_site.position.y)))
	view.extension.call("update_simulation", 50.0)
	var materials_after_claim: Array = view.extension.call("economy_get_storage_info", commander_id)
	check(int(view.material_site_state[900].owner) == 0 and float(materials_after_claim[0]) > materials_before_claim, "Field Engineer captures a neutral facility into shared Materials")
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
	var build_catalog: Array = view.command_hud.snapshot.get("build_catalog", [])
	var catalog_types: Array[int] = []
	for entry in build_catalog:
		if not bool(entry.get("is_structure", false)):
			catalog_types.append(int(entry.get("type", -1)))
	check(catalog_types.has(9) and catalog_types.has(10) and catalog_types.has(11) and view.construction_frames.get_child_count() == 1, "build menu exposes the added fighter, VTOL, and patrol boat alongside the active queue")
	if OS.get_environment("RTS_CAPTURE_SCREENSHOT") == "1":
		await RenderingServer.frame_post_draw
		var production_image := view.get_viewport().get_texture().get_image()
		check(production_image.save_png("/tmp/near-future-rts-presentation.png") == OK, "GPU production HUD screenshot saved")
	for _tick in range(310):
		view.extension.call("update_simulation", 50.0)
		view._sync_new_entities()
	check(view.player_entity_ids.size() == 2, "completed Command Walker production adds a player unit")
	check((view.demo_unit_views.has(view.player_entity_ids[1]) or view.prototype_visual_views.has(view.player_entity_ids[1])) and view.demo_unit_views.size() + view.prototype_visual_views.size() >= 3, "completed unit receives a visible 3D model")
	check(view.extension.call("get_unit_x", view.player_entity_ids[1]) != view.extension.call("get_unit_x", commander_id), "completed unit spawns at a visible factory rally point")
	view._set_selected(engineer_id, true)
	check(view._build_unit_type_at_index(3) == 9 and view._build_unit_type_at_index(4) == 10 and view._build_unit_type_at_index(5) == 11, "new unit shortcuts resolve from the native catalog rather than hard-coded unit types")
	view._queue_commander_unit(9)
	view.extension.call("update_simulation", 50.0)
	view._update_hud()
	production_queue = view.command_hud.snapshot.get("queue", [])
	active_build = production_queue[0] if not production_queue.is_empty() else {}
	check(int(active_build.get("type", -1)) == 9, "Command Walker queues the added fighter through the gameplay build path")
	for _tick in range(410):
		view.extension.call("update_simulation", 50.0)
		view._sync_new_entities()
	var fighter_view: Node3D = view.prototype_visual_views.get(view.player_entity_ids[2], null)
	var fighter_model_root: Node3D = fighter_view.get_node_or_null("ModelRoot") if fighter_view != null else null
	check(view.player_entity_ids.size() == 3 and (fighter_view != null or view.demo_unit_views.has(view.player_entity_ids[2])) and (fighter_model_root == null or fighter_model_root.get_child_count() > 0) and (fighter_model_root == null or fighter_model_root.get_child(0).name != "DevelopmentFallbackMesh"), "completed fighter joins the player force with its registered imported gameplay visual")
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
	var fob_engineer_id: int = view.extension.call("create_unit_with_type", 30.0, 30.0, 8, 0)
	check(fob_engineer_id > 0, "engineering-unit FOB fixture is created")
	view._register_presented_unit(fob_engineer_id, 8, 0, Vector2(30.0, 30.0))
	view._clear_selection()
	view._set_selected(fob_engineer_id, true)
	var fob_target := Vector2(36.0, 36.0)
	view.fob_build_mode = true
	var fob_screen: Vector2 = view.camera.unproject_position(Vector3(fob_target.x, 0.0, fob_target.y))
	view._order_fob(fob_screen)
	view.extension.call("update_simulation", 50.0)
	view._update_hud()
	var fob_state: Array = view.command_hud.snapshot.get("fob_installation", [])
	check(fob_state.size() >= 6 and int(fob_state[3]) == 1 and float(fob_state[4]) > 0.0 and float(fob_state[4]) < 1.0, "FOB HUD reports authoritative construction progress")
	for _tick in range(200):
		view.extension.call("update_simulation", 50.0)
	view._update_hud()
	fob_state = view.command_hud.snapshot.get("fob_installation", [])
	check(fob_state.size() >= 6 and int(fob_state[2]) == 1 and int(fob_state[3]) == 0 and float(fob_state[4]) == 1.0, "FOB HUD reports completed active installation")
	check(String(view.command_hud.snapshot.get("fob_completion_notification", "")) == "FOB ONLINE  //  LOGISTICS LINK ESTABLISHED", "FOB HUD reports completion notification")
	print("GODOT_SKIRMISH_PRESENTATION checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)
