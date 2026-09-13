extends SceneTree

const MAIN_SCENE := preload("res://main.tscn")
const DEFAULT_SEED := 424242
const SCENARIO_DESCRIPTIONS := {
	"basic_selection_move": "Select the production Field Engineer through mouse input and move it to a terrain-picked destination.",
	"strategic_zoom_transition": "Zoom from the tactical model to the strategic icon through mouse-wheel input while preserving selection and cursor focus.",
	"oak_grove_showcase": "Frame the imported oak forest at tactical and strategic distances while proving variant, cluster, and terrain-grounding contracts.",
	"airfield_fighter_ferry": "Construct an airfield, request a runway fighter, and verify its paid off-map airborne ingress.",
	"road_construction": "Place a road through the engineer input path, reject water, pay costs and complete a visible persistent segment."
}

var _scenario := ""
var _artifact_dir := ""
var _run_id := ""
var _seed := DEFAULT_SEED
var _started_at := ""
var _started_ms := 0
var _force_failure := false
var _require_screenshots := false
var _record := false
var _failures := 0
var _warnings: Array[String] = []
var _checks: Array[Dictionary] = []
var _artifacts: Array[Dictionary] = []
var _metrics: Dictionary = {}
var _screenshot_index := 0
var _checkpoints: Array[String] = []


func _initialize() -> void:
	_scenario = OS.get_environment("MANDATE_VALIDATION_SCENARIO")
	_artifact_dir = OS.get_environment("MANDATE_VALIDATION_ARTIFACT_DIR")
	_run_id = OS.get_environment("MANDATE_VALIDATION_RUN_ID")
	_seed = int(OS.get_environment("MANDATE_VALIDATION_SEED")) if OS.has_environment("MANDATE_VALIDATION_SEED") else DEFAULT_SEED
	_started_at = OS.get_environment("MANDATE_VALIDATION_STARTED_AT")
	_force_failure = OS.get_environment("MANDATE_VALIDATION_FORCE_FAILURE") == "1"
	_require_screenshots = OS.get_environment("MANDATE_VALIDATION_REQUIRE_SCREENSHOTS") == "1"
	_record = OS.get_environment("MANDATE_VALIDATION_RECORD") == "1"
	_started_ms = Time.get_ticks_msec()
	call_deferred("_run")


func _run() -> void:
	if _artifact_dir.is_empty():
		push_error("MANDATE_VALIDATION_ARTIFACT_DIR is required")
		quit(2)
		return

	DirAccess.make_dir_recursive_absolute(_artifact_dir)
	seed(_seed)

	match _scenario:
		"basic_selection_move":
			await _scenario_basic_selection_move()
		"strategic_zoom_transition":
			await _scenario_strategic_zoom_transition()
		"oak_grove_showcase":
			await _scenario_oak_grove_showcase()
		"airfield_fighter_ferry":
			await _scenario_airfield_fighter_ferry()
		"road_construction":
			await _scenario_road_construction()
		_:
			_check(false, "scenario.known", "Unknown validation scenario", {"scenario": _scenario})
	_check(_has_check("scenario.completed"), "framework.scenario_returned", "The scenario reached its explicit completion sentinel")

	if _force_failure:
		_check(false, "framework.forced_failure", "Intentional failure requested for artifact-pipeline validation")

	_write_report()
	quit(1 if _failures > 0 else 0)


func _create_skirmish() -> Node:
	var view := MAIN_SCENE.instantiate()
	root.add_child(view)
	await process_frame
	view.call("_on_start_skirmish_pressed")
	# Validation owns the tick schedule, including during rendered checkpoints.
	view.set_process(false)
	await process_frame
	return view


func _destroy_skirmish(view: Node) -> void:
	if is_instance_valid(view):
		view.queue_free()
		await process_frame


