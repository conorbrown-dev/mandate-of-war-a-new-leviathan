extends Node3D

const DEFAULT_UNIT_COUNT := 1000
const DEFAULT_SKIRMISH_PATH := "res://scenarios/two_landmass_skirmish.json"
const UNIT_SPACING := 3.0
const UNIT_BASE_COLOR := Color(0.12, 0.62, 1.0, 1.0)
const UNIT_SELECTED_COLOR := Color(1.0, 0.72, 0.08, 1.0)
const PLAYER_FACTION_COLOR := Color(0.12, 0.62, 1.0, 1.0)
const AI_FACTION_COLOR := Color(0.92, 0.20, 0.18, 1.0)
const HUMAN_PLAYER_ID := 0
const CAMERA_MIN_DISTANCE := 24.0
const CAMERA_MAX_DISTANCE := 260.0
const SIMULATION_WORLD_CENTER_SPAN := 319.0

@onready var camera: Camera3D = $Camera3D
@onready var unit_view: MultiMeshInstance3D = $Units
@onready var west_landmass: MeshInstance3D = $WestLandmass
@onready var east_landmass: MeshInstance3D = $EastLandmass
@onready var debug_label: Label = $HUD/DebugPanel/DebugLabel
@onready var selection_rect: ColorRect = $HUD/SelectionRect
@onready var debug_panel: ColorRect = $HUD/DebugPanel
@onready var help_label: Label = $HUD/HelpLabel
@onready var startup_overlay: ColorRect = $HUD/StartupOverlay
@onready var scenario_title: Label = $HUD/StartupOverlay/Panel/VBox/ScenarioTitle
@onready var scenario_description: Label = $HUD/StartupOverlay/Panel/VBox/ScenarioDescription
@onready var scenario_status: Label = $HUD/StartupOverlay/Panel/VBox/ScenarioStatus
@onready var start_button: Button = $HUD/StartupOverlay/Panel/VBox/StartButton

var extension: Object
var unit_multimesh: MultiMesh
var entity_ids := PackedInt32Array()
var entity_to_instance: Dictionary = {}
var entity_base_colors: Dictionary = {}
var selected_ids := PackedInt32Array()
var player_entity_ids := PackedInt32Array()
var ai_entity_ids := PackedInt32Array()
var unit_positions := PackedFloat32Array()
var scenario_definition: Dictionary = {}
var match_started := false

func _get_visible_unit_count() -> int:
	var env_count := OS.get_environment("UNIT_COUNT")
	if env_count != "":
		if env_count.is_valid_int():
			var count := int(env_count)
			if count > 0 and count <= 100000:
				return count
	print("Warning: UNIT_COUNT env var invalid or missing, using default")
	return DEFAULT_UNIT_COUNT

var camera_target := Vector3.ZERO
var camera_distance := 100.0
var target_camera_distance := 100.0
var selecting := false
var panning := false
var selection_start := Vector2.ZERO
var selection_end := Vector2.ZERO
var hud_elapsed := 0.0
var profile_target_frames := 0
var profile_warmup_frames := 60
var profile_seen_frames := 0
var profile_sample_count := 0
var profile_frame_ms_total := 0.0
var profile_process_ms_total := 0.0
var profile_simulation_ms_total := 0.0
var profile_position_fetch_ms_total := 0.0
var profile_multimesh_upload_ms_total := 0.0
var profile_fps_total := 0.0
var profile_draw_calls_total := 0.0
var profile_max_process_ms := 0.0
var profile_max_simulation_ms := 0.0
var profile_max_position_fetch_ms := 0.0
var profile_max_multimesh_upload_ms := 0.0


