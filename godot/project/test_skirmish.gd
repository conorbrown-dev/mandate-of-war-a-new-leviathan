extends SceneTree

const UiIconRegistry = preload("res://ui/icons/ui_icon_registry.gd")

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
	check(view.terrain_world_width == 40000.0 and view.terrain_world_height == 40000.0, "starting theater is a 40 km by 40 km battlefield")
	check(view.camera.far >= 120000.0, "camera far plane covers the full theater at strategic zoom")
	var battlefield_environment: WorldEnvironment = view.get_node("WorldEnvironment")
	check(battlefield_environment.environment != null and battlefield_environment.environment.background_mode == Environment.BG_SKY and battlefield_environment.environment.fog_enabled, "battlefield scene uses a sky and distance fog instead of the editor clear color")
	var battlefield_light: DirectionalLight3D = view.get_node("DirectionalLight3D")
	check(battlefield_light.shadow_enabled and battlefield_light.light_energy > 1.0, "battlefield lighting has directional shadows and authored sunlight")
	var moon_light: DirectionalLight3D = view.get_node("MoonLight3D")
	check(moon_light.shadow_enabled and not moon_light.visible, "battlefield scene has a shadow-casting moon light reserved for nighttime")
	check(view.terrain_view.mesh is ArrayMesh and view.terrain_view.mesh.get_surface_count() == 1, "one generated heightmap mesh is visible")
	var terrain_bounds: AABB = view.terrain_view.mesh.get_aabb()
	check(terrain_bounds.size.x > 630.0 and terrain_bounds.size.z > 630.0 and terrain_bounds.size.y > 1.0, "terrain is map-scale and visibly non-flat")
	check(terrain_bounds.size.x > 39000.0 and terrain_bounds.size.z > 39000.0 and terrain_bounds.size.y > 1200.0, "terrain includes visible mountain-scale elevation")
	check(not view.west_landmass.visible and not view.east_landmass.visible, "heightmap replaces fallback landmass planes without duplicates")
	check(view.terrain_view.material_override is ShaderMaterial, "terrain uses the textured shader material")
	var terrain_material: ShaderMaterial = view.terrain_view.material_override
	check(terrain_material.get_shader_parameter("ocean_texture") != null and terrain_material.get_shader_parameter("grassland_texture") != null, "terrain shader has ocean and grassland texture assets")
	check(terrain_material.get_shader_parameter("forest_texture") != null and terrain_material.get_shader_parameter("mud_texture") != null and terrain_material.get_shader_parameter("rocky_texture") != null, "terrain shader binds imported forest, mud, and rocky surfaces")
	check(float(terrain_material.get_shader_parameter("water_wave_speed")) > 0.0 and float(terrain_material.get_shader_parameter("water_wave_height")) > 0.0, "terrain water has authored animated current and wave-height parameters")
	check(is_equal_approx(float(terrain_material.get_shader_parameter("terrain_brightness")), 1.0) and is_equal_approx(float(terrain_material.get_shader_parameter("terrain_daylight")), 1.0) and float(terrain_material.get_shader_parameter("terrain_ambient_fill")) > 0.0 and terrain_material.get_shader_parameter("fog_beacon_lights") != null and is_equal_approx(float(terrain_material.get_shader_parameter("texture_tiling")), 384.0), "terrain shader exposes daytime, local floodlight illumination, and tactical-scale texture tiling")
	var tactical_tree_count := 0
	var strategic_tree_count := 0
	var tree_grounded: bool = view.forest_tree_root_depths.size() == view.forest_tree_positions.size()
	for tree_view in view.tactical_tree_views:
		tactical_tree_count += tree_view.multimesh.instance_count
	for root_depth_value in view.forest_tree_root_depths:
		tree_grounded = tree_grounded and float(root_depth_value) >= -0.15 and float(root_depth_value) <= 0.02
	for tree_view in view.strategic_tree_views:
		strategic_tree_count += tree_view.multimesh.instance_count
	check(view.tactical_tree_views.size() == 3 and tactical_tree_count == 960 and view.terrain_trees.multimesh.mesh.get_surface_count() >= 1, "terrain uses the three user-supplied GLB variants in batched tactical forest groups")
	check(view.forest_cluster_centers.size() == 9 and view.forest_tree_positions.size() == 960 and view.forest_tree_variant_indices.size() == 960 and strategic_tree_count == tactical_tree_count / 2, "default skirmish distributes the varied user trees across large deterministic clusters and half-density strategic groups")
	check(tree_grounded, "the lowest imported oak vertices embed only slightly beneath the heightmap")
	view.camera_distance = 7000.0
	view._sync_tree_lod()
	check(view.strategic_trees.visible and not view.terrain_trees.visible, "strategic zoom switches to the lower-cost tree representation")
	view.camera_distance = 1500.0
	view._sync_tree_lod()
	check(view.terrain_trees.visible and not view.strategic_trees.visible, "tactical zoom restores detailed tree models")
	check(view.forest_landmarks.get_child_count() == 24, "opening view has visible forest silhouettes on both landmasses")
	var center_screen := view.get_viewport().get_visible_rect().get_center()
	var terrain_cursor_target: Vector2 = view._screen_to_world(center_screen)
	var terrain_cursor_projection: Vector2 = view.camera.unproject_position(Vector3(terrain_cursor_target.x, view._terrain_height_at(terrain_cursor_target.x, terrain_cursor_target.y), terrain_cursor_target.y))
	check(terrain_cursor_projection.distance_to(center_screen) < 3.0, "terrain build ray follows the cursor instead of the distant flat-world plane")

	view._on_start_skirmish_pressed()
	check(view.match_started, "menu starts the native skirmish")
	check(bool(view.extension.call("validate_structure_placement", 0, -16500.0, -1500.0)), "native structure validator accepts a clear land footprint")
	check(not bool(view.extension.call("validate_structure_placement", 0, 0.0, 0.0)), "native structure validator rejects the theater water channel")
	check(view.command_hud.visible, "tactical command HUD appears after the skirmish starts")
	check(UiIconRegistry.get_icon_path(&"resource.material") == "res://assets/ui/icons/resources/material.svg", "resource HUD uses the semantic material icon")
	check(view.command_hud.snapshot.get("hover_title", "") == "TACTICAL INSPECT", "bottom inspection strip has a stable tactical empty state")
	check(not view.territory_debug_label.visible, "centered territory diagnostic is hidden from the tactical battlefield")
	check(not view.debug_panel.visible and not view.help_label.visible, "legacy debug and help overlays do not crowd the tactical HUD")
	var f8 := InputEventKey.new()
	f8.pressed = true
	f8.keycode = KEY_F8
	view._unhandled_input(f8)
	check(view.debug_panel.visible, "F8 opens the developer environment panel")
	check(view.environment_debug_weather_option != null and view.environment_debug_day_button != null and view.environment_debug_night_button != null, "developer environment panel exposes weather and day/night controls")
	view._apply_environment_weather("STORM")
	check(view.battlefield_environment.environment.fog_density > 0.000018, "storm weather increases battlefield fog")
	view._apply_environment_weather("CLEAR")
	check(view.battlefield_light.light_energy > 1.85 and float(terrain_material.get_shader_parameter("terrain_daylight")) > 1.0 and float(terrain_material.get_shader_parameter("terrain_ambient_fill")) >= 0.28, "clear daytime weather restores strong overhead sunlight, terrain daylight, and shadow fill")
	view._apply_time_of_day("NIGHTTIME")
	check(not battlefield_light.visible and moon_light.visible and view.battlefield_environment.environment.ambient_light_energy < 0.72, "nighttime switches from sunlight to moonlight and lowers ambient fill")
	view._reset_environment_debug()
	view._unhandled_input(f8)
	check(not view.debug_panel.visible, "F8 closes the developer environment panel")
	check(view.player_entity_ids.size() == 1 and view.ai_entity_ids.size() == 1 and view.player_entity_ids[0] == view._player_engineer_id(), "each side starts with a visible Field Engineer while production bases stay presentation-hidden")
	check(view.prototype_visual_views.size() == 2, "each opening Field Engineer receives its registered imported model")
	check(view.resource_sites.get_child_count() == 4 and view.extension.call("economy_get_resource_node_count") == 4, "four visible material facilities are registered with the simulation")
	check(view.civilian_building_instance_count >= 20 and view.civilian_buildings.get_child_count() == 3, "civilian dressing creates clustered instanced building groups")
	var civilian_land_only := true
	var first_civilian_position := Vector3.INF
	for civilian_group in view.civilian_buildings.get_children():
		var civilian_multimesh := civilian_group.multimesh as MultiMesh
		for instance_index in range(civilian_multimesh.instance_count):
			var civilian_position := civilian_multimesh.get_instance_transform(instance_index).origin
	for civilian_position in view.civilian_building_positions:
		if first_civilian_position == Vector3.INF:
			first_civilian_position = Vector3(civilian_position.x, 0.0, civilian_position.y)
		civilian_land_only = civilian_land_only and absf(civilian_position.x) > 5500.0
	check(civilian_land_only, "civilian buildings are authored on walkable land only")
	check(first_civilian_position != Vector3.INF and not bool(view.extension.call("is_land_position", first_civilian_position.x, first_civilian_position.z)) and bool(view.extension.call("is_land_position", first_civilian_position.x, first_civilian_position.z + 3000.0)), "civilian building footprints block local ground navigation without sealing the wider route")
	var first_model: Node3D = view.prototype_visual_views.get(view.player_entity_ids[0], null)
	check(first_model != null and first_model.get_node("ModelRoot").get_child_count() > 0, "Field Engineer has an imported visual model")
	check(first_model != null and first_model.get_node("StrategicZoomVisual").mesh != null, "Field Engineer has a generated strategic icon")
	var engineer_terrain_height: float = float(view._terrain_height_at(first_model.global_position.x, first_model.global_position.z)) if first_model != null else 0.0
	var engineer_model_bottom: float = first_model.global_position.y + float(first_model.call("model_bottom_height")) if first_model != null else INF
	check(first_model != null and absf(engineer_model_bottom - engineer_terrain_height - 0.05) < 0.02, "Field Engineer model bottom stays five centimeters above the rendered terrain")
	view.camera_distance = 550.0
	view._sync_unit_transforms()
	check(first_model != null and first_model.get_node("ModelRoot").visible, "zooming in past the strategic marker threshold restores the engineer model")
	view._update_hud()
	check(not view.command_hud.snapshot.is_empty() and view.command_hud.snapshot.get("friendly", 0) == 1, "HUD receives live friendly-force telemetry")
	view._set_selected(view.player_entity_ids[0], true)
	view._update_hud()
	check(view.command_hud.snapshot.get("selected", 0) == 1, "HUD updates selection context")
	view.hover_context = {"key": "unit:%d" % view.player_entity_ids[0], "title": "FRIENDLY // ENGINEERING VEHICLE", "detail": "INTEGRITY 100 / 100", "accent": Color("#7ad99b")}
	view._update_hud()
	check(view.command_hud.snapshot.get("hover_title", "") == "FRIENDLY // ENGINEERING VEHICLE" and view.command_hud.snapshot.get("hover_detail", "") == "INTEGRITY 100 / 100", "bottom inspection strip receives stable hovered-unit context")
	var engineer_hud_state: Dictionary = view.extension.call("get_hud_state", 0, int(view.commander_ids.get(0, -1)), view.player_entity_ids[0], 0.0, 0.0, false)
	var engineer_off_road_state: PackedFloat32Array = engineer_hud_state.get("selected_off_road", PackedFloat32Array())
	check(engineer_off_road_state.size() == 3 and view.command_hud.snapshot.get("off_road_speed_multiplier", -1.0) >= 0.0 and "WEAR" in String(view.command_hud.snapshot.get("selection_detail", "")), "selected ground units expose off-road wear and speed telemetry")
	var pan_key := InputEventKey.new()
	pan_key.pressed = true
	pan_key.keycode = KEY_A
	view._unhandled_input(pan_key)
	check(view.build_mode_type == -1, "A remains a camera-pan key and never arms construction")
	var yaw_before: float = view.tactical_camera_yaw
	view._rotate_camera_facing(-1.0, 0.5)
	check(view.tactical_camera_yaw < yaw_before, "Q-facing camera rotation turns the tactical view left")
	view.tactical_camera_yaw = PI * 0.5
	var forward_at_east := Vector2(sin(view.tactical_camera_yaw), cos(view.tactical_camera_yaw))
	check(forward_at_east.distance_to(Vector2.RIGHT) < 0.001, "camera movement forward follows the current tactical yaw")
	var viewport_size := view.get_viewport().get_visible_rect().size
	var build_ui_scale := clampf(minf(viewport_size.x / 1920.0, viewport_size.y / 1080.0), 0.62, 1.0)
	var build_origin := Vector2((viewport_size.x / build_ui_scale - 540.0) * 0.5, 12.0)
	var floodlight_card := (build_origin + Vector2(8 + 3 * 132 + 64, 138)) * build_ui_scale
	check(view._blueprint_at_screen(floodlight_card) == 103, "compact fourth structure card selects the Floodlight without overflowing the build deck")
	var floodlight_hover: Dictionary = view.command_hud.get_build_hover_context_at(floodlight_card / build_ui_scale)
	check(floodlight_hover.get("title", "") == "BUILD // FLOODLIGHT" and "620" in String(floodlight_hover.get("detail", "")), "bottom inspection strip receives Floodlight identity, costs, and readiness")
	check(floodlight_hover.get("accent", "") == Color("#ffbd52"), "BUILD hover uses themed orange accent")
	check(first_model.get_node_or_null("SelectionVisual") == null, "selection relies on the persistent range envelopes instead of a redundant yellow hex")
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
	var produced_position: PackedFloat32Array = view.extension.call("get_unit_position", view.player_entity_ids[1])
	var commander_position: PackedFloat32Array = view.extension.call("get_unit_position", commander_id)
	check(produced_position.size() == 2 and commander_position.size() == 2 and produced_position[0] != commander_position[0], "completed unit spawns at a visible factory rally point")
	view._set_selected(engineer_id, true)
	check(view._build_unit_type_at_index(3) == 9 and view._build_unit_type_at_index(4) == 10 and view._build_unit_type_at_index(5) == 11, "new unit shortcuts resolve from the native catalog rather than hard-coded unit types")
	var fighter_queue_before_airfield: Array = view.extension.call("get_hud_state", 0, commander_id, -1, 0.0, 0.0, false).get("production_queue", [])
	view._queue_commander_unit(9)
	var fighter_queue_after_airfield: Array = view.extension.call("get_hud_state", 0, commander_id, -1, 0.0, 0.0, false).get("production_queue", [])
	check(fighter_queue_after_airfield.size() == fighter_queue_before_airfield.size(), "runway aircraft cannot be queued before an airfield is online")
	var visibility_hex := first_model.get_node("VisibilityHex") as MeshInstance3D
	var radar_hex := first_model.get_node("RadarHex") as MeshInstance3D
	var attack_hex := first_model.get_node("AttackHex") as MeshInstance3D
	check(visibility_hex.visible and radar_hex.visible and attack_hex.visible and visibility_hex.mesh is ImmediateMesh and radar_hex.mesh is ImmediateMesh and attack_hex.mesh is ImmediateMesh, "unselected units show flat blue visibility, purple radar, and red attack hex line ranges")
	var requested_airfield_target := Vector2(-12500.0, 0.0)
	var airfield_target := requested_airfield_target
	check(bool(view.extension.call("validate_structure_placement", 2, airfield_target.x, airfield_target.y)), "Airfield placement accepts the exact cursor location on the level starting pad")
	view._set_selected(engineer_id, true)
	view._order_commander_to_build(102, airfield_target)
	var pending_airfield_order: Dictionary = view.pending_build_order
	check(not pending_airfield_order.is_empty() and (pending_airfield_order.get("target", Vector2.INF) as Vector2).distance_to(airfield_target) <= 0.01, "Airfield input preserves the exact cursor target without distant snapping")
	view.pending_build_order.clear()
	check(view._queue_commander_structure(2, airfield_target), "Command Walker queues an airfield structure on land")
	for _tick in range(750):
		view.extension.call("update_simulation", 50.0)
		view._sync_new_entities()
	view._update_hud()
	check(view.extension.call("territory_get_installation_info", airfield_target.x, airfield_target.y).size() >= 6, "completed airfield is registered as an active tactical installation")
	var beacon_target := Vector2(-15000.0, -6500.0)
	var beacon_catalog: Array = view.extension.call("get_build_catalog", 0)
	var beacon_entry: Dictionary = beacon_catalog.filter(func(entry): return int(entry.get("type", -1)) == 103).front() if not beacon_catalog.filter(func(entry): return int(entry.get("type", -1)) == 103).is_empty() else {}
	check(beacon_entry.get("name", "") == "FLOODLIGHT" and float(beacon_entry.get("material", 0.0)) > 0.0 and float(beacon_entry.get("energy", 0.0)) > 0.0, "Floodlight is an authoritative Material and Energy build option")
	var beacon_queued: bool = view._queue_commander_structure(3, beacon_target)
	if beacon_queued:
		# The input path records this when the engineer reaches the site; the
		# direct harness call records the same presentation completion contract.
		view.pending_completed_structures.append({"type": 3, "target": beacon_target})
	check(beacon_queued, "Command Walker queues a Floodlight on land")
	for _tick in range(500):
		view.extension.call("update_simulation", 50.0)
		view._sync_new_entities()
	view._update_hud()
	var floodlight: Node3D = view.get_node_or_null("Built_Floodlight")
	check(floodlight != null and floodlight.get_node_or_null("Floodlight") is OmniLight3D and is_equal_approx((floodlight.get_node("Floodlight") as OmniLight3D).omni_range, 360.0) and is_equal_approx((floodlight.get_node("Floodlight") as OmniLight3D).light_energy, 6.0), "completed Floodlight creates a focused tactical light")
	var beacon_lighting: PackedVector4Array = terrain_material.get_shader_parameter("fog_beacon_lights")
	check(beacon_lighting.size() >= 1 and is_equal_approx(beacon_lighting[0].x, beacon_target.x) and is_equal_approx(beacon_lighting[0].y, beacon_target.y) and is_equal_approx(beacon_lighting[0].z, 360.0), "completed Floodlight publishes focused local ground illumination to the terrain shader")
	view._apply_time_of_day("NIGHTTIME")
	check((floodlight.get_node("Floodlight") as OmniLight3D).visible and float(terrain_material.get_shader_parameter("terrain_daylight")) < 0.5 and beacon_lighting[0].w > 1.0, "Floodlight remains active with independent terrain illumination at night")
	view._apply_time_of_day("DAYTIME")
	view._set_selected_structure(floodlight)
	var floodlight_label := floodlight.get_node_or_null("StructureStatus") as Label3D if floodlight != null else null
	check(floodlight_label != null and floodlight_label.visible and "FLOODLIGHT" in floodlight_label.text and "380" in floodlight_label.text and "620" in floodlight_label.text, "selected structures show their name and committed Material/Energy above the model")
	view._queue_commander_unit(9)
	view.extension.call("update_simulation", 50.0)
	view._update_hud()
	production_queue = view.command_hud.snapshot.get("queue", [])
	active_build = production_queue[0] if not production_queue.is_empty() else {}
	check(int(active_build.get("type", -1)) == 9, "Command Walker queues the added fighter through the gameplay build path")
	for _tick in range(410):
		view.extension.call("update_simulation", 50.0)
		view._sync_new_entities()
	var fighter_id := int(view.player_entity_ids[2]) if view.player_entity_ids.size() > 2 else -1
	var fighter_view: Node3D = view.prototype_visual_views.get(fighter_id, null)
	var fighter_model_root: Node3D = fighter_view.get_node_or_null("ModelRoot") if fighter_view != null else null
	check(view.player_entity_ids.size() == 3 and (fighter_view != null or view.demo_unit_views.has(fighter_id)) and (fighter_model_root == null or fighter_model_root.get_child_count() > 0) and (fighter_model_root == null or fighter_model_root.get_child(0).name != "DevelopmentFallbackMesh"), "completed fighter joins the player force with its registered imported gameplay visual")
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
	var fob_spawn := Vector2(-16500.0, -1500.0)
	var fob_engineer_id: int = view.extension.call("create_unit_with_type", fob_spawn.x, fob_spawn.y, 8, 0)
	check(fob_engineer_id > 0, "engineering-unit FOB fixture is created")
	view._register_presented_unit(fob_engineer_id, 8, 0, fob_spawn)
	view._clear_selection()
	view._set_selected(fob_engineer_id, true)
	var fob_target := Vector2(-16400.0, -1500.0)
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
	view.camera_distance = 700.0
	view.target_camera_distance = 700.0
	view.camera_target = view._entity_world_position(view.player_entity_ids[0])
	view._update_camera(1.0)
	view._sync_unit_transforms()
	check(view.strategic_icon_overlay.match_view == view and not first_model.get_node("ModelRoot").visible and not first_model.get_node("StrategicZoomVisual").visible, "strategic view uses the screen-space marker overlay instead of a 3D gray dot")
	var zoom_cursor := Vector2(view.get_viewport().size.x * 0.82, view.get_viewport().size.y * 0.22)
	var zoom_anchor: Vector2 = view._screen_to_world(zoom_cursor)
	var centre_anchor: Vector2 = view._screen_to_world(view.get_viewport().get_visible_rect().get_center())
	check(zoom_anchor.distance_to(centre_anchor) > 100.0, "cursor-zoom regression fixture uses terrain materially away from screen centre")
	var wheel_zoom := InputEventMouseButton.new()
	wheel_zoom.button_index = MOUSE_BUTTON_WHEEL_UP
	wheel_zoom.pressed = true
	wheel_zoom.position = zoom_cursor
	view._unhandled_input(wheel_zoom)
	for frame in range(75):
		view._update_camera(1.0 / 60.0)
	var zoom_after: Vector2 = view._screen_to_world(zoom_cursor)
	check(zoom_after.distance_to(zoom_anchor) <= 25.0 and not view.zoom_focus_pending, "strategic zoom keeps the terrain under the cursor within the validated 25 m tolerance")
	view.target_camera_distance = 24.0
	view._update_camera(1.0)
	check(view.camera.global_position.y >= view._terrain_height_at(view.camera.global_position.x, view.camera.global_position.z) + 5.9, "close camera stays above the terrain clearance floor")
	print("GODOT_SKIRMISH_PRESENTATION checks=%d failures=%d" % [checks, failures])
	view.queue_free()
	await process_frame
	quit(1 if failures > 0 else 0)