func _scenario_basic_selection_move() -> void:
	var view := await _create_skirmish()
	var engineer_id := int(view.call("_player_engineer_id"))
	_check(engineer_id > 0, "selection.engineer_exists", "The player engineer exists", {"entity_id": engineer_id})
	if engineer_id <= 0:
		await _destroy_skirmish(view)
		return

	var start := _entity_position(view, engineer_id)
	_focus_camera_on(view, engineer_id, 80.0)
	await process_frame
	var engineer_view: Node = view.get("prototype_visual_views").get(engineer_id)
	var start_surface_height := float(view.call("_terrain_height_at", start.x, start.y))
	var start_model_bottom: float = engineer_view.global_position.y + float(engineer_view.call("model_bottom_height")) if engineer_view != null else INF
	_check(engineer_view != null and absf(start_model_bottom - start_surface_height - 0.05) <= 0.02, "movement.engineer_grounded_at_start", "The engineer's lowest mesh point sits five centimeters above the rendered terrain", {"clearance_m": start_model_bottom - start_surface_height})
	var screen_position := view.get_viewport().get_camera_3d().unproject_position(view.call("_entity_world_position", engineer_id))
	_send_mouse_button(view, MOUSE_BUTTON_LEFT, screen_position)
	await process_frame
	var selected = view.get("selected_ids")
	_check(selected.has(engineer_id), "selection.click_selects_engineer", "A real left-click selects the engineer", {"selected_ids": selected})
	await _capture_checkpoint(view, "engineer_selected")

	# The engineering chassis is intentionally slow (2 m/s). Keep the smoke
	# order short enough to complete quickly while still proving real movement.
	var target := _find_valid_land_target(view, start, 30.0)
	_check(target != start, "movement.valid_target_found", "A nearby traversable order target was found", {"start": _vec2(start), "target": _vec2(target)})
	if target != start:
		var target_world := Vector3(target.x, float(view.call("_terrain_height_at", target.x, target.y)), target.y)
		var target_screen := view.get_viewport().get_camera_3d().unproject_position(target_world)
		_send_mouse_button(view, MOUSE_BUTTON_RIGHT, target_screen)
		await process_frame
		await _capture_checkpoint(view, "move_order_issued")

	var ticks := 0
	var end := start
	var maximum_grounding_error := 0.0
	while ticks < 600 and target != start:
		view.get("extension").call("update_simulation", 50.0)
		view.call("_sync_new_entities")
		view.call("_sync_unit_transforms")
		end = _entity_position(view, engineer_id)
		if engineer_view != null:
			var surface_height := float(view.call("_terrain_height_at", end.x, end.y))
			var model_bottom: float = engineer_view.global_position.y + float(engineer_view.call("model_bottom_height"))
			maximum_grounding_error = maxf(maximum_grounding_error, absf(model_bottom - surface_height - 0.05))
		ticks += 1
		if _record and ticks % 5 == 0:
			await process_frame
		if end.distance_to(target) <= 5.0:
			break

	var moved_distance := start.distance_to(end)
	var target_error := end.distance_to(target)
	_metrics["simulation_ticks"] = ticks
	_metrics["moved_distance_m"] = snappedf(moved_distance, 0.01)
	_metrics["target_error_m"] = snappedf(target_error, 0.01)
	_metrics["maximum_grounding_error_m"] = snappedf(maximum_grounding_error, 0.001)
	_check(moved_distance > 15.0, "movement.unit_moves", "The selected engineer moves after the order", {"distance_m": moved_distance, "start": _vec2(start), "end": _vec2(end)})
	_check(target_error <= 5.0, "movement.reaches_target", "The engineer reaches the bounded target tolerance", {"target_error_m": target_error, "tick_limit": 600, "ticks": ticks})
	_check(maximum_grounding_error <= 0.02, "movement.engineer_follows_rendered_terrain", "The engineer remains grounded while crossing interpolated terrain", {"maximum_grounding_error_m": maximum_grounding_error})
	await _capture_checkpoint(view, "engineer_arrived")
	await _destroy_skirmish(view)
	_check(true, "scenario.completed", "Selection and movement scenario completed")