func _setup_economy_for_player_1() -> void:
	var NODE_METAL := 0
	var NODE_ENERGY := 1

	extension.call("economy_add_resource_node", NODE_METAL, 0.0, 0.0, 1000.0, 1)
	extension.call("economy_add_resource_node", NODE_ENERGY, 0.0, 0.0, 1000.0, 2)
	extension.call("economy_add_extractor", 1, 0.0, 0.0, NODE_METAL, 10.0)
	extension.call("economy_add_extractor", 2, 0.0, 0.0, NODE_ENERGY, 10.0)
	extension.call("economy_add_storage", 1, 0.0, 0.0, 5000.0, 5000.0, 1000.0)
	extension.call("economy_add_production_line", 1, 1, 100.0, 100.0, 5)


func _ready() -> void:
	var content_data_root := ProjectSettings.globalize_path("res://../../data").simplify_path()
	OS.set_environment("RTS_DATA_ROOT", content_data_root)

	if not ClassDB.class_exists("RtsExtension"):
		push_error("RtsExtension is unavailable. Build rts_gdextension and check rts.gdextension.")
		set_process(false)
		return

	extension = ClassDB.instantiate("RtsExtension")
	if extension == null:
		push_error("RtsExtension is registered but could not be instantiated.")
		set_process(false)
		return

	var profile_frames := OS.get_environment("RTS_PROFILE_FRAMES")
	if profile_frames.is_valid_int() and int(profile_frames) > 0:
		profile_target_frames = int(profile_frames)
		_start_scale_profile()
		return

	start_button.pressed.connect(_on_start_skirmish_pressed)
	
	var map_data: Dictionary = extension.call("map_loader_load_map", DEFAULT_SKIRMISH_PATH)
	if not map_data.get("ok", false):
		scenario_status.text = "Scenario unavailable: %s" % map_data.get("error", "unknown load error")
		start_button.disabled = true
		return
	
	scenario_definition = {
		"display_name": map_data.get("name", "Unknown"),
		"description": map_data.get("description", ""),
		"theater": {
			"width": float(map_data.get("width", 0)),
			"height": float(map_data.get("height", 0)),
			"landmasses": _build_landmasses_from_map(map_data)
		},
		"player": _extract_player_from_map(map_data),
		"ai": _extract_ai_from_map(map_data),
		"victory": {
			"type": "last_faction_standing",
			"description": "Destroy all enemy factions"
		}
	}
	_configure_scenario_theater()
	scenario_title.text = scenario_definition.display_name
	scenario_description.text = scenario_definition.description
	scenario_status.text = "Elite Precision vs Mass Warfare\n%s" % scenario_definition.victory.description
	if OS.get_environment("RTS_AUTO_START_SKIRMISH") == "1":
		_on_start_skirmish_pressed.call_deferred()


# warning-ignore-all-return-values
func _build_landmasses_from_map(map_data: Dictionary) -> Array:
	var landmasses: Array = []
	var landmass_ids: PackedInt32Array = map_data.get("landmass_ids", PackedInt32Array())
	var spawn_x: PackedFloat32Array = map_data.get("spawn_x", PackedFloat32Array())
	var spawn_y: PackedFloat32Array = map_data.get("spawn_y", PackedFloat32Array())
	
	for index in range(landmass_ids.size()):
		var landmass_id: int = int(landmass_ids[index])
		var center_x: float = float(spawn_x[index]) if index < spawn_x.size() else 0.0
		var center_y: float = float(spawn_y[index]) if index < spawn_y.size() else 0.0
		var size_half: float = float(map_data.get("width", 320)) / 3.0
		landmasses.append({
			"id": str(landmass_id),
			"center": [center_x, center_y],
			"size": [size_half * 2.0, size_half * 2.0]
		})
	
	return landmasses


func _extract_player_from_map(map_data: Dictionary) -> Dictionary:
	var spawn_x: PackedFloat32Array = map_data.get("spawn_x", PackedFloat32Array())
	var spawn_y: PackedFloat32Array = map_data.get("spawn_y", PackedFloat32Array())
	var spawn_faction: PackedStringArray = map_data.get("spawn_faction", PackedStringArray())
	
	for index in range(spawn_faction.size()):
		if String(spawn_faction[index]) == "faction_0":
			return {
				"faction_id": 0,
				"spawn": [spawn_x[index], spawn_y[index]],
				"units": []
			}
	
	return {"faction_id": 0, "spawn": [0.0, 0.0], "units": []}