func _scenario_strategic_zoom_transition() -> void:
	var view := await _create_skirmish()
	var engineer_id := int(view.call("_player_engineer_id"))
	_check(engineer_id > 0, "zoom.engineer_exists", "The player engineer exists", {"entity_id": engineer_id})
	if engineer_id <= 0:
		await _destroy_skirmish(view)
		return

	_focus_camera_on(view, engineer_id, 350.0)
	view.call("_sync_unit_transforms")
	var wrapper: Node = view.get("prototype_visual_views").get(engineer_id)
	var model_root: Node = wrapper.get_node_or_null("ModelRoot") if wrapper != null else null
	_check(wrapper != null, "zoom.wrapper_exists", "The engineer presentation wrapper exists")
	_check(model_root != null and model_root.visible, "zoom.tactical_model_visible", "The tactical model is visible at close zoom")
	await _capture_checkpoint(view, "tactical_model")

	var viewport_size := view.get_viewport().get_visible_rect().size
	var cursor := Vector2(viewport_size.x * 0.82, viewport_size.y * 0.28)
	for _index in range(5):
		_send_wheel(view, MOUSE_BUTTON_WHEEL_DOWN, cursor)
		await _advance_camera(view, 80)
	view.call("_sync_unit_transforms")
	_check(float(view.get("camera_distance")) >= 600.0, "zoom.reaches_strategic_distance", "Mouse-wheel input reaches strategic zoom", {"camera_distance": view.get("camera_distance")})
	_check(model_root != null and not model_root.visible, "zoom.strategic_icon_replaces_model", "The tactical model is replaced at strategic zoom")
	var selected = view.get("selected_ids")
	_check(selected.has(engineer_id), "zoom.selection_persists", "Selection persists across the strategic transition", {"selected_ids": selected})
	await _capture_checkpoint(view, "strategic_icon")

	var before: Vector2 = view.call("_screen_to_world", cursor)
	_send_wheel(view, MOUSE_BUTTON_WHEEL_UP, cursor)
	await _advance_camera(view, 100)
	var after: Vector2 = view.call("_screen_to_world", cursor)
	var anchor_error := before.distance_to(after)
	_metrics["cursor_anchor_error_m"] = snappedf(anchor_error, 0.01)
	_metrics["camera_distance"] = snappedf(float(view.get("camera_distance")), 0.01)
	_check(anchor_error <= 25.0, "zoom.cursor_anchor_preserved", "Strategic zoom remains anchored near the cursor", {"anchor_error_m": anchor_error, "cursor": _vec2(cursor)})
	await _destroy_skirmish(view)
	_check(true, "scenario.completed", "Strategic zoom scenario completed")