func _extract_ai_from_map(map_data: Dictionary) -> Dictionary:
	var spawn_x: PackedFloat32Array = map_data.get("spawn_x", PackedFloat32Array())
	var spawn_y: PackedFloat32Array = map_data.get("spawn_y", PackedFloat32Array())
	var spawn_faction: PackedStringArray = map_data.get("spawn_faction", PackedStringArray())
	
	for index in range(spawn_faction.size()):
		if String(spawn_faction[index]) == "faction_1":
			return {
				"faction_id": 1,
				"spawn": [spawn_x[index], spawn_y[index]],
				"units": []
			}
	
	return {"faction_id": 1, "spawn": [0.0, 0.0], "units": []}


func _configure_scenario_theater() -> void:
	var landmass_nodes: Array[MeshInstance3D] = [west_landmass, east_landmass]
	var landmasses: Array = scenario_definition.theater.landmasses
	for index in range(landmass_nodes.size()):
		var definition: Dictionary = landmasses[index]
		var mesh := landmass_nodes[index].mesh.duplicate() as PlaneMesh
		mesh.size = Vector2(float(definition["size"][0]), float(definition["size"][1]))
		landmass_nodes[index].mesh = mesh
		landmass_nodes[index].position = Vector3(
			float(definition.center[0]),
			0.12,
			float(definition.center[1])
		)


func _start_scale_profile() -> void:
	extension.call("start_simulation")
	match_started = true
	_setup_economy_for_player_1()
	var unit_count := _get_visible_unit_count()
	_create_unit_multimesh(unit_count)
	_spawn_units(unit_count)
	startup_overlay.visible = false
	debug_panel.visible = true
	help_label.visible = true
	_update_camera(1.0)
	_update_hud()
	print("RtsExtension loaded; spawned %d batched profile units" % entity_ids.size())


func _on_start_skirmish_pressed() -> void:
	if match_started or scenario_definition.is_empty():
		return

	extension.call("start_simulation")
	match_started = true
	_setup_economy_for_player_1()
	var total_units := _scenario_unit_count(scenario_definition.player) + _scenario_unit_count(scenario_definition.ai)
	_create_unit_multimesh(total_units)
	if not _spawn_skirmish_units():
		push_error("Failed to create the validated skirmish starting forces")
		extension.call("stop_simulation")
		match_started = false
		return

	startup_overlay.visible = false
	debug_panel.visible = true
	help_label.visible = true
	camera_target = Vector3.ZERO
	_update_camera(1.0)
	_update_hud()
	print("Started %s with %d player units and %d AI units" % [
		scenario_definition.display_name,
		player_entity_ids.size(),
		ai_entity_ids.size(),
	])


func _exit_tree() -> void:
	if extension != null and match_started:
		extension.call("stop_simulation")


func _process(delta: float) -> void:
	if extension == null or not match_started:
		return

	var simulation_start_us := Time.get_ticks_usec()
	extension.call("update_simulation", delta * 1000.0)
	var simulation_call_ms := float(Time.get_ticks_usec() - simulation_start_us) / 1000.0
	_update_keyboard_pan(delta)
	_update_camera(delta)
	var sync_timings := _sync_unit_transforms()
	_record_profile_frame(delta, simulation_call_ms, sync_timings.x, sync_timings.y)

	hud_elapsed += delta
	if hud_elapsed >= 0.2:
		hud_elapsed = 0.0
		_update_hud()


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		get_tree().quit()
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_X:
		_issue_stop_order()
		return

	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			target_camera_distance = clampf(target_camera_distance * 0.86, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			target_camera_distance = clampf(target_camera_distance * 1.16, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE)
		elif event.button_index == MOUSE_BUTTON_MIDDLE:
			panning = event.pressed
		elif event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				selecting = true
				selection_start = event.position
				selection_end = event.position
				_update_selection_rect()
			else:
				_finish_selection(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			_issue_attack_order(event.position)

	if event is InputEventMouseMotion:
		if selecting:
			selection_end = event.position
			_update_selection_rect()
		elif panning:
			var pan_scale := camera_distance * 0.0025
			camera_target.x -= event.relative.x * pan_scale
			camera_target.z -= event.relative.y * pan_scale


func _create_unit_multimesh(instance_count: int) -> void:
	entity_ids.clear()
	entity_to_instance.clear()
	entity_base_colors.clear()
	selected_ids.clear()
	player_entity_ids.clear()
	ai_entity_ids.clear()
	unit_positions.clear()

	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.roughness = 0.68

	var mesh := BoxMesh.new()
	mesh.size = Vector3(1.5, 0.8, 2.0)
	mesh.material = material

	unit_multimesh = MultiMesh.new()
	unit_multimesh.transform_format = MultiMesh.TRANSFORM_3D
	unit_multimesh.use_colors = true
	unit_multimesh.mesh = mesh
	unit_multimesh.instance_count = instance_count
	unit_multimesh.visible_instance_count = instance_count
	unit_view.multimesh = unit_multimesh


func _spawn_units(count: int) -> void:
	var columns := ceili(sqrt(float(count)))
	var rows := ceili(float(count) / float(columns))
	var largest_dimension := maxi(columns - 1, rows - 1)
	var spawn_spacing := UNIT_SPACING
	if largest_dimension > 0:
		spawn_spacing = minf(spawn_spacing, SIMULATION_WORLD_CENTER_SPAN / float(largest_dimension))
	var half_width := float(columns - 1) * spawn_spacing * 0.5
	var half_height := float(rows - 1) * spawn_spacing * 0.5

	for index in range(count):
		var x := float(index % columns) * spawn_spacing - half_width
		var z := float(index / columns) * spawn_spacing - half_height
		var entity_id: int = extension.call("create_unit", x, z)
		entity_ids.append(entity_id)
		entity_to_instance[entity_id] = index
		entity_base_colors[entity_id] = UNIT_BASE_COLOR
		player_entity_ids.append(entity_id)
		unit_positions.append(x)
		unit_positions.append(z)
		unit_multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY, Vector3(x, 0.4, z)))
		unit_multimesh.set_instance_color(index, UNIT_BASE_COLOR)


func _scenario_unit_count(side: Dictionary) -> int:
	var total := 0
	for unit_definition in side.units:
		total += int(unit_definition.count)
	return total


func _spawn_skirmish_units() -> bool:
	return _spawn_faction_units(scenario_definition.player, PLAYER_FACTION_COLOR, true) \
		and _spawn_faction_units(scenario_definition.ai, AI_FACTION_COLOR, false)


func _spawn_faction_units(side: Dictionary, color: Color, player_controlled: bool) -> bool:
	var faction_id := int(side.faction_id)
	var spawn_x := float(side.spawn[0])
	var spawn_y := float(side.spawn[1])
	var side_unit_count := _scenario_unit_count(side)
	var columns := ceili(sqrt(float(side_unit_count)))
	var side_index := 0

	for unit_definition in side.units:
		var unit_type := int(unit_definition.unit_type)
		for _unit_number in range(int(unit_definition.count)):
			var column := side_index % columns
			var row := side_index / columns
			var world_x := spawn_x + (float(column) - float(columns - 1) * 0.5) * 5.0
			var world_y := spawn_y + (float(row) - 2.0) * 5.0
			var entity_id: int = extension.call("create_unit_with_type", world_x, world_y, unit_type, faction_id)
			if entity_id < 0:
				return false

			var instance_index := entity_ids.size()
			entity_ids.append(entity_id)
			entity_to_instance[entity_id] = instance_index
			entity_base_colors[entity_id] = color
			unit_positions.append(world_x)
			unit_positions.append(world_y)
			unit_multimesh.set_instance_transform(instance_index, Transform3D(Basis.IDENTITY, Vector3(world_x, 0.4, world_y)))
			unit_multimesh.set_instance_color(instance_index, color)
			if player_controlled:
				player_entity_ids.append(entity_id)
			else:
				ai_entity_ids.append(entity_id)
			side_index += 1
	return true