func _scenario_oak_grove_showcase() -> void:
	var view := await _create_skirmish()
	var tactical_views: Array = view.get("tactical_tree_views")
	var strategic_views: Array = view.get("strategic_tree_views")
	var cluster_centers: Array = view.get("forest_cluster_centers")
	var tree_positions: Array = view.get("forest_tree_positions")
	var tree_scales: Array = view.get("forest_tree_scales")
	var tree_variants: Array = view.get("forest_tree_variant_indices")
	var tree_root_depths: Array = view.get("forest_tree_root_depths")
	var tactical_count := 0
	var strategic_count := 0
	var roots_grounded := tree_root_depths.size() == tree_positions.size()
	var smallest_mesh_height := INF
	var largest_mesh_height := 0.0
	for tree_view_value in tactical_views:
		var tree_view := tree_view_value as MultiMeshInstance3D
		if tree_view == null or tree_view.multimesh == null:
			continue
		tactical_count += tree_view.multimesh.instance_count
		var mesh_height := tree_view.multimesh.mesh.get_aabb().size.y
		smallest_mesh_height = minf(smallest_mesh_height, mesh_height)
		largest_mesh_height = maxf(largest_mesh_height, mesh_height)
	for root_depth_value in tree_root_depths:
		var root_depth: float = root_depth_value
		roots_grounded = roots_grounded and root_depth >= -0.15 and root_depth <= 0.02
	var smallest_tree_scale := INF
	var largest_tree_scale := 0.0
	for scale_value in tree_scales:
		var scale: Vector2 = scale_value
		smallest_tree_scale = minf(smallest_tree_scale, minf(scale.x, scale.y))
		largest_tree_scale = maxf(largest_tree_scale, maxf(scale.x, scale.y))
	for tree_view_value in strategic_views:
		var tree_view := tree_view_value as MultiMeshInstance3D
		if tree_view != null and tree_view.multimesh != null:
			strategic_count += tree_view.multimesh.instance_count
	_check(tactical_views.size() == 3 and tactical_count == 960, "oak.variant_population", "Three user-supplied GLB forms populate the 960-tree tactical forest", {"variant_count": tactical_views.size(), "tree_count": tactical_count})
	_check(strategic_views.size() == 3 and strategic_count == tactical_count / 2, "oak.strategic_lod_population", "Strategic tree groups retain exactly half the tactical instances", {"strategic_tree_count": strategic_count, "tactical_tree_count": tactical_count})
	_check(roots_grounded, "oak.roots_follow_heightmap", "The lowest imported tree vertices sit within 15 cm below and 2 cm above the heightmap")
	_check(tree_scales.size() == tactical_count and largest_tree_scale - smallest_tree_scale >= 0.6 and largest_mesh_height - smallest_mesh_height >= 7.0, "oak.scale_variation", "User GLB instances use visibly different configured scales and imported mesh heights", {"smallest_scale": snappedf(smallest_tree_scale, 0.01), "largest_scale": snappedf(largest_tree_scale, 0.01), "smallest_mesh_height": snappedf(smallest_mesh_height, 0.01), "largest_mesh_height": snappedf(largest_mesh_height, 0.01)})
	_check(cluster_centers.size() == 9 and tree_positions.size() == 960, "oak.cluster_definition", "The default map defines nine deterministic large oak groves", {"cluster_count": cluster_centers.size(), "tree_count": tree_positions.size()})
	if cluster_centers.is_empty():
		await _destroy_skirmish(view)
		return
	var grove_center: Vector2 = cluster_centers[0]
	var grove_population := 0
	var grove_variants: Dictionary = {}
	for position_index in range(tree_positions.size()):
		var position_value: Vector2 = tree_positions[position_index]
		if position_value.distance_to(grove_center) <= 400.0:
			grove_population += 1
			grove_variants[int(tree_variants[position_index])] = true
	_check(grove_population >= 80 and grove_variants.size() == 3, "oak.large_grove_density", "The showcased grove contains a dense cluster with all three user GLB forms", {"grove_population": grove_population, "grove_variant_count": grove_variants.size()})
	var close_tree: Vector2 = tree_positions[0]
	_focus_camera_on_position(view, close_tree, 180.0)
	await _advance_camera(view, 80)
	view.call("_sync_tree_lod")
	await _capture_checkpoint(view, "close_oak_tree")
	_focus_camera_on_position(view, grove_center, 500.0)
	await _advance_camera(view, 80)
	view.call("_sync_tree_lod")
	_check(not bool(view.get("strategic_trees").visible), "oak.tactical_lod_visible", "Tactical distance shows full imported oak forms")
	await _capture_checkpoint(view, "tactical_oak_grove")
	_focus_camera_on_position(view, grove_center, 6200.0)
	await _advance_camera(view, 240)
	view.call("_sync_tree_lod")
	_check(bool(view.get("strategic_trees").visible), "oak.strategic_lod_visible", "Strategic distance switches to the reduced oak representation")
	await _capture_checkpoint(view, "strategic_oak_grove")
	_metrics["tactical_tree_count"] = tactical_count
	_metrics["strategic_tree_count"] = strategic_count
	_metrics["grove_population"] = grove_population
	_metrics["smallest_tree_scale"] = snappedf(smallest_tree_scale, 0.01)
	_metrics["largest_tree_scale"] = snappedf(largest_tree_scale, 0.01)
	_metrics["smallest_tree_mesh_height"] = snappedf(smallest_mesh_height, 0.01)
	_metrics["largest_tree_mesh_height"] = snappedf(largest_mesh_height, 0.01)
	await _destroy_skirmish(view)
	_check(true, "scenario.completed", "Oak grove showcase scenario completed")