func _sync_unit_transforms() -> Vector2:
	var fetch_start_us := Time.get_ticks_usec()
	var latest_positions: PackedFloat32Array = extension.call("get_unit_positions", entity_ids)
	var fetch_ms := float(Time.get_ticks_usec() - fetch_start_us) / 1000.0
	if latest_positions.size() != entity_ids.size() * 2:
		push_error("Native position snapshot size did not match entity IDs")
		return Vector2(fetch_ms, 0.0)
	unit_positions = latest_positions
	var upload_start_us := Time.get_ticks_usec()
	for index in range(entity_ids.size()):
		var x := unit_positions[index * 2]
		var z := unit_positions[index * 2 + 1]
		unit_multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY, Vector3(x, 0.4, z)))
	var upload_ms := float(Time.get_ticks_usec() - upload_start_us) / 1000.0
	return Vector2(fetch_ms, upload_ms)


func _validate_state() -> void:
	var count: int = extension.call("get_entity_count")
	if count != entity_ids.size():
		push_error("Entity count drift: expected %d, got %d" % [entity_ids.size(), count])
	
	var latest_positions: PackedFloat32Array = extension.call("get_unit_positions", entity_ids)
	if latest_positions.size() != entity_ids.size() * 2:
		push_error("Position snapshot size mismatch: expected %d, got %d" % [entity_ids.size() * 2, latest_positions.size()])
		return
	
	for i in range(entity_ids.size()):
		var x: float = latest_positions[i * 2]
		var z: float = latest_positions[i * 2 + 1]
		if not is_finite(x) or not is_finite(z):
			push_error("Invalid position for entity %d: (%.2f, %.2f)" % [entity_ids[i], x, z])

func _record_profile_frame(delta: float, simulation_ms: float, fetch_ms: float, upload_ms: float) -> void:
	if profile_target_frames <= 0:
		return

	profile_seen_frames += 1
	if profile_seen_frames == profile_warmup_frames / 2:
		extension.call("move_units_formation", entity_ids, 90.0, 0.0, 0.8)
	if profile_seen_frames <= profile_warmup_frames:
		return

	_validate_state()
	
	var process_ms := float(Performance.get_monitor(Performance.TIME_PROCESS)) * 1000.0
	profile_sample_count += 1
	profile_frame_ms_total += delta * 1000.0
	profile_process_ms_total += process_ms
	profile_simulation_ms_total += simulation_ms
	profile_position_fetch_ms_total += fetch_ms
	profile_multimesh_upload_ms_total += upload_ms
	profile_fps_total += float(Performance.get_monitor(Performance.TIME_FPS))
	profile_draw_calls_total += float(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
	profile_max_process_ms = maxf(profile_max_process_ms, process_ms)
	profile_max_simulation_ms = maxf(profile_max_simulation_ms, simulation_ms)
	profile_max_position_fetch_ms = maxf(profile_max_position_fetch_ms, fetch_ms)
	profile_max_multimesh_upload_ms = maxf(profile_max_multimesh_upload_ms, upload_ms)

	if profile_sample_count >= profile_target_frames:
		_print_profile_result()
		get_tree().quit()


func _print_profile_result() -> void:
	var samples := float(profile_sample_count)
	print("RTS_GRAPHICS_PROFILE units=%d samples=%d" % [entity_ids.size(), profile_sample_count])
	print("  frame_delta_avg_ms=%.3f process_avg_ms=%.3f process_max_ms=%.3f fps_avg=%.2f" % [
		profile_frame_ms_total / samples,
		profile_process_ms_total / samples,
		profile_max_process_ms,
		profile_fps_total / samples,
	])
	print("  simulation_call_avg_ms=%.3f simulation_call_max_ms=%.3f" % [
		profile_simulation_ms_total / samples,
		profile_max_simulation_ms,
	])
	print("  position_fetch_avg_ms=%.3f position_fetch_max_ms=%.3f" % [
		profile_position_fetch_ms_total / samples,
		profile_max_position_fetch_ms,
	])
	print("  multimesh_upload_avg_ms=%.3f multimesh_upload_max_ms=%.3f" % [
		profile_multimesh_upload_ms_total / samples,
		profile_max_multimesh_upload_ms,
	])
	print("  draw_calls_avg=%.2f static_memory_mib=%.2f video_memory_mib=%.2f" % [
		profile_draw_calls_total / samples,
		float(Performance.get_monitor(Performance.MEMORY_STATIC)) / 1048576.0,
		float(Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED)) / 1048576.0,
	])