func _scenario_airfield_fighter_ferry() -> void:
	var view := await _create_skirmish()
	var extension: Object = view.get("extension")
	var engineer_id := int(view.call("_player_engineer_id"))
	_check(engineer_id > 0, "air.engineer_exists", "The player engineer exists", {"entity_id": engineer_id})
	if engineer_id <= 0:
		await _destroy_skirmish(view)
		return

	view.call("_set_selected", engineer_id, true)
	var requested_build_position := Vector2(-12500.0, 0.0)
	var build_position: Vector2 = view.call("_resolve_structure_placement_target", 2, requested_build_position)
	_check(build_position != Vector2.INF and build_position.distance_to(requested_build_position) <= 1000.0, "air.placement_snaps_near_request", "An uneven airfield request resolves to a nearby valid operational footprint", {"requested": _vec2(requested_build_position), "resolved": _vec2(build_position), "snap_distance_m": requested_build_position.distance_to(build_position)})
	if build_position == Vector2.INF:
		await _destroy_skirmish(view)
		return
	var airfield_queued := bool(view.call("_queue_commander_structure", 2, build_position))
	_check(airfield_queued, "air.airfield_queued", "The engineer queues an airfield on valid land", {"position": _vec2(build_position)})
	var airfield_ticks := 0
	var installation_info: Array = []
	while airfield_ticks < 900 and installation_info.size() < 6:
		extension.call("update_simulation", 50.0)
		view.call("_sync_new_entities")
		installation_info = extension.call("territory_get_installation_info", build_position.x, build_position.y)
		airfield_ticks += 1
		if _record and airfield_ticks % 10 == 0:
			await process_frame
	_check(installation_info.size() >= 6, "air.airfield_completes", "The airfield completes within the bounded tick budget", {"ticks": airfield_ticks, "tick_limit": 900})
	if installation_info.size() < 6:
		await _destroy_skirmish(view)
		return

	view.call("_update_hud")
	var airfield_view := view.get_node_or_null("Built_Airfield")
	if airfield_view == null:
		# The interactive approach path normally schedules this view before the
		# native queue starts. This scenario queues at the same production method
		# after validating placement, so invoke the production presentation helper
		# once the authoritative installation proves completion.
		view.call("_spawn_completed_structure_view", 2, build_position)
		airfield_view = view.get_node_or_null("Built_Airfield")
	_check_not_null(airfield_view, "air.airfield_view_exists", "The completed airfield has its production presentation")
	if airfield_view != null:
		view.call("_set_selected_structure", airfield_view)
	_focus_camera_on_position(view, build_position, 180.0)
	await _capture_checkpoint(view, "airfield_complete")
	var before_ids: PackedInt32Array = view.get("player_entity_ids").duplicate()
	var pending_before := int(view.get("pending_player_build_types").size())
	view.call("_queue_commander_unit", 9)
	var pending_after := int(view.get("pending_player_build_types").size())
	_check(pending_after == pending_before + 1, "air.fighter_queued", "The completed airfield accepts a fighter ferry request", {"before": pending_before, "after": pending_after})

	var fighter_id := 0
	var fighter_entry := Vector2.ZERO
	var ferry_ticks := 0
	while ferry_ticks < 700 and fighter_id <= 0:
		extension.call("update_simulation", 50.0)
		view.call("_sync_new_entities")
		for entity_id_variant in view.get("player_entity_ids"):
			var entity_id := int(entity_id_variant)
			if not before_ids.has(entity_id) and int(view.get("entity_unit_types").get(entity_id, -1)) == 9:
				fighter_id = entity_id
				fighter_entry = _entity_position(view, entity_id)
				break
		ferry_ticks += 1
		if _record and ferry_ticks % 5 == 0:
			await process_frame
	_check(fighter_id > 0, "air.fighter_arrives", "A fighter entity arrives within the bounded tick budget", {"ticks": ferry_ticks, "tick_limit": 700})
	if fighter_id > 0:
		var fighter_view: Node = view.get("prototype_visual_views").get(fighter_id)
		var fighter_definition: Dictionary = fighter_view.get("_definition") if fighter_view != null else {}
		_check(int(extension.call("get_unit_faction_id", fighter_id)) == 0 and String(fighter_definition.get("visual_id", "")) == "visual.industrial.fighter.f15c.prototype", "air.default_faction_uses_f15c", "The default faction's fighter uses the imported F-15C visual", {"faction_id": int(extension.call("get_unit_faction_id", fighter_id)), "visual_id": String(fighter_definition.get("visual_id", "")), "source_asset_id": String(fighter_definition.get("source_asset_id", ""))})
		_check(fighter_entry.x < -20000.0, "air.fighter_enters_from_off_map", "The fighter is created beyond the tactical map edge", {"entry": _vec2(fighter_entry)})
		var inside_position := fighter_entry
		var ingress_ticks := 0
		while ingress_ticks < 700 and inside_position.x < -20000.0:
			extension.call("update_simulation", 50.0)
			view.call("_sync_new_entities")
			view.call("_sync_unit_transforms")
			inside_position = _entity_position(view, fighter_id)
			ingress_ticks += 1
			if _record and ingress_ticks % 2 == 0:
				await process_frame
		_check(inside_position.x >= -20000.0, "air.fighter_flies_onto_map", "The fighter crosses onto the tactical battlefield", {"position": _vec2(inside_position), "ticks": ingress_ticks})
		_metrics["fighter_ingress_ticks"] = ingress_ticks
		_focus_camera_on(view, fighter_id, 800.0)
		await _capture_checkpoint(view, "fighter_ingress")
	_metrics["airfield_completion_ticks"] = airfield_ticks
	_metrics["fighter_delivery_ticks"] = ferry_ticks
	await _destroy_skirmish(view)
	_check(true, "scenario.completed", "Airfield and fighter ferry scenario completed")


func _scenario_road_construction() -> void:
	var view := await _create_skirmish()
	var extension: Object = view.get("extension")
	var engineer := int(view.call("_player_engineer_id"))
	_check(engineer > 0, "road.engineer_exists", "Production engineer exists")
	if engineer <= 0:
		await _destroy_skirmish(view)
		return
	var start := _entity_position(view, engineer)
	var finish := start
	# Roads must span at least one navigation cell in the 40 km theater.
	for offset in [Vector2(400, 0), Vector2(0, 400), Vector2(-400, 0), Vector2(0, -400)]:
		var candidate: Vector2 = start + offset
		if bool(extension.call("validate_road_placement", start.x, start.y, candidate.x, candidate.y)):
			finish = candidate
			break
	_check(finish != start, "road.valid_route", "An authored terrain segment accepts road construction")
	if finish == start:
		await _destroy_skirmish(view)
		return
	_focus_camera_on(view, engineer, 1800.0)
	var key := InputEventKey.new()
	key.keycode = KEY_R
	key.pressed = true
	view.call("_unhandled_input", key)
	_check(bool(view.get("road_build_mode")), "road.key_arms_mode", "R arms road construction for the selected engineer")
	var start_world := Vector3(start.x, float(view.call("_terrain_height_at", start.x, start.y)), start.y)
	var start_screen: Vector2 = view.get_viewport().get_camera_3d().unproject_position(start_world)
	_send_mouse_button(view, MOUSE_BUTTON_LEFT, start_screen)
	_check(view.get("road_start") != Vector2.INF, "road.first_click", "First terrain click establishes a road endpoint")
	view.call("_update_road_preview", Vector2.ZERO)
	var preview: Material = view.get("road_preview_material")
	_check(preview != null and preview.albedo_color.r > preview.albedo_color.b, "road.water_preview_red", "Road preview rejects the water channel in red")
	var before_invalid: PackedFloat32Array = extension.call("get_road_segments")
	_check(not bool(extension.call("queue_road", engineer, 0, 0.0, 0.0, 60.0, 0.0)), "road.water_authority_rejects", "Native road command rejects a water route")
	_check(extension.call("get_road_segments") == before_invalid, "road.invalid_no_segment", "Rejected water construction creates no segment")
	view.call("_update_road_preview", finish)
	_check(preview.albedo_color.b > preview.albedo_color.r, "road.valid_preview", "Valid road preview uses the normal placement color")
	await _capture_checkpoint(view, "road_preview")
	var storage_id := int(extension.call("get_faction_production_line", 0))
	var stores_before: Array = extension.call("economy_get_storage_info", storage_id)
	var finish_world := Vector3(finish.x, float(view.call("_terrain_height_at", finish.x, finish.y)), finish.y)
	_send_mouse_button(view, MOUSE_BUTTON_LEFT, view.get_viewport().get_camera_3d().unproject_position(finish_world))
	var stores_after: Array = extension.call("economy_get_storage_info", storage_id)
	_check(not bool(view.get("road_build_mode")), "road.second_click_commits", "Second terrain click commits the road order")
	_check(stores_after.size() >= 2 and stores_after[0] < stores_before[0], "road.paid", "Road order reserves Material from the player's economy")
	_check(extension.call("get_road_segments").is_empty(), "road.not_instant", "Road has no completed segment before construction ticks")
	var ticks := 0
	while ticks < 600 and extension.call("get_road_segments").is_empty():
		extension.call("update_simulation", 50.0)
		view.call("_sync_completed_roads")
		ticks += 1
	var segments: PackedFloat32Array = extension.call("get_road_segments")
	_check(segments.size() == 6, "road.completed", "Fixed-tick construction completes exactly one road", {"ticks": ticks})
	_check(view.get("road_views").size() == 1, "road.visual_registered", "Completed native road receives its gameplay mesh")
	var road_surface_follows_terrain := false
	for road in view.get("road_views").values():
		var vertices: PackedVector3Array = road.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
		road_surface_follows_terrain = vertices.size() > 4
		for vertex in vertices:
			road_surface_follows_terrain = road_surface_follows_terrain and absf(vertex.y - float(view.call("_terrain_height_at", vertex.x, vertex.z)) - 0.12) < 0.01
	_check(road_surface_follows_terrain, "road.terrain_conforming", "Road samples its full surface against the heightfield instead of bridging endpoint heights")
	for _tick in range(20):
		extension.call("update_simulation", 50.0)
	view.call("_sync_completed_roads")
	_check(extension.call("get_road_segments") == segments and view.get("road_views").size() == 1, "road.persists_without_duplicates", "Repeated synchronization preserves the segment and one visual")
	_metrics["construction_ticks"] = ticks
	_focus_camera_on_position(view, (start + finish) * 0.5, 450.0)
	view.call("_update_hud")
	await _capture_checkpoint(view, "road_complete")
	await _destroy_skirmish(view)
	_check(true, "scenario.completed", "Road construction scenario completed")