func _update_keyboard_pan(delta: float) -> void:
	var direction := Vector2.ZERO
	if Input.is_action_pressed("ui_left") or Input.is_key_pressed(KEY_A):
		direction.x -= 1.0
	if Input.is_action_pressed("ui_right") or Input.is_key_pressed(KEY_D):
		direction.x += 1.0
	if Input.is_action_pressed("ui_up") or Input.is_key_pressed(KEY_W):
		direction.y -= 1.0
	if Input.is_action_pressed("ui_down") or Input.is_key_pressed(KEY_S):
		direction.y += 1.0

	if direction != Vector2.ZERO:
		direction = direction.normalized()
		var speed := maxf(18.0, camera_distance * 0.7)
		camera_target += Vector3(direction.x, 0.0, direction.y) * speed * delta


func _update_camera(delta: float) -> void:
	camera_distance = lerpf(camera_distance, target_camera_distance, clampf(delta * 9.0, 0.0, 1.0))
	var desired_position := camera_target + Vector3(0.0, camera_distance * 0.78, camera_distance)
	camera.global_position = camera.global_position.lerp(desired_position, clampf(delta * 10.0, 0.0, 1.0))
	camera.look_at(camera_target, Vector3.UP)


func _finish_selection(mouse_position: Vector2) -> void:
	if not selecting:
		return

	selecting = false
	selection_end = mouse_position
	selection_rect.visible = false

	var additive := Input.is_key_pressed(KEY_SHIFT)
	if not additive:
		_clear_selection()

	if selection_start.distance_to(selection_end) < 5.0:
		_select_nearest(selection_end)
	else:
		_select_in_rectangle(_selection_bounds())


func _select_nearest(screen_position: Vector2) -> void:
	var closest_id := -1
	var closest_distance := 14.0
	for entity_id in player_entity_ids:
		var world_position := _entity_world_position(entity_id)
		if camera.is_position_behind(world_position):
			continue
		var distance := camera.unproject_position(world_position).distance_to(screen_position)
		if distance < closest_distance:
			closest_distance = distance
			closest_id = entity_id

	if closest_id >= 0:
		_set_selected(closest_id, true)


func _select_in_rectangle(bounds: Rect2) -> void:
	for entity_id in player_entity_ids:
		var world_position := _entity_world_position(entity_id)
		if not camera.is_position_behind(world_position) and bounds.has_point(camera.unproject_position(world_position)):
			_set_selected(entity_id, true)


func _clear_selection() -> void:
	for entity_id in selected_ids:
		_set_instance_color(entity_id, entity_base_colors.get(entity_id, UNIT_BASE_COLOR))
	selected_ids.clear()


func _set_selected(entity_id: int, selected: bool) -> void:
	if selected and not selected_ids.has(entity_id):
		selected_ids.append(entity_id)
		_set_instance_color(entity_id, UNIT_SELECTED_COLOR)
	elif not selected and selected_ids.has(entity_id):
		selected_ids.remove_at(selected_ids.find(entity_id))
		_set_instance_color(entity_id, entity_base_colors.get(entity_id, UNIT_BASE_COLOR))


func _set_instance_color(entity_id: int, color: Color) -> void:
	var instance_index: int = entity_to_instance.get(entity_id, -1)
	if instance_index >= 0:
		unit_multimesh.set_instance_color(instance_index, color)


func _issue_move_order(screen_position: Vector2) -> void:
	if selected_ids.is_empty():
		return

	var ray_origin := camera.project_ray_origin(screen_position)
	var ray_direction := camera.project_ray_normal(screen_position)
	var intersection = Plane(Vector3.UP, 0.0).intersects_ray(ray_origin, ray_direction)
	if intersection == null:
		return

	var target: Vector3 = intersection
	var accepted_count: int = extension.call(
		"issue_move_commands",
		selected_ids,
		HUMAN_PLAYER_ID,
		target.x,
		target.z,
		UNIT_SPACING
	)
	if accepted_count != selected_ids.size():
		push_warning("Move order rejected by authoritative command validation")


func _issue_stop_order() -> void:
	if selected_ids.is_empty():
		return
	var accepted_count: int = extension.call("issue_stop_commands", selected_ids, HUMAN_PLAYER_ID)
	if accepted_count != selected_ids.size():
		push_warning("Stop order rejected by authoritative command validation")


func _issue_attack_order(screen_position: Vector2) -> void:
	if selected_ids.is_empty():
		return

	var ray_origin := camera.project_ray_origin(screen_position)
	var ray_direction := camera.project_ray_normal(screen_position)
	var intersection = Plane(Vector3.UP, 0.0).intersects_ray(ray_origin, ray_direction)
	if intersection == null:
		return

	var target: Vector3 = intersection
	var mouse_x := target.x
	var mouse_z := target.z

	var nearest_enemy_id: int = -1
	var nearest_distance := 9999.0

	for entity_id in player_entity_ids:
		var entity_pos := _entity_world_position(entity_id)
		var dx := entity_pos.x - mouse_x
		var dz := entity_pos.z - mouse_z
		var dist := sqrt(dx * dx + dz * dz)
		if dist < nearest_distance:
			nearest_distance = dist
			nearest_enemy_id = entity_id

	if nearest_enemy_id >= 0:
		var accepted_count: int = extension.call("issue_attack_commands", selected_ids, HUMAN_PLAYER_ID, nearest_enemy_id)
		if accepted_count != selected_ids.size():
			push_warning("Attack order rejected by authoritative command validation")


func _entity_world_position(entity_id: int) -> Vector3:
	var instance_index: int = entity_to_instance.get(entity_id, -1)
	if instance_index < 0 or instance_index * 2 + 1 >= unit_positions.size():
		return Vector3.ZERO
	var x := unit_positions[instance_index * 2]
	var z := unit_positions[instance_index * 2 + 1]
	return Vector3(x, 0.4, z)


func _update_selection_rect() -> void:
	selection_rect.visible = true
	var bounds := _selection_bounds()
	selection_rect.position = bounds.position
	selection_rect.size = bounds.size


func _selection_bounds() -> Rect2:
	var top_left := Vector2(
		minf(selection_start.x, selection_end.x),
		minf(selection_start.y, selection_end.y)
	)
	return Rect2(top_left, (selection_end - selection_start).abs())


func _update_hud() -> void:
	var scenario_name: String = scenario_definition.get("display_name", "Scale profile")
	debug_label.text = "%s\nPlayer: %d  Enemy: %d  Selected: %d\nFPS: %d  Sim: %.2f ms" % [
		scenario_name,
		player_entity_ids.size(),
		ai_entity_ids.size(),
		selected_ids.size(),
		Engine.get_frames_per_second(),
		extension.call("get_simulation_tick_ms"),
	]