func _focus_camera_on(view: Node, entity_id: int, distance: float) -> void:
	var position := _entity_position(view, entity_id)
	_focus_camera_on_position(view, position, distance)


func _focus_camera_on_position(view: Node, position: Vector2, distance: float) -> void:
	view.set("camera_target", Vector3(position.x, float(view.call("_terrain_height_at", position.x, position.y)), position.y))
	view.set("camera_distance", distance)
	view.set("target_camera_distance", distance)
	view.call("_update_camera", 1.0)


func _advance_camera(view: Node, frames: int) -> void:
	for _index in range(frames):
		view.call("_update_camera", 1.0 / 60.0)
		if _record and _index % 3 == 0:
			await process_frame


func _send_mouse_button(view: Node, button: MouseButton, position: Vector2) -> void:
	var pressed := InputEventMouseButton.new()
	pressed.button_index = button
	pressed.position = position
	pressed.global_position = position
	pressed.pressed = true
	view.call("_unhandled_input", pressed)
	var released := InputEventMouseButton.new()
	released.button_index = button
	released.position = position
	released.global_position = position
	released.pressed = false
	view.call("_unhandled_input", released)


func _send_wheel(view: Node, button: MouseButton, position: Vector2) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.position = position
	event.global_position = position
	event.pressed = true
	view.call("_unhandled_input", event)


func _find_valid_land_target(view: Node, origin: Vector2, radius: float) -> Vector2:
	var extension: Object = view.get("extension")
	for direction in [Vector2.RIGHT, Vector2.DOWN, Vector2.LEFT, Vector2.UP, Vector2(0.707, 0.707)]:
		var candidate: Vector2 = origin + direction * radius
		if bool(extension.call("is_land_position", candidate.x, candidate.y)):
			return candidate
	return origin


func _find_valid_build_position(view: Node, preferred: Vector2, unit_type: int) -> Vector2:
	var extension: Object = view.get("extension")
	if bool(extension.call("validate_structure_placement", unit_type, preferred.x, preferred.y)):
		return preferred
	for ring in range(1, 16):
		var radius := float(ring) * 400.0
		for index in range(16):
			var angle := TAU * float(index) / 16.0
			var candidate := preferred + Vector2(cos(angle), sin(angle)) * radius
			if bool(extension.call("validate_structure_placement", unit_type, candidate.x, candidate.y)):
				return candidate
	return preferred


func _entity_position(view: Node, entity_id: int) -> Vector2:
	var extension: Object = view.get("extension")
	return Vector2(float(extension.call("get_unit_x", entity_id)), float(extension.call("get_unit_y", entity_id)))


func _capture_checkpoint(view: Node, checkpoint: String) -> void:
	_checkpoints.append(checkpoint)
	if DisplayServer.get_name() == "headless":
		var message := "Screenshot checkpoint '%s' skipped because no render-capable display is available" % checkpoint
		_warnings.append(message)
		if _require_screenshots:
			_check(false, "capture.%s" % checkpoint, message)
		return

	_screenshot_index += 1
	var screenshot_dir := _artifact_dir.path_join("screenshots")
	DirAccess.make_dir_recursive_absolute(screenshot_dir)
	# Scene nodes queued during fixed-tick orchestration need more than one frame
	# to enter the rendered tree. Capturing the first frame can produce a valid
	# PNG containing only the clear color and one early HUD node.
	for _settle_frame in range(3):
		await process_frame
	RenderingServer.force_draw()
	await RenderingServer.frame_post_draw
	var image := view.get_viewport().get_texture().get_image()
	var path := screenshot_dir.path_join("%02d_%s.png" % [_screenshot_index, checkpoint])
	var error := image.save_png(path)
	var sanity := _image_sanity(image)
	var capture_valid := error == OK and image.get_width() > 0 and image.get_height() > 0 and float(sanity.mean_luminance) > 0.01 and float(sanity.luminance_range) > 0.02
	_check(capture_valid, "capture.%s" % checkpoint, "Screenshot checkpoint is non-empty and visually non-blank", {"path": path, "width": image.get_width(), "height": image.get_height(), "error": error, "sanity": sanity})
	if capture_valid:
		_artifacts.append({
			"kind": "screenshot",
			"checkpoint": checkpoint,
			"path": "screenshots/%s" % path.get_file(),
			"width": image.get_width(),
			"height": image.get_height()
		})


func _image_sanity(image: Image) -> Dictionary:
	if image.is_empty():
		return {"samples": 0, "mean_luminance": 0.0, "luminance_range": 0.0}
	var minimum := 1.0
	var maximum := 0.0
	var total := 0.0
	var samples := 0
	var step_x := maxi(1, image.get_width() / 40)
	var step_y := maxi(1, image.get_height() / 24)
	for y in range(step_y / 2, image.get_height(), step_y):
		for x in range(step_x / 2, image.get_width(), step_x):
			var luminance := image.get_pixel(x, y).get_luminance()
			minimum = minf(minimum, luminance)
			maximum = maxf(maximum, luminance)
			total += luminance
			samples += 1
	return {
		"samples": samples,
		"mean_luminance": total / float(samples) if samples > 0 else 0.0,
		"luminance_range": maximum - minimum if samples > 0 else 0.0
	}


func _check(condition: bool, check_id: String, message: String, diagnostics: Dictionary = {}) -> void:
	var status := "PASS" if condition else "FAIL"
	_checks.append({
		"id": check_id,
		"status": status,
		"message": message,
		"diagnostics": diagnostics
	})
	if condition:
		print("VALIDATION PASS [%s] %s" % [check_id, message])
	else:
		_failures += 1
		push_error("VALIDATION FAIL [%s] %s | %s" % [check_id, message, JSON.stringify(diagnostics)])


func _check_equal(actual: Variant, expected: Variant, check_id: String, message: String) -> void:
	_check(actual == expected, check_id, message, {"actual": actual, "expected": expected})


func _check_near(actual: float, expected: float, tolerance: float, check_id: String, message: String) -> void:
	_check(absf(actual - expected) <= tolerance, check_id, message, {"actual": actual, "expected": expected, "tolerance": tolerance})


func _check_in_range(actual: float, minimum: float, maximum: float, check_id: String, message: String) -> void:
	_check(actual >= minimum and actual <= maximum, check_id, message, {"actual": actual, "minimum": minimum, "maximum": maximum})


func _check_not_null(actual: Variant, check_id: String, message: String) -> void:
	_check(actual != null, check_id, message)


func _has_check(check_id: String) -> bool:
	for check in _checks:
		if check.get("id") == check_id and check.get("status") == "PASS":
			return true
	return false


func _write_report() -> void:
	var duration_ms := Time.get_ticks_msec() - _started_ms
	_metrics["duration_ms"] = duration_ms
	_metrics["check_count"] = _checks.size()
	_metrics["failure_count"] = _failures
	var report := {
		"schemaVersion": 1,
		"scenarioId": _scenario,
		"description": SCENARIO_DESCRIPTIONS.get(_scenario, "Unknown scenario"),
		"runId": _run_id,
		"seed": _seed,
		"status": "PASS" if _failures == 0 else "FAIL",
		"startedAt": _started_at,
		"durationMs": duration_ms,
		"environment": {
			"godot_version": Engine.get_version_info().get("string", "unknown"),
			"display_server": DisplayServer.get_name(),
			"renderer": RenderingServer.get_current_rendering_method(),
			"headless": DisplayServer.get_name() == "headless"
		},
		"assertions": _checks,
		"checkpoints": _checkpoints,
		"metrics": _metrics,
		"artifacts": _artifacts,
		"warnings": _warnings,
		"errors": _failed_check_messages()
	}
	var report_path := _artifact_dir.path_join("report.json")
	var file := FileAccess.open(report_path, FileAccess.WRITE)
	if file == null:
		push_error("Unable to write validation report: %s" % report_path)
		return
	file.store_string(JSON.stringify(report, "  "))
	file.store_line("")
	print("VALIDATION REPORT %s" % report_path)


func _failed_check_messages() -> Array[String]:
	var messages: Array[String] = []
	for check in _checks:
		if check.get("status") == "FAIL":
			messages.append("%s: %s" % [check.get("id"), check.get("message")])
	return messages


func _vec2(value: Vector2) -> Dictionary:
	return {"x": snappedf(value.x, 0.01), "y": snappedf(value.y, 0.01)}
