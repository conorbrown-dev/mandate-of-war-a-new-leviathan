extends Node3D

const DEFAULT_UNIT_COUNT := 1000
const DEFAULT_SKIRMISH_PATH := "res://scenarios/commander_start_skirmish.json"
const UNIT_SPACING := 3.0
const UNIT_BASE_COLOR := Color(0.12, 0.62, 1.0, 1.0)
const UNIT_SELECTED_COLOR := Color(1.0, 0.72, 0.08, 1.0)
const PLAYER_FACTION_COLOR := Color(0.12, 0.62, 1.0, 1.0)
const AI_FACTION_COLOR := Color(0.92, 0.20, 0.18, 1.0)
const HUMAN_PLAYER_ID := 0
const BUILD_UNIT_SHORTCUTS := ["1", "4", "5", "9", "0", "P"]
const CAMERA_MIN_DISTANCE := 24.0
const CAMERA_MAX_DISTANCE := 26000.0
const CAMERA_MIN_FAR_DISTANCE := 120000.0
const CAMERA_TERRAIN_CLEARANCE := 6.0
const UNIT_MODEL_GROUND_CLEARANCE := 1.2
const FREE_CAMERA_MIN_PITCH := 12.0
const FREE_CAMERA_MAX_PITCH := 82.0
const FREE_CAMERA_ROTATION_SPEED := 0.004
const CAMERA_YAW_SPEED := 1.45
const SIMULATION_WORLD_CENTER_SPAN := 319.0
const VisualRegistryScript = preload("res://visual_definition_registry.gd")
const VisualRootScript = preload("res://unit_visual_root.gd")
const VisualSpawnBridgeScript = preload("res://visual_spawn_bridge.gd")
const VisualPresentationPolicyScript = preload("res://visual_presentation_policy.gd")
const VisualPackCompatibilityScript = preload("res://visual_pack_compatibility.gd")
const UiTypographyScript = preload("res://ui_typography.gd")
const StrategicUnitIconScript = preload("res://strategic_unit_icon.gd")
const MESH_BASE_PATH := "res://scenarios/meshes/"
const TERRAIN_SAMPLE_WIDTH := 320
const TERRAIN_SAMPLE_HEIGHT := 320
const CIVILIAN_DRESSING_PATH := "res://scenarios/civilian_dressing.json"
const TREE_INSTANCE_TARGET := 520
const SkirmishConfigLoader := preload("res://skirmish_config.gd")
const ArmorTexture := preload("res://assets/units/near_future_armor_tile_v1.png")
const MATERIAL_SITES := [
	{"id": 900, "name": "RARE METALS", "position": Vector2(-15500, -9000), "owner": -1, "color": Color("#ffbd52")},
	{"id": 901, "name": "SILICON", "position": Vector2(-10500, 11000), "owner": -1, "color": Color("#47d9ff")},
	{"id": 902, "name": "POLYMER FEEDSTOCK", "position": Vector2(10500, -11000), "owner": 1, "color": Color("#ce82ff")},
	{"id": 903, "name": "SYNTHETIC OIL", "position": Vector2(15500, 9000), "owner": 1, "color": Color("#7ad99b")},
]

@onready var camera: Camera3D = $Camera3D
@onready var terrain_view: MeshInstance3D = $Ocean
@onready var unit_view: MultiMeshInstance3D = $Units
@onready var unit_models: Node3D = $UnitModels
@onready var resource_sites: Node3D = $ResourceSites
@onready var construction_frames: Node3D = $ConstructionFrames
@onready var terrain_trees: MultiMeshInstance3D = $TerrainTrees
@onready var strategic_trees: MultiMeshInstance3D = $StrategicTrees
@onready var forest_landmarks: Node3D = $ForestLandmarks
@onready var civilian_buildings: Node3D = $CivilianBuildings
@onready var west_landmass: MeshInstance3D = $WestLandmass
@onready var east_landmass: MeshInstance3D = $EastLandmass
@onready var debug_label: Label = $HUD/DebugPanel/DebugLabel
@onready var selection_rect: ColorRect = $HUD/SelectionRect
@onready var debug_panel: ColorRect = $HUD/DebugPanel
@onready var help_label: Label = $HUD/HelpLabel
@onready var command_hud: Control = $HUD/CommandHUD
@onready var strategic_icon_overlay: Control = $HUD/StrategicIconOverlay
@onready var startup_overlay: ColorRect = $HUD/StartupOverlay
@onready var scenario_title: Label = $HUD/StartupOverlay/Panel/VBox/ScenarioTitle
@onready var scenario_description: Label = $HUD/StartupOverlay/Panel/VBox/ScenarioDescription
@onready var scenario_status: Label = $HUD/StartupOverlay/Panel/VBox/ScenarioStatus
@onready var start_button: Button = $HUD/StartupOverlay/Panel/VBox/StartButton
@onready var territory_debug_label: Label = $HUD/TerritoryDebugLabel
@onready var territory_material: ShaderMaterial = $Ocean.material_override
@onready var battlefield_environment: WorldEnvironment = $WorldEnvironment
@onready var battlefield_light: DirectionalLight3D = $DirectionalLight3D
@onready var moon_light: DirectionalLight3D = $MoonLight3D

var extension: Object
var unit_multimesh: MultiMesh
var demo_unit_views: Dictionary = {}
var demo_mode := false
var terrain_heights := PackedFloat32Array()
var terrain_world_width := 320.0
var terrain_world_height := 320.0

var mesh_cache := {}

var UNIT_TYPE_TO_CONTENT_ID := {
	0: "elite_main_battle_tank",
	1: "elite_long_range_artillery",
	2: "elite_anti_air",
	3: "mass_swarm_tank",
	4: "mass_assault_vehicle",
	5: "mass_anti_air",
	6: "industrial_mbt",
	7: "industrial_missile_platform",
	8: "industrial_engineering",
	9: "elite_t1_fighter",
	10: "elite_t1_vtol",
	11: "elite_patrol_boat",
	12: "command_walker"
}

const UNIT_TYPE_DISPLAY_NAME := {
	0: "ELITE MBT",
	1: "LONG-RANGE ARTILLERY",
	2: "ANTI-AIR PLATFORM",
	3: "MASS SWARM TANK",
	4: "MASS ASSAULT VEHICLE",
	5: "MASS ANTI-AIR",
	6: "INDUSTRIAL MBT",
	7: "MISSILE PLATFORM",
	8: "ENGINEERING VEHICLE",
	9: "T1 FIGHTER",
	10: "T1 VTOL",
	11: "PATROL BOAT",
	12: "COMMAND WALKER"
}

func _load_mesh_from_json(content_id: String) -> ArrayMesh:
	if mesh_cache.has(content_id):
		return mesh_cache[content_id]

	var mesh_path := MESH_BASE_PATH + content_id + ".mesh.json"
	var file := FileAccess.open(mesh_path, FileAccess.READ)
	if file == null:
		print("WARNING: Mesh file not found: %s" % mesh_path)
		return null

	var content := file.get_as_text()
	file.close()

	var parser := JSON.new()
	var parse_result := parser.parse(content)
	if parse_result != OK:
		print("WARNING: Failed to parse mesh JSON: %s" % mesh_path)
		return null

	var data: Dictionary = parser.data
	var geometry: Dictionary = data.get("geometry", {})
	var vertices_array: Array = geometry.get("vertices", [])
	var triangles_array: Array = geometry.get("triangles", [])

	if vertices_array.is_empty() or triangles_array.is_empty():
		print("WARNING: Mesh has no geometry: %s" % content_id)
		return null

	var surface_array := Array()
	surface_array.resize(ArrayMesh.ARRAY_MAX)

	var vertices := PackedVector3Array()
	for v in vertices_array:
		vertices.append(Vector3(v[0], v[1], v[2]))
	surface_array[ArrayMesh.ARRAY_VERTEX] = vertices

	var indices := PackedInt32Array()
	for i in range(0, triangles_array.size(), 3):
		if i + 2 < triangles_array.size():
			indices.append(triangles_array[i])
			indices.append(triangles_array[i + 1])
			indices.append(triangles_array[i + 2])
	surface_array[ArrayMesh.ARRAY_INDEX] = indices

	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface_array)

	mesh_cache[content_id] = mesh
	return mesh

var entity_ids := PackedInt32Array()
var entity_to_instance: Dictionary = {}
var entity_base_colors: Dictionary = {}
var entity_unit_types: Dictionary = {}
var selected_ids := PackedInt32Array()
var player_entity_ids := PackedInt32Array()
var ai_entity_ids := PackedInt32Array()
var unit_positions := PackedFloat32Array()
var scenario_definition: Dictionary = {}
var match_started := false
var prototype_visuals_enabled := false
var prototype_visual_limit := 200
var visual_registry
var visual_spawn_bridge
var prototype_visual_views: Dictionary = {}
var unit_visual_headings: Dictionary = {}
var commander_ids: Dictionary = {}
var engineer_ids: Dictionary = {}
var hidden_base_ids: Dictionary = {}
var pending_player_build_types: Array[int] = []
var pending_structure_type := -1
var pending_structure_position := Vector2.ZERO
var completed_structure_views: Array[Node3D] = []
var build_mode_type := -1
var build_ghost: Node3D
var build_ghost_material: StandardMaterial3D
var road_build_mode := false
var road_start := Vector2.INF
var road_preview: MeshInstance3D
var road_preview_material: StandardMaterial3D
var road_views: Dictionary = {}
var blueprint_pointer_down := false
var pending_completed_structures: Array[Dictionary] = []
var selected_structure_view: Node3D
var civilian_building_instance_count := 0
var civilian_building_positions: Array[Vector2] = []
var tree_lod_distance := 5000.0
var pending_build_order: Dictionary = {}
var material_site_state: Dictionary = {}
var material_order_mode := 0 # 0 normal, 1 claim/capture, 2 demolish
var fob_build_mode := false
var fob_build_target := Vector2.INF
var fob_was_constructing := false
var fob_completion_notification := ""
var hover_context: Dictionary = {}

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
var zoom_focus_world := Vector2.ZERO
var zoom_focus_screen := Vector2.ZERO
var zoom_focus_pending := false
const ZOOM_FOCUS_TOLERANCE := 0.05
var free_float_camera := false
var free_camera_yaw := 0.0
var free_camera_pitch := 38.0
var tactical_camera_yaw := 0.0
var environment_debug_brightness := 0.72
var environment_debug_fog := 0.000018
var environment_debug_exposure := 1.08
var environment_debug_sliders: Dictionary = {}
var environment_debug_value_labels: Dictionary = {}
var environment_debug_weather := "OVERCAST"
var environment_debug_time_of_day := "DAYTIME"
var environment_weather_sun_multiplier := 1.0
var environment_weather_terrain_multiplier := 1.0
var environment_weather_terrain_ambient := 0.20
var environment_debug_weather_option: OptionButton
var environment_debug_day_button: Button
var environment_debug_night_button: Button
var environment_debug_steering_label: Label
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
	extension.call("economy_add_storage", 1, 0.0, 0.0, 5000.0, 5000.0, 1000.0)
	extension.call("economy_add_production_line", 1, 1, 100.0, 100.0, 5)


func _configure_visual_pack_handshake() -> void:
	var pack_handshake: Dictionary = VisualPackCompatibilityScript.handshake()
	var pack_id := String(pack_handshake.get("pack_id", ""))
	var pack_version := int(pack_handshake.get("pack_version", -1))
	var pack_hash := String(pack_handshake.get("sha256", ""))
	if pack_id.is_empty() or pack_hash.length() != 64 or pack_version < 0:
		push_error("Visual-pack handshake metadata is invalid; multiplayer compatibility is disabled")
		return
	if extension.has_method("set_expected_visual_pack"):
		extension.call("set_expected_visual_pack", pack_id, pack_version, pack_hash)
	if extension.has_method("send_visual_pack_handshake"):
		# The join transport invokes this when a peer connection is established.
		# Keeping the manifest extraction here prevents simulation code from reading files.
		print("Visual-pack handshake ready: %s v%d" % [pack_id, pack_version])


func _ready() -> void:
	UiTypographyScript.apply_to(self)
	_setup_environment_debug_panel()
	strategic_icon_overlay.match_view = self
	# Territory data is still a development diagnostic; the centered table was
	# competing with the tactical view. Keep the update path available for a
	# future operational-theater panel, but never show it over the battlefield.
	territory_debug_label.visible = false
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
	_configure_visual_pack_handshake()
	var profile_frames := OS.get_environment("RTS_PROFILE_FRAMES")
	var is_scale_profile := profile_frames.is_valid_int() and int(profile_frames) > 0
	# User-provided reference visuals are the normal small-skirmish default
	# until bespoke models replace them. Set RTS_PROTOTYPE_VISUALS=0 to return
	# to the procedural presentation; scale profiles still force MultiMesh.
	var prototype_visuals_requested := OS.get_environment("RTS_PROTOTYPE_VISUALS") != "0"
	prototype_visuals_enabled = VisualPresentationPolicyScript.use_prototype_wrappers(
		prototype_visuals_requested,
		is_scale_profile
	)
	if prototype_visuals_requested and is_scale_profile:
		push_warning("Prototype visual wrappers are disabled for scale profiles; retaining batched MultiMesh presentation")
	if OS.get_environment("RTS_PROTOTYPE_VISUAL_LIMIT").is_valid_int():
		prototype_visual_limit = maxi(1, int(OS.get_environment("RTS_PROTOTYPE_VISUAL_LIMIT")))
	if prototype_visuals_enabled:
		visual_registry = VisualRegistryScript.new()
		if not visual_registry.load_definitions():
			push_warning("Prototype visual registry failed; retaining MultiMesh presentation")
			prototype_visuals_enabled = false
		else:
			visual_spawn_bridge = VisualSpawnBridgeScript.new()

	if is_scale_profile:
		profile_target_frames = int(profile_frames)
		_start_scale_profile()
		return

	start_button.pressed.connect(_on_start_skirmish_pressed)

	# The native map loader deliberately exposes map metadata, not the starting
	# rosters.  The validated scenario definition is therefore the authority for
	# presentation setup; using the map result here silently produced zero units.
	var scenario_result: Dictionary = SkirmishConfigLoader.load_definition(DEFAULT_SKIRMISH_PATH)
	if not scenario_result.get("ok", false):
		scenario_status.text = ("Scenario unavailable: %s" % scenario_result.get("error", "unknown load error")).to_upper()
		start_button.disabled = true
		return

	scenario_definition = scenario_result.definition
	_configure_scenario_theater()
	scenario_title.text = String(scenario_definition.display_name).to_upper()
	scenario_description.text = String(scenario_definition.description).to_upper()
	scenario_status.text = "Elite Precision vs Mass Warfare\n%s" % scenario_definition.victory.description
	if OS.get_environment("RTS_AUTO_START_SKIRMISH") == "1":
		_on_start_skirmish_pressed.call_deferred()


func _setup_environment_debug_panel() -> void:
	var environment := battlefield_environment.environment
	if environment == null:
		return
	environment_debug_brightness = environment.ambient_light_energy
	environment_debug_fog = environment.fog_density
	environment_debug_exposure = environment.tonemap_exposure
	debug_panel.offset_right = 380.0
	debug_panel.offset_bottom = 346.0
	debug_label.visible = false
	var title := Label.new()
	title.text = "ENVIRONMENT DEBUG // F8"
	title.position = Vector2(12, 10)
	title.add_theme_color_override("font_color", Color("#47d9ff"))
	debug_panel.add_child(title)
	_add_environment_slider("brightness", "MAP BRIGHTNESS", 0.25, 1.8, environment_debug_brightness, 42.0)
	_add_environment_slider("fog", "FOG DENSITY", 0.0, 0.00006, environment_debug_fog, 82.0)
	_add_environment_slider("exposure", "EXPOSURE", 0.5, 1.8, environment_debug_exposure, 122.0)
	var weather_label := Label.new()
	weather_label.text = "WEATHER"
	weather_label.position = Vector2(12, 164)
	weather_label.size = Vector2(132, 24)
	debug_panel.add_child(weather_label)
	environment_debug_weather_option = OptionButton.new()
	environment_debug_weather_option.position = Vector2(138, 162)
	environment_debug_weather_option.size = Vector2(190, 28)
	for weather_name in ["CLEAR", "OVERCAST", "STORM"]:
		environment_debug_weather_option.add_item(weather_name)
	environment_debug_weather_option.item_selected.connect(func(index: int): _apply_environment_weather(environment_debug_weather_option.get_item_text(index)))
	debug_panel.add_child(environment_debug_weather_option)
	environment_debug_weather_option.select(1)
	var time_label := Label.new()
	time_label.text = "TIME OF DAY"
	time_label.position = Vector2(12, 204)
	time_label.size = Vector2(132, 24)
	debug_panel.add_child(time_label)
	environment_debug_day_button = Button.new()
	environment_debug_day_button.text = "DAYTIME"
	environment_debug_day_button.position = Vector2(138, 202)
	environment_debug_day_button.size = Vector2(92, 28)
	environment_debug_day_button.pressed.connect(func(): _apply_time_of_day("DAYTIME"))
	debug_panel.add_child(environment_debug_day_button)
	environment_debug_night_button = Button.new()
	environment_debug_night_button.text = "NIGHTTIME"
	environment_debug_night_button.position = Vector2(236, 202)
	environment_debug_night_button.size = Vector2(92, 28)
	environment_debug_night_button.pressed.connect(func(): _apply_time_of_day("NIGHTTIME"))
	debug_panel.add_child(environment_debug_night_button)
	environment_debug_steering_label = Label.new()
	environment_debug_steering_label.position = Vector2(12, 244)
	environment_debug_steering_label.size = Vector2(356, 28)
	environment_debug_steering_label.autowrap_mode = TextServer.AUTOWRAP_OFF
	environment_debug_steering_label.add_theme_color_override("font_color", Color("#b6c7cf"))
	debug_panel.add_child(environment_debug_steering_label)
	var reset := Button.new()
	reset.text = "RESET DEFAULTS"
	reset.position = Vector2(208, 284)
	reset.size = Vector2(120, 28)
	reset.pressed.connect(_reset_environment_debug)
	debug_panel.add_child(reset)
	_apply_environment_weather(environment_debug_weather)
	_apply_time_of_day(environment_debug_time_of_day)


func _add_environment_slider(key: String, title_text: String, minimum: float, maximum: float, initial: float, y: float) -> void:
	var label := Label.new()
	label.text = title_text
	label.position = Vector2(12, y)
	label.size = Vector2(132, 24)
	debug_panel.add_child(label)
	var value_label := Label.new()
	value_label.position = Vector2(335, y)
	value_label.size = Vector2(34, 24)
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	debug_panel.add_child(value_label)
	var slider := HSlider.new()
	slider.position = Vector2(138, y + 2)
	slider.size = Vector2(190, 22)
	slider.min_value = minimum
	slider.max_value = maximum
	slider.step = 0.000001 if key == "fog" else 0.01
	slider.value = initial
	slider.value_changed.connect(func(value: float): _set_environment_debug_value(key, value))
	debug_panel.add_child(slider)
	environment_debug_sliders[key] = slider
	environment_debug_value_labels[key] = value_label
	_set_environment_debug_value(key, initial)


func _set_environment_debug_value(key: String, value: float) -> void:
	match key:
		"brightness":
			environment_debug_brightness = value
			_apply_time_of_day(environment_debug_time_of_day)
			territory_material.set_shader_parameter("terrain_brightness", value / 0.72)
			(environment_debug_value_labels.get(key) as Label).text = "%.2f" % value
		"fog":
			environment_debug_fog = value
			battlefield_environment.environment.fog_density = value
			(environment_debug_value_labels.get(key) as Label).text = "%.5f" % value
		"exposure":
			environment_debug_exposure = value
			battlefield_environment.environment.tonemap_exposure = value
			(environment_debug_value_labels.get(key) as Label).text = "%.2f" % value


func _apply_environment_weather(weather_name: String) -> void:
	environment_debug_weather = weather_name.to_upper()
	var environment := battlefield_environment.environment
	var sky_material := environment.sky.sky_material as ProceduralSkyMaterial
	match environment_debug_weather:
		"CLEAR":
			environment.fog_density = 0.000008
			environment.fog_light_color = Color("#9fb7c5")
			environment.fog_light_energy = 0.82
			sky_material.sky_top_color = Color("#18385d")
			sky_material.sky_horizon_color = Color("#91a9b7")
			battlefield_light.light_color = Color("#fff1cc")
			environment_weather_sun_multiplier = 1.18
			environment_weather_terrain_multiplier = 1.22
			environment_weather_terrain_ambient = 0.28
		"STORM":
			environment.fog_density = 0.00004
			environment.fog_light_color = Color("#3e4b57")
			environment.fog_light_energy = 0.42
			sky_material.sky_top_color = Color("#07101d")
			sky_material.sky_horizon_color = Color("#303a43")
			battlefield_light.light_color = Color("#aeb9c2")
			environment_weather_sun_multiplier = 0.58
			environment_weather_terrain_multiplier = 0.66
			environment_weather_terrain_ambient = 0.12
		_:
			environment.fog_density = 0.000018
			environment.fog_light_color = Color("#859499")
			environment.fog_light_energy = 0.65
			sky_material.sky_top_color = Color("#132133")
			sky_material.sky_horizon_color = Color("#6e808c")
			battlefield_light.light_color = Color("#f0e3c7")
			environment_weather_sun_multiplier = 1.0
			environment_weather_terrain_multiplier = 1.0
			environment_weather_terrain_ambient = 0.20
	environment_debug_fog = environment.fog_density
	if environment_debug_sliders.has("fog"):
		(environment_debug_sliders["fog"] as HSlider).set_value_no_signal(environment.fog_density)
		(environment_debug_value_labels["fog"] as Label).text = "%.5f" % environment.fog_density
	_apply_time_of_day(environment_debug_time_of_day)


func _apply_time_of_day(time_name: String) -> void:
	environment_debug_time_of_day = time_name.to_upper()
	var is_night := environment_debug_time_of_day == "NIGHTTIME"
	battlefield_light.visible = not is_night
	moon_light.visible = is_night
	if is_night:
		battlefield_environment.environment.ambient_light_energy = environment_debug_brightness * 0.30
		moon_light.light_energy = 0.42 * environment_debug_brightness / 0.72
	else:
		battlefield_environment.environment.ambient_light_energy = environment_debug_brightness
		battlefield_light.light_energy = 1.85 * environment_debug_brightness / 0.72 * environment_weather_sun_multiplier
	territory_material.set_shader_parameter(
		"terrain_daylight",
		environment_weather_terrain_multiplier * (0.28 if is_night else 1.0)
	)
	territory_material.set_shader_parameter(
		"terrain_ambient_fill",
		environment_weather_terrain_ambient * (0.14 if is_night else 1.0)
	)
	if environment_debug_day_button != null:
		environment_debug_day_button.modulate = Color("#47d9ff") if not is_night else Color("#73808a")
		environment_debug_night_button.modulate = Color("#c49cff") if is_night else Color("#73808a")


func _reset_environment_debug() -> void:
	for key in ["brightness", "fog", "exposure"]:
		var slider := environment_debug_sliders.get(key) as HSlider
		if slider != null:
			slider.value = 0.72 if key == "brightness" else 0.000018 if key == "fog" else 1.08
	if environment_debug_weather_option != null:
		environment_debug_weather_option.select(1)
	_apply_environment_weather("OVERCAST")
	_apply_time_of_day("DAYTIME")


func _update_steering_debug() -> void:
	if environment_debug_steering_label == null:
		return
	if extension == null or selected_ids.size() != 1:
		environment_debug_steering_label.text = "STEERING // SELECT ONE GROUND UNIT"
		return
	var state: PackedFloat32Array = extension.call("get_unit_steering_state", selected_ids[0])
	if state.size() != 3:
		environment_debug_steering_label.text = "STEERING // NOT A GROUND VEHICLE"
		return
	var heading_degrees := rad_to_deg(state[0])
	var desired_degrees := rad_to_deg(state[1])
	environment_debug_steering_label.text = "STEERING // HDG %03.0f  TARGET %03.0f  SPEED %+.1f" % [heading_degrees, desired_degrees, state[2]]


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
	var terrain: Dictionary = scenario_definition.theater.terrain
	terrain_world_width = float(scenario_definition.theater.get("width", TERRAIN_SAMPLE_WIDTH))
	terrain_world_height = float(scenario_definition.theater.get("height", TERRAIN_SAMPLE_HEIGHT))
	# The far plane must cover the opposite edge of the theater while the
	# camera is zoomed out and panned toward a map boundary.
	camera.far = maxf(CAMERA_MIN_FAR_DISTANCE, maxf(terrain_world_width, terrain_world_height) * 3.0)

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

	if terrain.has("data") and terrain.data != "":
		var heights := HeightMap.load_from_file(terrain.data)
		if !heights.is_empty():
			terrain_heights = heights
			print("Loaded heightmap with %d values" % heights.size())
			# A heightmap covers the complete theater.  Assigning it to each
			# landmass duplicated the world twice at their local offsets, obscuring
			# the intended terrain and units.  The Ocean node is the single map-wide
			# terrain host; the old landmass planes are only a no-heightmap fallback.
			terrain_view.mesh = HeightMap.generate_terrain_mesh(heights, terrain_world_width, terrain_world_height)
			terrain_view.visible = true
			for landmass in landmass_nodes:
				landmass.visible = false
			_rebuild_terrain_trees()


func _start_scale_profile() -> void:
	extension.call("start_simulation")
	match_started = true
	var unit_count := _get_visible_unit_count()
	_create_unit_multimesh(unit_count)
	_spawn_units(unit_count)
	startup_overlay.visible = false
	debug_panel.visible = false
	help_label.visible = false
	command_hud.visible = true
	_update_camera(1.0)
	_update_hud()
	print("RtsExtension loaded; spawned %d batched profile units" % entity_ids.size())


func _on_start_skirmish_pressed() -> void:
	if match_started or scenario_definition.is_empty():
		return

	extension.call("configure_world_size", terrain_world_width, terrain_world_height)
	var west_theater_landmass: Dictionary = scenario_definition.theater.landmasses[0]
	var east_theater_landmass: Dictionary = scenario_definition.theater.landmasses[1]
	extension.call("configure_theater_landmasses",
		float(west_theater_landmass.center[0]), float(west_theater_landmass.center[1]), float(west_theater_landmass.size[0]), float(west_theater_landmass.size[1]),
		float(east_theater_landmass.center[0]), float(east_theater_landmass.center[1]), float(east_theater_landmass.size[0]), float(east_theater_landmass.size[1]))
	var theater_terrain: Dictionary = scenario_definition.theater.get("terrain", {})
	if theater_terrain.get("data", "") != "":
		extension.call("load_terrain_heightmap", theater_terrain.get("data", ""))
	extension.call("start_simulation")
	match_started = true
	_setup_economy_for_player_1()
	_create_unit_multimesh(2)
	demo_mode = true
	entity_ids.clear()
	entity_to_instance.clear()
	entity_base_colors.clear()
	entity_unit_types.clear()
	selected_ids.clear()
	player_entity_ids.clear()
	ai_entity_ids.clear()
	unit_positions.clear()
	commander_ids.clear()
	engineer_ids.clear()
	hidden_base_ids.clear()
	pending_player_build_types.clear()
	material_site_state.clear()
	material_order_mode = 0
	if not _spawn_commander_start():
		push_error("Failed to create the authoritative engineer-start forces")
		extension.call("stop_simulation")
		match_started = false
		return
	_spawn_material_sites()
	_spawn_civilian_buildings()
	_set_selected(_player_engineer_id(), true)

	startup_overlay.visible = false
	debug_panel.visible = false
	help_label.visible = false
	command_hud.visible = true
	var player_spawn: Array = scenario_definition.player.get("spawn", [0.0, 0.0])
	var player_x := float(player_spawn[0])
	var player_z := float(player_spawn[1])
	camera_target = Vector3(player_x, _terrain_height_at(player_x, player_z), player_z)
	# The 40 km theater has kilometer-scale relief. Start high enough to clear
	# the nearby ridges while preserving a useful tactical angle.
	camera_distance = 1500.0
	target_camera_distance = 1500.0
	_update_camera(1.0)
	_update_hud()
	print("Started %s with %d player Field Engineer and %d AI Field Engineer" % [
		scenario_definition.display_name,
		player_entity_ids.size(),
		ai_entity_ids.size(),
	])


func _exit_tree() -> void:
	if extension != null and match_started:
		extension.call("stop_simulation")


func _process(delta: float) -> void:
	# Middle-button release can be missed if focus changes while the pointer is
	# outside the window. Never leave tactical panning latched in that case.
	if panning and not Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
		panning = false
	if road_build_mode:
		var road_target := _screen_to_world(get_viewport().get_mouse_position())
		_update_road_preview(road_target)
	if build_mode_type >= 0:
		var ghost_target := _screen_to_world(get_viewport().get_mouse_position())
		if build_ghost == null:
			build_ghost = _make_build_ghost(build_mode_type)
			add_child(build_ghost)
		var ghost_valid := true
		if build_mode_type >= 100 and extension != null:
			ghost_valid = bool(extension.call("validate_structure_placement", build_mode_type - 100, ghost_target.x, ghost_target.y))
		_set_build_ghost_valid(ghost_valid)
		build_ghost.position = Vector3(ghost_target.x, _terrain_height_at(ghost_target.x, ghost_target.y) + 0.8, ghost_target.y)
	if extension == null or not match_started:
		return

	var simulation_start_us := Time.get_ticks_usec()
	extension.call("update_simulation", delta * 1000.0)
	_process_pending_build_order()
	var simulation_call_ms := float(Time.get_ticks_usec() - simulation_start_us) / 1000.0
	_sync_new_entities()
	_sync_completed_roads()
	_update_keyboard_pan(delta)
	_update_camera(delta)
	_sync_tree_lod()
	var sync_timings := _sync_unit_transforms()
	_record_profile_frame(delta, simulation_call_ms, sync_timings.x, sync_timings.y)
	_update_hover_context()

	hud_elapsed += delta
	if hud_elapsed >= 0.2:
		hud_elapsed = 0.0
		_update_hud()

	_update_territory()


func _update_territory() -> void:
	var zone_count: int = int(extension.territory_get_zone_count())
	if zone_count <= 0:
		return
	
	var territory_texture := ImageTexture.new()
	var image := Image.new()
	var width := 256
	var height := 256
	image.create(width, height, false, Image.FORMAT_RGBA8)
	
	for i in range(zone_count):
		var zone_info: Array = extension.territory_get_zone_info(i)
		if zone_info.size() >= 5:
			var x: float = float(zone_info[0])
			var y: float = float(zone_info[1])
			var state: int = int(zone_info[2])
			var type: int = int(zone_info[3])
			
			var color := Color(0.5, 0.5, 0.5, 0.3)
			if state == 0:
				color = Color(0.2, 0.2, 0.2, 0.3)
			elif state == 1:
				color = Color(1.0, 0.5, 0.0, 0.4)
			elif state == 2:
				color = Color(0.0, 0.5, 1.0, 0.3)
			elif state == 3:
				color = Color(0.0, 0.8, 0.0, 0.4)
			
			var screen_x := int(clampf((x + 160.0) / 320.0 * width, 0, width - 1))
			var screen_y := int(clampf((y + 160.0) / 320.0 * height, 0, height - 1))
			image.setPixel(screen_x, screen_y, color)
	
	territory_texture = ImageTexture.create_from_image(image)
	territory_material.setShaderParameter("territory_texture", territory_texture)
	
	var state_names: Array[String] = ["NEUTRAL", "RECON", "CLIMBING", "CLAIMED", "SECURED", "CONSOLIDATED", "ESTABLISHED"]
	var type_names: Array[String] = ["RECONZONE", "HELIPADZONE", "FOBZONE", "BASEZONE", "NAVALZONE", "AIRBASEZONE", "COMMANDZONE"]
	var debug_text := "Territory Zones: %d\n" % zone_count
	debug_text += "%-15s %-15s %-12s %-12s\n" % ["X", "Y", "State", "Type"]
	for i in range(min(zone_count, 5)):
		var zone_info: Array = extension.territory_get_zone_info(i)
		if zone_info.size() >= 5:
			var x: float = float(zone_info[0])
			var y: float = float(zone_info[1])
			var state: int = int(zone_info[2])
			var type: int = int(zone_info[3])
			var state_name: String = state_names[state] if state >= 0 and state < state_names.size() else "UNKNOWN"
			var type_name: String = type_names[type] if type >= 0 and type < type_names.size() else "UNKNOWN"
			debug_text += "%-15.1f %-15.1f %-12s %-12s\n" % [x, y, state_name, type_name]
	territory_debug_label.text = debug_text


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and event.keycode == KEY_F8:
		debug_panel.visible = not debug_panel.visible
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		if build_mode_type >= 0 or fob_build_mode or material_order_mode != 0:
			_cancel_tactical_modes()
			return
		get_tree().quit()
		return
	if event is InputEventKey and event.keycode == KEY_SPACE:
		if not event.echo:
			free_float_camera = event.pressed
			if free_float_camera:
				free_camera_yaw = 0.0
				free_camera_pitch = 38.0
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_X:
		_issue_stop_order()
		return
	if event is InputEventKey and event.pressed:
		if event.keycode == KEY_8:
			fob_build_mode = selected_ids.size() > 0
			material_order_mode = 0
			_update_hud()
			return
		var unit_shortcut_index := _build_unit_shortcut_index(event.keycode)
		if unit_shortcut_index >= 0:
			var unit_type := _build_unit_type_at_index(unit_shortcut_index)
			if unit_type >= 0:
				_begin_build_placement(unit_type)
			return
		if event.keycode == KEY_6:
			_begin_build_placement(100)
			return
		if event.keycode == KEY_7:
			_begin_build_placement(101)
			return
		if event.keycode == KEY_L:
			_begin_build_placement(103)
			return
		if event.keycode == KEY_R:
			_begin_road_placement()
			return
	if event is InputEventKey and event.pressed and event.keycode == KEY_2:
		_set_material_order_mode(1)
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_3:
		_set_material_order_mode(2)
		return

	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			_zoom_toward_cursor(event.position, 0.86)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			_zoom_toward_cursor(event.position, 1.16)
		elif event.button_index == MOUSE_BUTTON_MIDDLE:
			panning = event.pressed
			if event.pressed:
				zoom_focus_pending = false
		elif event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				if fob_build_mode:
					return
				if road_build_mode:
					return
				var selected_blueprint := _blueprint_at_screen(event.position)
				if selected_blueprint >= 0:
					_begin_build_placement(selected_blueprint)
					blueprint_pointer_down = true
					return
				if build_mode_type >= 0:
					return
				selecting = true
				selection_start = event.position
				selection_end = event.position
				_update_selection_rect()
			else:
				if fob_build_mode:
					_order_fob(event.position)
					return
				if road_build_mode:
					_handle_road_click(_screen_to_world(event.position))
					return
				if blueprint_pointer_down:
					blueprint_pointer_down = false
					return
				if build_mode_type >= 0:
					var target := _screen_to_world(event.position)
					_order_commander_to_build(build_mode_type, target)
					build_mode_type = -1
					if build_mode_type < 0 and build_ghost != null: build_ghost.queue_free(); build_ghost = null
					selecting = false
					selection_rect.visible = false
				else:
					_finish_selection(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			if build_mode_type >= 0 or fob_build_mode:
				_cancel_tactical_modes()
				return
			if material_order_mode != 0:
				_issue_material_site_order(event.position)
				return
			# RTS primary command: right-click sends selected units to the terrain
			# position.  Attack is deliberately an explicit modifier so an empty
			# patch of ground can never turn a move order into a rejected attack.
			if event.ctrl_pressed:
				_issue_attack_order(event.position)
			else:
				_issue_move_order(event.position)

	if event is InputEventMouseMotion:
		if selecting:
			selection_end = event.position
			_update_selection_rect()
		elif panning:
			if free_float_camera:
				free_camera_yaw -= event.relative.x * FREE_CAMERA_ROTATION_SPEED
				free_camera_pitch = clampf(
					free_camera_pitch - event.relative.y * FREE_CAMERA_ROTATION_SPEED * 90.0,
					FREE_CAMERA_MIN_PITCH,
					FREE_CAMERA_MAX_PITCH)
			else:
				var pan_scale := camera_distance * 0.0025
				camera_target.x -= event.relative.x * pan_scale
				camera_target.z -= event.relative.y * pan_scale


func _create_unit_multimesh(instance_count: int) -> void:
	for wrapper in prototype_visual_views.values():
		if is_instance_valid(wrapper):
			wrapper.queue_free()
	entity_ids.clear()
	entity_to_instance.clear()
	entity_base_colors.clear()
	selected_ids.clear()
	player_entity_ids.clear()
	ai_entity_ids.clear()
	unit_positions.clear()
	prototype_visual_views.clear()
	unit_visual_headings.clear()

	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.roughness = 0.68

	var content_id: String = UNIT_TYPE_TO_CONTENT_ID.get(0, "elite_main_battle_tank")
	var mesh: ArrayMesh = _load_mesh_from_json(content_id)
	if mesh == null:
		print("WARNING: Using fallback BoxMesh for content_id: %s" % content_id)
		var fallback_mesh: Mesh = BoxMesh.new()
		fallback_mesh.size = Vector3(1.5, 0.8, 2.0)
		fallback_mesh.material = material
		mesh = fallback_mesh
	else:
		var surface_count := mesh.get_surface_count()
		for i in range(surface_count):
			mesh.surface_set_material(i, material)

	unit_multimesh = MultiMesh.new()
	unit_multimesh.transform_format = MultiMesh.TRANSFORM_3D
	unit_multimesh.use_colors = true
	unit_multimesh.mesh = mesh
	unit_multimesh.instance_count = instance_count
	unit_multimesh.visible_instance_count = instance_count
	unit_view.multimesh = unit_multimesh
	unit_view.visible = not prototype_visuals_enabled


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
		entity_unit_types[entity_id] = 0
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


func _spawn_commander_start() -> bool:
	for entry in [[scenario_definition.player, PLAYER_FACTION_COLOR], [scenario_definition.ai, AI_FACTION_COLOR]]:
		var side: Dictionary = entry[0]
		var faction_id := int(side.faction_id)
		var spawn: Array = side.spawn
		var entity_id: int = extension.call("create_faction_base", faction_id, float(spawn[0]), float(spawn[1]))
		if entity_id <= 0:
			return false
		commander_ids[faction_id] = entity_id
		hidden_base_ids[entity_id] = true
		# The commander's authored spawn can be strategically valid while its
		# heightfield cell is too steep or cramped for a construction unit. Find
		# the nearest deterministic build-capable site instead of leaving the
		# engineer stranded at a location where structures and roads are invalid.
		var engineer_spawn := _find_engineer_spawn(Vector2(float(spawn[0]), float(spawn[1])))
		if engineer_spawn == Vector2.INF:
			return false
		var engineer_id: int = extension.call("create_unit_with_type", engineer_spawn.x, engineer_spawn.y, 8, faction_id)
		if engineer_id <= 0:
			return false
		engineer_ids[faction_id] = engineer_id
		_register_presented_unit(engineer_id, 8, faction_id, engineer_spawn)
	return true


func _find_engineer_spawn(origin: Vector2) -> Vector2:
	var offsets: Array[Vector2] = [Vector2(4.0, 0.0)]
	for radius in range(1, 9):
		var distance := float(radius) * 500.0
		for direction in [Vector2(-1.0, -1.0), Vector2(-1.0, 0.0), Vector2(-1.0, 1.0), Vector2(0.0, -1.0), Vector2(0.0, 1.0), Vector2(1.0, -1.0), Vector2(1.0, 0.0), Vector2(1.0, 1.0)]:
			offsets.append(direction * distance)
	for offset in offsets:
		var candidate := origin + offset
		if bool(extension.call("validate_engineer_placement", candidate.x, candidate.y)):
			return candidate
	return Vector2.INF


func _player_engineer_id() -> int:
	return int(engineer_ids.get(HUMAN_PLAYER_ID, -1))


func _spawn_material_sites() -> void:
	_clear_material_sites()
	for definition in MATERIAL_SITES:
		var site: Dictionary = definition.duplicate(true)
		material_site_state[int(site.id)] = site
		var position: Vector2 = site.position
		# ResourceNode METAL is the game's single shared Materials economy; the
		# labels describe the near-future field source, not separate inventories.
		extension.call("economy_add_resource_node", int(site.id), position.x, position.y, 50000.0, 0)
		if int(site.owner) == 1:
			var enemy_commander := int(commander_ids.get(1, -1))
			if enemy_commander > 0:
				extension.call("issue_harvest_commands", PackedInt32Array([enemy_commander]), 1, position.x, position.y)
	# Apply initial enemy ownership through the same fixed-tick command path.
	extension.call("update_simulation", 50.0)
	_rebuild_material_site_views()


func _clear_material_sites() -> void:
	for child in resource_sites.get_children():
		child.queue_free()


func _rebuild_material_site_views() -> void:
	_clear_material_sites()
	for site in material_site_state.values():
		if bool(site.get("destroyed", false)):
			continue
		var root := Node3D.new()
		root.name = "MaterialFacility_%d" % int(site.id)
		var position: Vector2 = site.position
		root.position = Vector3(position.x, _terrain_height_at(position.x, position.y), position.y)
		resource_sites.add_child(root)
		var owner := int(site.get("owner", -1))
		var signal_color: Color = PLAYER_FACTION_COLOR if owner == HUMAN_PLAYER_ID else AI_FACTION_COLOR if owner == 1 else Color("#ffbd52")
		var hull := _demo_material(Color("#2b3537"))
		var signal_material := _demo_material(signal_color, 1.2)
		_add_box_model(root, Vector3(4.4, 0.55, 3.4), Vector3(0, 0.28, 0), hull)
		_add_cylinder_model(root, 0.95, 1.35, Vector3(0, 0.95, 0), hull)
		_add_box_model(root, Vector3(0.22, 2.2, 0.22), Vector3(-1.55, 1.25, -0.85), signal_material)
		_add_box_model(root, Vector3(0.22, 2.2, 0.22), Vector3(1.55, 1.25, -0.85), signal_material)
		_add_cylinder_model(root, 0.25, 0.16, Vector3(0, 1.72, 0), signal_material)


func _spawn_civilian_buildings() -> void:
	for child in civilian_buildings.get_children():
		child.queue_free()
	civilian_building_instance_count = 0
	civilian_building_positions.clear()
	var file := FileAccess.open(CIVILIAN_DRESSING_PATH, FileAccess.READ)
	if file == null:
		push_warning("Civilian dressing data is unavailable")
		return
	var dressing: Variant = JSON.parse_string(file.get_as_text())
	if not dressing is Dictionary:
		push_warning("Civilian dressing data is invalid")
		return
	var type_definitions: Array = dressing.get("building_types", [])
	var transforms: Array[Array] = []
	var colors: Array[Color] = []
	var sizes: Array[Vector3] = []
	for definition in type_definitions:
		var dimensions: Array = definition.get("size", [60, 30, 50])
		transforms.append([])
		colors.append(Color(String(definition.get("color", "#68736f"))))
		sizes.append(Vector3(float(dimensions[0]), float(dimensions[1]), float(dimensions[2])))
	if transforms.is_empty():
		return
	var spawn_origins: Array[Vector2] = []
	for faction in [scenario_definition.get("player", {}), scenario_definition.get("ai", {})]:
		var spawn: Array = faction.get("spawn", [0.0, 0.0])
		spawn_origins.append(Vector2(float(spawn[0]), float(spawn[1])))
	for site in MATERIAL_SITES:
		spawn_origins.append(site.position)
	var clusters: Array = dressing.get("clusters", [])
	for cluster in clusters:
		var rng := RandomNumberGenerator.new()
		rng.seed = int(cluster.get("seed", 1))
		var center := Vector2(float(cluster.center[0]), float(cluster.center[1]))
		var spread := Vector2(float(cluster.spread[0]), float(cluster.spread[1]))
		var attempts := int(cluster.get("count", 0)) * 8
		var placed := 0
		for _attempt in range(attempts):
			if placed >= int(cluster.get("count", 0)):
				break
			var candidate := center + Vector2(rng.randf_range(-spread.x, spread.x), rng.randf_range(-spread.y, spread.y))
			if not bool(extension.call("is_land_position", candidate.x, candidate.y)):
				continue
			# Civilian dressing is authored scenery rather than a military build
			# order. Keep the cluster on land, but allow a broader terrain range
			# than an outpost so the world does not look unnaturally empty.
			var local_height := _terrain_height_at(candidate.x, candidate.y)
			var local_height_min := local_height
			var local_height_max := local_height
			for sample_offset in [Vector2(-100.0, 0.0), Vector2(100.0, 0.0), Vector2(0.0, -100.0), Vector2(0.0, 100.0)]:
				var sample_height := _terrain_height_at(candidate.x + sample_offset.x, candidate.y + sample_offset.y)
				local_height_min = minf(local_height_min, sample_height)
				local_height_max = maxf(local_height_max, sample_height)
			if local_height_max - local_height_min > 180.0:
				continue
			var too_close := false
			for protected in spawn_origins:
				if candidate.distance_to(protected) < 650.0:
					too_close = true
					break
			if too_close:
				continue
			var type_index := (placed + int(cluster.get("seed", 0))) % transforms.size()
			var building_size: Vector3 = sizes[type_index]
			var scale := rng.randf_range(0.82, 1.16)
			var height := _terrain_height_at(candidate.x, candidate.y)
			var transform := Transform3D(
				Basis(Vector3.UP, rng.randf_range(0.0, TAU)).scaled(Vector3(scale, scale, scale)),
				Vector3(candidate.x, height + building_size.y * scale * 0.5, candidate.y))
			(transforms[type_index] as Array).append(transform)
			civilian_building_positions.append(candidate)
			extension.call("block_civilian_area", candidate.x, candidate.y, maxf(building_size.x, building_size.z) * 0.32)
			placed += 1
			civilian_building_instance_count += 1
	for type_index in range(transforms.size()):
		var instances: Array = transforms[type_index]
		if instances.is_empty():
			continue
		var mesh := BoxMesh.new()
		mesh.size = sizes[type_index]
		var material := _demo_material(colors[type_index])
		material.roughness = 0.88
		mesh.material = material
		var multimesh := MultiMesh.new()
		multimesh.transform_format = MultiMesh.TRANSFORM_3D
		multimesh.mesh = mesh
		multimesh.instance_count = instances.size()
		for instance_index in range(instances.size()):
			multimesh.set_instance_transform(instance_index, instances[instance_index])
		var view := MultiMeshInstance3D.new()
		view.name = "Civilian_%s" % String(type_definitions[type_index].get("id", type_index))
		view.multimesh = multimesh
		view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
		civilian_buildings.add_child(view)


func _set_material_order_mode(mode: int) -> void:
	var commander_id := _player_engineer_id()
	if commander_id <= 0:
		return
	# Resource actions are commander-level orders. Arm the mode even when the
	# player forgot to select the opening unit first.
	if not selected_ids.has(commander_id):
		_clear_selection()
		_set_selected(commander_id, true)
	material_order_mode = mode
	_update_hud()


func _issue_material_site_order(screen_position: Vector2) -> void:
	var ray_origin := camera.project_ray_origin(screen_position)
	var ray_direction := camera.project_ray_normal(screen_position)
	var intersection = Plane(Vector3.UP, 0.0).intersects_ray(ray_origin, ray_direction)
	if intersection == null:
		return
	var target: Vector3 = intersection
	var selected_site: Dictionary = {}
	var nearest_distance := 8.0
	for site in material_site_state.values():
		if bool(site.get("destroyed", false)):
			continue
		var position: Vector2 = site.position
		var distance := Vector2(target.x, target.z).distance_to(position)
		if distance <= nearest_distance:
			nearest_distance = distance
			selected_site = site
	if selected_site.is_empty():
		return
	var commander_id := _player_engineer_id()
	var site_position: Vector2 = selected_site.position
	if material_order_mode == 1:
		var accepted: int = extension.call("issue_harvest_commands", PackedInt32Array([commander_id]), HUMAN_PLAYER_ID, site_position.x, site_position.y)
		if accepted == 1:
			selected_site.owner = HUMAN_PLAYER_ID
			material_site_state[int(selected_site.id)] = selected_site
	elif material_order_mode == 2:
		if extension.call("destroy_resource_site", commander_id, site_position.x, site_position.y):
			selected_site.destroyed = true
			material_site_state[int(selected_site.id)] = selected_site
	material_order_mode = 0
	_rebuild_material_site_views()
	_update_hud()


func _register_presented_unit(entity_id: int, unit_type: int, faction_id: int, world: Vector2) -> void:
	if entity_to_instance.has(entity_id):
		return
	var player_controlled := faction_id == HUMAN_PLAYER_ID
	var color := PLAYER_FACTION_COLOR if player_controlled else AI_FACTION_COLOR
	var instance_index := entity_ids.size()
	entity_ids.append(entity_id)
	entity_to_instance[entity_id] = instance_index
	entity_base_colors[entity_id] = color
	entity_unit_types[entity_id] = unit_type
	unit_positions.append(world.x)
	unit_positions.append(world.y)
	unit_multimesh.instance_count = entity_ids.size()
	unit_multimesh.visible_instance_count = entity_ids.size()
	if prototype_visuals_enabled and unit_type != 12:
		var visual_id := String(extension.call("get_unit_visual_id", unit_type))
		var wrapper = visual_spawn_bridge.spawn(self, visual_registry, visual_id, Transform3D(Basis.IDENTITY, Vector3(world.x, _terrain_height_at(world.x, world.y) + UNIT_MODEL_GROUND_CLEARANCE, world.y)), color)
		wrapper.set_strategic_icon(unit_type, color)
		wrapper.set_range_terrain_height_sampler(func(world_x: float, world_z: float): return _terrain_height_at(world_x, world_z))
		prototype_visual_views[entity_id] = wrapper
	else:
		_create_demo_unit(entity_id, unit_type, color, Vector3(world.x, _terrain_height_at(world.x, world.y), world.y))
	if player_controlled:
		player_entity_ids.append(entity_id)
	else:
		ai_entity_ids.append(entity_id)


func _sync_new_entities() -> void:
	var all_ids: PackedInt32Array = extension.call("get_entity_ids")
	for entity_id in all_ids:
		if hidden_base_ids.has(entity_id):
			continue
		if entity_to_instance.has(entity_id):
			continue
		var faction_id := int(extension.call("get_unit_faction_id", entity_id))
		if faction_id < 0 or faction_id > 2:
			continue
		var position := Vector2(float(extension.call("get_unit_x", entity_id)), float(extension.call("get_unit_y", entity_id)))
		var unit_type := 0
		if faction_id == HUMAN_PLAYER_ID and not pending_player_build_types.is_empty():
			unit_type = pending_player_build_types.pop_front()
		_register_presented_unit(entity_id, unit_type, faction_id, position)


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
			entity_unit_types[entity_id] = unit_type
			unit_positions.append(world_x)
			unit_positions.append(world_y)
			unit_multimesh.set_instance_transform(instance_index, Transform3D(Basis.IDENTITY, Vector3(world_x, 0.4, world_y)))
			unit_multimesh.set_instance_color(instance_index, color)
			if prototype_visuals_enabled:
				var visual_id := String(extension.call("get_unit_visual_id", unit_type))
				var wrapper = visual_spawn_bridge.spawn(self, visual_registry, visual_id, Transform3D(Basis.IDENTITY, Vector3(world_x, 0.4, world_y)), color)
				wrapper.set_strategic_icon(unit_type, color)
				wrapper.set_range_terrain_height_sampler(func(world_x: float, world_z: float): return _terrain_height_at(world_x, world_z))
				prototype_visual_views[entity_id] = wrapper
			if player_controlled:
				player_entity_ids.append(entity_id)
			else:
				ai_entity_ids.append(entity_id)
			side_index += 1
	return true


func _sync_unit_transforms() -> Vector2:
	var fetch_start_us := Time.get_ticks_usec()
	var latest_positions: PackedFloat32Array = extension.call("get_unit_positions", entity_ids)
	var latest_headings: PackedFloat32Array = extension.call("get_unit_headings", entity_ids)
	var fetch_ms := float(Time.get_ticks_usec() - fetch_start_us) / 1000.0
	if latest_positions.size() != entity_ids.size() * 2:
		push_error("Native position snapshot size did not match entity IDs")
		return Vector2(fetch_ms, 0.0)
	var previous_positions := unit_positions
	unit_positions = latest_positions
	var upload_start_us := Time.get_ticks_usec()
	for index in range(entity_ids.size()):
		var x := unit_positions[index * 2]
		var z := unit_positions[index * 2 + 1]
		var heading := latest_headings[index] + PI if index < latest_headings.size() else float(unit_visual_headings.get(entity_ids[index], PI))
		if previous_positions.size() == unit_positions.size():
			var delta := Vector2(x - previous_positions[index * 2], z - previous_positions[index * 2 + 1])
			if delta.length_squared() > 0.000001:
				# Godot's forward axis is -Z, so add PI to align the visible hull
				# with the authoritative movement vector in the X/Z battlefield.
				if index >= latest_headings.size():
					heading = atan2(delta.x, delta.y) + PI
		unit_visual_headings[entity_ids[index]] = heading
		var basis := Basis(Vector3.UP, heading)
		if prototype_visuals_enabled and prototype_visual_views.has(entity_ids[index]):
			var wrapper = prototype_visual_views[entity_ids[index]]
			wrapper.apply_simulation_transform(Transform3D(basis, Vector3(x, _terrain_height_at(x, z) + UNIT_MODEL_GROUND_CLEARANCE, z)))
			wrapper.update_lod(camera_distance)
		else:
			unit_multimesh.set_instance_transform(index, Transform3D(basis, Vector3(x, 0.4, z)))
			var demo_root: Node3D = demo_unit_views.get(entity_ids[index], null)
			if demo_root != null:
				_set_unit_lod(demo_root)
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
	var yaw_direction := 0.0
	if Input.is_key_pressed(KEY_Q):
		yaw_direction -= 1.0
	if Input.is_key_pressed(KEY_E):
		yaw_direction += 1.0
	if yaw_direction != 0.0:
		_rotate_camera_facing(yaw_direction, delta)

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
		zoom_focus_pending = false
		direction = direction.normalized()
		var speed := maxf(18.0, camera_distance * (0.9 if free_float_camera else 0.7))
		if free_float_camera:
			var forward := Vector2(sin(free_camera_yaw), cos(free_camera_yaw))
			var right := Vector2(cos(free_camera_yaw), -sin(free_camera_yaw))
			var free_direction := right * direction.x + forward * direction.y
			camera_target += Vector3(free_direction.x, 0.0, free_direction.y) * speed * delta
		else:
			var forward := Vector2(sin(tactical_camera_yaw), cos(tactical_camera_yaw))
			var right := Vector2(cos(tactical_camera_yaw), -sin(tactical_camera_yaw))
			var yawed_direction := right * direction.x + forward * direction.y
			camera_target += Vector3(yawed_direction.x, 0.0, yawed_direction.y) * speed * delta


func _rotate_camera_facing(direction: float, delta: float) -> void:
	if free_float_camera:
		free_camera_yaw += direction * CAMERA_YAW_SPEED * delta
	else:
		tactical_camera_yaw += direction * CAMERA_YAW_SPEED * delta


func _update_camera(delta: float) -> void:
	camera_distance = lerpf(camera_distance, target_camera_distance, clampf(delta * 9.0, 0.0, 1.0))
	var desired_offset := Vector3(
		sin(tactical_camera_yaw) * camera_distance,
		camera_distance * 0.78,
		cos(tactical_camera_yaw) * camera_distance)
	if free_float_camera:
		var pitch := deg_to_rad(free_camera_pitch)
		desired_offset = Vector3(
			sin(free_camera_yaw) * cos(pitch),
			sin(pitch),
			cos(free_camera_yaw) * cos(pitch)) * camera_distance
	var desired_position := camera_target + desired_offset
	desired_position.y = _camera_clearance_height(camera_target, desired_position)
	camera.global_position = camera.global_position.lerp(desired_position, clampf(delta * 10.0, 0.0, 1.0))
	camera.look_at(camera_target, Vector3.UP)
	if zoom_focus_pending:
		var under_cursor: Vector2 = _screen_to_world(zoom_focus_screen)
		var correction := zoom_focus_world - under_cursor
		camera_target.x += correction.x
		camera_target.z += correction.y
		camera.global_position += Vector3(correction.x, 0.0, correction.y)
		camera.look_at(camera_target, Vector3.UP)
		# Camera distance eases over several frames. Re-anchor through that entire
		# movement, otherwise the first correction is subsequently pulled back
		# toward the screen centre by the remaining camera interpolation.
		if absf(camera_distance - target_camera_distance) <= ZOOM_FOCUS_TOLERANCE and correction.length() <= ZOOM_FOCUS_TOLERANCE:
			zoom_focus_pending = false


func _zoom_toward_cursor(screen_position: Vector2, multiplier: float) -> void:
	var anchor := _screen_to_world(screen_position)
	zoom_focus_world = anchor
	zoom_focus_screen = screen_position
	zoom_focus_pending = true
	target_camera_distance = clampf(target_camera_distance * multiplier, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE)


func _camera_clearance_height(target: Vector3, desired: Vector3) -> float:
	var floor_height := _terrain_height_at(desired.x, desired.z) + CAMERA_TERRAIN_CLEARANCE
	# Check the whole camera-to-target line so a ridge between the focus point
	# and camera cannot occlude the near view when zooming into relief.
	for sample in range(1, 9):
		var fraction := float(sample) / 8.0
		var point := target.lerp(desired, fraction)
		floor_height = maxf(floor_height, _terrain_height_at(point.x, point.z) + CAMERA_TERRAIN_CLEARANCE)
	return maxf(desired.y, floor_height)


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
	_update_hud()


func _select_nearest(screen_position: Vector2) -> void:
	var closest_id := -1
	# At strategic zoom a vehicle's rendered hull is only a few pixels wide.
	# Use a forgiving but bounded screen-space hit radius for a direct click.
	var closest_distance := clampf(camera_distance * 0.22, 20.0, 32.0)
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
		return

	var closest_structure: Node3D
	for structure in completed_structure_views:
		if not is_instance_valid(structure) or camera.is_position_behind(structure.global_position):
			continue
		var distance := camera.unproject_position(structure.global_position).distance_to(screen_position)
		if distance < closest_distance:
			closest_distance = distance
			closest_structure = structure
	if closest_structure != null:
		_set_selected_structure(closest_structure)


func _update_hover_context() -> void:
	var pointer := get_viewport().get_mouse_position()
	var nearest_distance := clampf(camera_distance * 0.22, 20.0, 32.0)
	var next_context: Dictionary = command_hud.get_build_hover_context()
	if next_context.is_empty():
		for entity_id in entity_ids:
			var world_position := _entity_world_position(entity_id)
			if camera.is_position_behind(world_position):
				continue
			var distance := camera.unproject_position(world_position).distance_to(pointer)
			if distance < nearest_distance:
				nearest_distance = distance
				var unit := _selected_unit_snapshot(entity_id)
				var allegiance := "FRIENDLY" if player_entity_ids.has(entity_id) else "HOSTILE"
				next_context = {
					"key": "unit:%d" % entity_id,
					"title": "%s // %s" % [allegiance, unit.get("name", "COMBAT UNIT")],
					"detail": "INTEGRITY %.0f / %.0f  //  UNIT %d" % [unit.get("health", 0.0), unit.get("max_health", 0.0), entity_id],
					"accent": Color("#7ad99b") if allegiance == "FRIENDLY" else Color("#ff6d65"),
				}
		for structure in completed_structure_views:
			if not is_instance_valid(structure) or camera.is_position_behind(structure.global_position):
				continue
			var distance := camera.unproject_position(structure.global_position).distance_to(pointer)
			if distance < nearest_distance:
				nearest_distance = distance
				var structure_type := int(structure.get_meta("structure_type", -1))
				next_context = {
					"key": "structure:%s" % structure.get_instance_id(),
					"title": "FRIENDLY // %s" % _structure_display_name(structure_type),
					"detail": "WRENCH %.0f  //  BOLT %.0f  //  TACTICAL INSTALLATION" % [_structure_material_cost(structure_type), _structure_energy_cost(structure_type)],
					"accent": Color("#47d9ff"),
				}
	if next_context.get("key", "") == hover_context.get("key", ""):
		return
	hover_context = next_context
	_update_hud()


func _select_in_rectangle(bounds: Rect2) -> void:
	for entity_id in player_entity_ids:
		var world_position := _entity_world_position(entity_id)
		if not camera.is_position_behind(world_position) and bounds.has_point(camera.unproject_position(world_position)):
			_set_selected(entity_id, true)


func _clear_selection() -> void:
	for entity_id in selected_ids:
		_set_instance_color(entity_id, entity_base_colors.get(entity_id, UNIT_BASE_COLOR))
	selected_ids.clear()
	_set_selected_structure(null)


func _set_selected_structure(structure: Node3D) -> void:
	if selected_structure_view != null and is_instance_valid(selected_structure_view):
		var old_label := selected_structure_view.get_node_or_null("StructureStatus") as Label3D
		if old_label != null:
			old_label.visible = false
	selected_structure_view = structure
	if selected_structure_view != null and is_instance_valid(selected_structure_view):
		var label := selected_structure_view.get_node_or_null("StructureStatus") as Label3D
		if label != null:
			label.visible = true


func _set_selected(entity_id: int, selected: bool) -> void:
	if selected and not selected_ids.has(entity_id):
		selected_ids.append(entity_id)
		_set_instance_color(entity_id, UNIT_SELECTED_COLOR)
		if prototype_visual_views.has(entity_id):
			prototype_visual_views[entity_id].set_selected(true)
	elif not selected and selected_ids.has(entity_id):
		selected_ids.remove_at(selected_ids.find(entity_id))
		_set_instance_color(entity_id, entity_base_colors.get(entity_id, UNIT_BASE_COLOR))
		if prototype_visual_views.has(entity_id):
			prototype_visual_views[entity_id].set_selected(false)


func _set_instance_color(entity_id: int, color: Color) -> void:
	var instance_index: int = entity_to_instance.get(entity_id, -1)
	if demo_mode:
		_set_demo_unit_color(entity_id, color)
	elif instance_index >= 0:
		unit_multimesh.set_instance_color(instance_index, color)


func _terrain_height_at(world_x: float, world_z: float) -> float:
	if terrain_heights.is_empty():
		return 0.4
	var x := clampi(roundi((world_x / terrain_world_width + 0.5) * float(TERRAIN_SAMPLE_WIDTH - 1)), 0, TERRAIN_SAMPLE_WIDTH - 1)
	var z := clampi(roundi((world_z / terrain_world_height + 0.5) * float(TERRAIN_SAMPLE_HEIGHT - 1)), 0, TERRAIN_SAMPLE_HEIGHT - 1)
	return HeightMap.presentation_height(terrain_heights[z * TERRAIN_SAMPLE_WIDTH + x], world_x, terrain_world_width) + 0.8


func _rebuild_terrain_trees() -> void:
	for tree in forest_landmarks.get_children():
		tree.queue_free()
	var tree_mesh := _make_low_poly_tree_mesh()
	var strategic_mesh := CylinderMesh.new()
	strategic_mesh.top_radius = 0.0
	strategic_mesh.bottom_radius = 4.6
	strategic_mesh.height = 9.0
	strategic_mesh.radial_segments = 5
	strategic_mesh.material = _tree_material(Color("#244f2d"), 0.0)

	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = tree_mesh
	multimesh.instance_count = TREE_INSTANCE_TARGET
	var tree_transforms: Array[Transform3D] = []
	var rng := RandomNumberGenerator.new()
	rng.seed = 640640
	var placed := 0
	var attempts := 0
	while placed < TREE_INSTANCE_TARGET and attempts < TREE_INSTANCE_TARGET * 8:
		attempts += 1
		var world_x := rng.randf_range(-terrain_world_width * 0.46, terrain_world_width * 0.46)
		var world_z := rng.randf_range(-terrain_world_height * 0.46, terrain_world_height * 0.46)
		# The strait and its shore remain clear; foliage belongs on established land.
		if absf(world_x) < terrain_world_width * 0.24:
			continue
		var scale := rng.randf_range(0.55, 1.25)
		var transform := Transform3D(Basis(Vector3.UP, rng.randf_range(0.0, TAU)).scaled(Vector3(scale, scale, scale)), Vector3(world_x, _terrain_height_at(world_x, world_z) + 6.0 * scale, world_z))
		tree_transforms.append(transform)
		multimesh.set_instance_transform(placed, transform)
		placed += 1

	# All instances are populated before assignment, so this stays one batched draw.
	terrain_trees.multimesh = multimesh
	terrain_trees.custom_aabb = AABB(Vector3(-terrain_world_width * 0.5, -4.0, -terrain_world_height * 0.5), Vector3(terrain_world_width, 36.0, terrain_world_height))
	terrain_trees.visible = placed == TREE_INSTANCE_TARGET
	var strategic_multimesh := MultiMesh.new()
	strategic_multimesh.transform_format = MultiMesh.TRANSFORM_3D
	strategic_multimesh.mesh = strategic_mesh
	var strategic_transforms: Array[Transform3D] = []
	for index in range(0, tree_transforms.size(), 2):
		var source_transform := tree_transforms[index]
		var strategic_position := source_transform.origin
		strategic_transforms.append(Transform3D(
			Basis(Vector3.UP, source_transform.basis.get_euler().y).scaled(Vector3(0.72, 0.72, 0.72)),
			Vector3(strategic_position.x, strategic_position.y - 2.0, strategic_position.z)))
	strategic_multimesh.instance_count = strategic_transforms.size()
	for index in range(strategic_transforms.size()):
		strategic_multimesh.set_instance_transform(index, strategic_transforms[index])
	strategic_trees.multimesh = strategic_multimesh
	strategic_trees.custom_aabb = terrain_trees.custom_aabb
	strategic_trees.visible = false

	# Keep a sparse, close-range silhouette layer as well: it makes the forest
	# legible from the opening camera while the MultiMesh supplies map-wide cover.
	for side in [-1.0, 1.0]:
		for row in range(4):
			for column in range(3):
				var landmark := MeshInstance3D.new()
				landmark.mesh = tree_mesh
				landmark.position = Vector3(
					side * (terrain_world_width * 0.31 + float(column) * 12.0),
					_terrain_height_at(side * (terrain_world_width * 0.31 + float(column) * 12.0), -120.0 + float(row) * 78.0) + 7.0,
					-120.0 + float(row) * 78.0
				)
				landmark.scale = Vector3(1.1, 1.1, 1.1)
				forest_landmarks.add_child(landmark)
	_sync_tree_lod()


func _tree_material(color: Color, emission_strength: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.88
	material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	if emission_strength > 0.0:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = emission_strength
	return material


func _append_tree_surface(target: ArrayMesh, source: Mesh, material: Material, transform: Transform3D) -> void:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	surface.append_from(source, 0, transform)
	surface.set_material(material)
	surface.commit(target)


func _make_low_poly_tree_mesh() -> ArrayMesh:
	var tree_mesh := ArrayMesh.new()
	var trunk := CylinderMesh.new()
	trunk.top_radius = 1.5
	trunk.bottom_radius = 2.2
	trunk.height = 13.0
	trunk.radial_segments = 6
	_append_tree_surface(tree_mesh, trunk, _tree_material(Color("#3d3021"), 0.0), Transform3D(Basis.IDENTITY, Vector3(0.0, 6.5, 0.0)))
	for foliage in [
		{"radius": 7.0, "height": 10.0, "y": 10.0},
		{"radius": 5.8, "height": 9.0, "y": 16.0},
		{"radius": 4.3, "height": 8.0, "y": 21.5},
	]:
		var canopy := CylinderMesh.new()
		canopy.top_radius = 0.15
		canopy.bottom_radius = float(foliage.radius)
		canopy.height = float(foliage.height)
		canopy.radial_segments = 7
		_append_tree_surface(tree_mesh, canopy, _tree_material(Color("#1f5b32"), 0.0), Transform3D(Basis.IDENTITY, Vector3(0.0, float(foliage.y), 0.0)))
	return tree_mesh


func _sync_tree_lod() -> void:
	if terrain_trees.multimesh == null or strategic_trees.multimesh == null:
		return
	var strategic := camera_distance > tree_lod_distance
	terrain_trees.visible = not strategic
	strategic_trees.visible = strategic


func _clear_demo_units() -> void:
	for model in demo_unit_views.values():
		if is_instance_valid(model):
			model.queue_free()
	demo_unit_views.clear()


func _demo_material(color: Color, emission_strength: float = 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = 0.38
	material.roughness = 0.52
	if emission_strength > 0.0:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = emission_strength
	return material


func _armored_material(tint: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_texture = ArmorTexture
	material.albedo_color = tint
	material.metallic = 0.56
	material.roughness = 0.46
	return material


func _add_box_model(root: Node3D, size: Vector3, offset: Vector3, material: Material) -> MeshInstance3D:
	var part := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	part.mesh = mesh
	part.material_override = material
	part.position = offset
	part.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	root.add_child(part)
	return part


func _add_cylinder_model(root: Node3D, radius: float, height: float, offset: Vector3, material: Material) -> MeshInstance3D:
	var part := MeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 12
	part.mesh = mesh
	part.material_override = material
	part.position = offset
	part.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	root.add_child(part)
	return part


func _create_demo_unit(entity_id: int, unit_type: int, faction_color: Color, world_position: Vector3) -> void:
	var root := Node3D.new()
	root.name = "Unit_%d" % entity_id
	root.position = world_position
	root.scale = Vector3(1.75, 1.75, 1.75)
	root.set_meta("faction_color", faction_color)
	root.set_meta("unit_type", unit_type)
	unit_models.add_child(root)
	demo_unit_views[entity_id] = root

	# The hull stays textured armor; faction colour is restricted to the
	# recognizable near-future panels, optics, and beacon instead of tinting the
	# whole Sherman-derived silhouette blue or red.
	var body := _armored_material(Color(0.74, 0.80, 0.72, 1.0))
	var dark := _armored_material(Color(0.29, 0.34, 0.31, 1.0))
	var accent := _demo_material(faction_color.lightened(0.20), 0.35)
	match unit_type:
		0: # Elite main battle tank
			_add_box_model(root, Vector3(2.4, 0.55, 3.1), Vector3(0, 0.28, 0), body)
			_add_box_model(root, Vector3(0.48, 0.42, 3.25), Vector3(-1.18, 0.24, 0), dark)
			_add_box_model(root, Vector3(0.48, 0.42, 3.25), Vector3(1.18, 0.24, 0), dark)
			_add_cylinder_model(root, 0.62, 0.32, Vector3(0, 0.68, -0.15), dark)
			_add_box_model(root, Vector3(0.20, 0.20, 1.9), Vector3(0, 0.72, -1.05), accent)
			_add_box_model(root, Vector3(1.72, 0.12, 0.62), Vector3(0, 0.63, 0.70), accent)
			_add_box_model(root, Vector3(0.14, 0.30, 1.85), Vector3(-1.23, 0.56, 0), accent)
			_add_box_model(root, Vector3(0.14, 0.30, 1.85), Vector3(1.23, 0.56, 0), accent)
		1: # Long-range artillery
			_add_box_model(root, Vector3(2.1, 0.50, 3.0), Vector3(0, 0.25, 0), body)
			_add_box_model(root, Vector3(0.44, 0.38, 3.12), Vector3(-1.02, 0.22, 0), dark)
			_add_box_model(root, Vector3(0.44, 0.38, 3.12), Vector3(1.02, 0.22, 0), dark)
			_add_cylinder_model(root, 0.48, 0.30, Vector3(0, 0.62, 0.15), dark)
			_add_box_model(root, Vector3(0.18, 0.18, 3.2), Vector3(0, 0.76, -1.55), accent)
		2, 5: # Anti-air platforms
			_add_box_model(root, Vector3(2.0, 0.48, 2.4), Vector3(0, 0.24, 0), body)
			_add_cylinder_model(root, 0.54, 0.34, Vector3(0, 0.62, 0), dark)
			_add_box_model(root, Vector3(0.13, 0.13, 1.8), Vector3(-0.28, 0.88, -0.78), accent)
			_add_box_model(root, Vector3(0.13, 0.13, 1.8), Vector3(0.28, 0.88, -0.78), accent)
		3: # Mass swarm tank
			_add_box_model(root, Vector3(2.8, 0.42, 2.5), Vector3(0, 0.21, 0), body)
			_add_box_model(root, Vector3(1.25, 0.36, 1.25), Vector3(0, 0.60, -0.15), dark)
			_add_box_model(root, Vector3(0.18, 0.16, 1.45), Vector3(0, 0.65, -0.95), accent)
		4: # Mass assault vehicle
			_add_box_model(root, Vector3(2.7, 0.72, 2.4), Vector3(0, 0.36, 0), body)
			_add_cylinder_model(root, 0.46, 0.46, Vector3(0, 0.88, -0.25), dark)
			_add_box_model(root, Vector3(0.34, 0.22, 1.5), Vector3(0, 0.92, -1.0), accent)
		12: # Command Walker: the production-capable opening unit
			_add_box_model(root, Vector3(2.8, 0.72, 2.8), Vector3(0, 0.66, 0), body)
			_add_cylinder_model(root, 0.70, 0.48, Vector3(0, 1.18, 0), dark)
			_add_box_model(root, Vector3(0.24, 0.24, 2.2), Vector3(0, 1.30, -1.15), accent)
			_add_box_model(root, Vector3(0.45, 1.05, 0.45), Vector3(-0.92, 0.10, 0.72), dark)
			_add_box_model(root, Vector3(0.45, 1.05, 0.45), Vector3(0.92, 0.10, 0.72), dark)
		_:
			_add_box_model(root, Vector3(2.0, 0.6, 2.0), Vector3(0, 0.3, 0), body)
			_add_cylinder_model(root, 0.42, 0.28, Vector3(0, 0.68, 0), accent)

	# A small emissive command beacon gives every unit a readable faction signal
	# from the strategy camera without relying on missing texture assets.
	_add_cylinder_model(root, 0.20, 0.08, Vector3(0, 1.0, 0.35), accent)
	var strategic_marker := MeshInstance3D.new()
	strategic_marker.name = "StrategicLod"
	strategic_marker.mesh = StrategicUnitIconScript.mesh_for(unit_type)
	strategic_marker.scale = StrategicUnitIconScript.shape_scale_for(unit_type)
	strategic_marker.rotation.y = StrategicUnitIconScript.rotation_for(unit_type)
	strategic_marker.position = Vector3(0, 5.0, 0)
	strategic_marker.material_override = StrategicUnitIconScript.material_for(faction_color)
	strategic_marker.visible = false
	root.add_child(strategic_marker)
	var strategic_stem := MeshInstance3D.new()
	strategic_stem.name = "StrategicMarkerStem"
	var strategic_stem_mesh := CylinderMesh.new()
	strategic_stem_mesh.top_radius = 0.10
	strategic_stem_mesh.bottom_radius = 0.10
	strategic_stem_mesh.height = 5.0
	strategic_stem_mesh.radial_segments = 6
	strategic_stem.mesh = strategic_stem_mesh
	strategic_stem.position = Vector3(0, 2.5, 0)
	strategic_stem.material_override = StrategicUnitIconScript.material_for(faction_color)
	strategic_stem.visible = false
	root.add_child(strategic_stem)
	# SupCom-style strategic range overlays, intentionally faceted into six
	# straight segments so intelligence and weapon envelopes read as hexes.
	_add_hex_range_ring(root, "VisibilityHex", 15.0, 0.13, Color("#123a72"))
	_add_hex_range_ring(root, "RadarHex", 22.0, 0.18, Color("#8c3cc7"))
	_add_hex_range_ring(root, "AttackHex", 10.0, 0.23, Color("#d83b3b"))

func _add_hex_range_ring(root: Node3D, ring_name: String, world_radius: float, height: float, color: Color) -> void:
	var ring := MeshInstance3D.new()
	ring.name = ring_name
	ring.top_level = true
	var material := _demo_material(color, 2.2)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var lines := ImmediateMesh.new()
	lines.surface_begin(Mesh.PRIMITIVE_LINES)
	for side in range(6):
		var a := TAU * float(side) / 6.0 + PI / 6.0
		var b := TAU * float(side + 1) / 6.0 + PI / 6.0
		var start := Vector3(root.global_position.x + cos(a) * world_radius, _terrain_height_at(root.global_position.x + cos(a) * world_radius, root.global_position.z + sin(a) * world_radius) + height, root.global_position.z + sin(a) * world_radius)
		var finish := Vector3(root.global_position.x + cos(b) * world_radius, _terrain_height_at(root.global_position.x + cos(b) * world_radius, root.global_position.z + sin(b) * world_radius) + height, root.global_position.z + sin(b) * world_radius)
		lines.surface_add_vertex(start)
		lines.surface_add_vertex(finish)
	lines.surface_end()
	lines.surface_set_material(0, material)
	ring.mesh = lines
	ring.material_override = material
	ring.visible = true
	root.add_child(ring)

func _set_unit_lod(root: Node3D) -> void:
	var strategic := camera_distance > 500.0
	for child in root.get_children():
		if child.name in ["SelectionMarker", "VisibilityHex", "RadarHex", "AttackHex", "StrategicLod", "StrategicMarkerStem"]:
			continue
		child.visible = not strategic
	var marker := root.get_node_or_null("StrategicLod") as MeshInstance3D
	var stem := root.get_node_or_null("StrategicMarkerStem") as MeshInstance3D
	if marker != null:
		marker.visible = false
	if stem != null:
		stem.visible = false


func _set_demo_unit_color(entity_id: int, color: Color) -> void:
	var root: Node3D = demo_unit_views.get(entity_id, null)
	if root == null:
		return
	var selected := color == UNIT_SELECTED_COLOR
	var selection_marker := root.get_node_or_null("SelectionMarker") as MeshInstance3D
	if selection_marker != null:
		selection_marker.visible = selected
	for ring_name in ["VisibilityHex", "RadarHex", "AttackHex"]:
		var ring := root.get_node_or_null(ring_name) as Node3D
		if ring != null:
			ring.visible = true


func _issue_move_order(screen_position: Vector2) -> void:
	if selected_ids.is_empty():
		var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
		if commander_id <= 0:
			return
		_set_selected(commander_id, true)
	if selected_ids.is_empty():
		return

	var target := _screen_to_world(screen_position)
	var accepted_count: int = extension.call(
		"issue_move_commands",
		selected_ids,
		HUMAN_PLAYER_ID,
		target.x,
		target.y,
		UNIT_SPACING
	)
	if accepted_count != selected_ids.size():
		push_warning("Move order rejected: accepted=%d/%d entity=%d target=(%.6f,%.6f)" % [accepted_count, selected_ids.size(), selected_ids[0], target.x, target.y])


func _issue_stop_order() -> void:
	if selected_ids.is_empty():
		return
	var accepted_count: int = extension.call("issue_stop_commands", selected_ids, HUMAN_PLAYER_ID)
	if accepted_count != selected_ids.size():
		push_warning("Stop order rejected by authoritative command validation")


func _order_fob(screen_position: Vector2) -> void:
	var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
	if selected_ids.is_empty() or commander_id < 0:
		fob_build_mode = false
		return
	var ray_origin := camera.project_ray_origin(screen_position)
	var ray_direction := camera.project_ray_normal(screen_position)
	var intersection = Plane(Vector3.UP, 0.0).intersects_ray(ray_origin, ray_direction)
	if intersection == null:
		return
	var target: Vector3 = intersection
	var accepted := int(extension.call("issue_install_commands", selected_ids, HUMAN_PLAYER_ID, target.x, target.z, 1))
	if accepted == selected_ids.size():
		fob_build_target = Vector2(target.x, target.z)
		fob_was_constructing = false
		fob_completion_notification = ""
	else:
		push_warning("FOB placement rejected: select a unit with CONSTRUCT_FOB capability")
	fob_build_mode = false


func _queue_commander_unit(unit_type: int, target: Vector2 = Vector2.ZERO) -> void:
	var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
	if commander_id < 0 or not selected_ids.has(_player_engineer_id()):
		return
	var accepted_count: int = extension.call("issue_build_commands", PackedInt32Array([commander_id]), HUMAN_PLAYER_ID, target.x, target.y, unit_type)
	if accepted_count == 1:
		pending_player_build_types.append(unit_type)
		_update_hud()
	else:
		push_warning("Field Engineer could not queue this unit")

func _order_commander_to_build(build_type: int, target: Vector2) -> void:
	var commander_id := _player_engineer_id()
	if commander_id < 0:
		return
	var commander_position := Vector2(float(extension.call("get_unit_x", commander_id)), float(extension.call("get_unit_y", commander_id)))
	var direction := (target - commander_position).normalized()
	if direction.length_squared() < 0.001:
		direction = Vector2.RIGHT
	# Stop beyond the scaffold footprint rather than pathing the commander into
	# the finished building. The original target remains the construction site.
	var approach_target := target - direction * 7.0
	# The authoritative move validator rejects zero formation spacing. A single
	# commander still uses the normal RTS spacing contract used by manual moves.
	var accepted := int(extension.call("issue_move_commands", PackedInt32Array([commander_id]), HUMAN_PLAYER_ID, approach_target.x, approach_target.y, UNIT_SPACING))
	if accepted != 1:
		push_warning("Field Engineer could not reach the build location")
		return
	pending_build_order = {"type": build_type, "target": target, "approach": approach_target}

func _process_pending_build_order() -> void:
	if pending_build_order.is_empty():
		return
	var commander_id := _player_engineer_id()
	var target: Vector2 = pending_build_order.target
	var approach: Vector2 = pending_build_order.approach
	var dx := float(extension.call("get_unit_x", commander_id)) - approach.x
	var dz := float(extension.call("get_unit_y", commander_id)) - approach.y
	if dx * dx + dz * dz > 16.0:
		return
	var build_type := int(pending_build_order.type)
	if build_type >= 100:
		if _queue_commander_structure(build_type - 100, target):
			pending_completed_structures.append({"type": build_type - 100, "target": target})
	else:
		_queue_commander_unit(build_type, target)
	pending_build_order.clear()

func _screen_to_world(screen_position: Vector2) -> Vector2:
	var ray_origin := camera.project_ray_origin(screen_position)
	var ray_direction := camera.project_ray_normal(screen_position)
	# A flat y=0 raycast lands kilometers beyond the visible cursor on our
	# mountain-scale heightfield. March to the actual terrain surface, then
	# refine the crossing so both the ghost and final build site share it.
	if ray_direction.y < -0.00001:
		var prior_t := 0.0
		var prior_point := ray_origin
		var prior_above := prior_point.y - _terrain_height_at(prior_point.x, prior_point.z)
		for step in range(1, 241):
			var t := float(step) * 500.0
			var point := ray_origin + ray_direction * t
			var above := point.y - _terrain_height_at(point.x, point.z)
			if prior_above >= 0.0 and above <= 0.0:
				var low := prior_t
				var high := t
				for _refinement in range(12):
					var middle := (low + high) * 0.5
					var middle_point := ray_origin + ray_direction * middle
					if middle_point.y >= _terrain_height_at(middle_point.x, middle_point.z):
						low = middle
					else:
						high = middle
				var terrain_point := ray_origin + ray_direction * ((low + high) * 0.5)
				return Vector2(terrain_point.x, terrain_point.z)
			prior_t = t
			prior_point = point
			prior_above = above
	var intersection = Plane(Vector3.UP, 0.0).intersects_ray(ray_origin, ray_direction)
	return Vector2(intersection.x, intersection.z) if intersection != null else Vector2.ZERO

func _begin_build_placement(build_type: int) -> void:
	var commander_id := _player_engineer_id()
	if commander_id < 0 or not selected_ids.has(commander_id):
		push_warning("Select the Field Engineer before placing a build")
		return
	build_mode_type = build_type
	selecting = false
	selection_rect.visible = false


func _cancel_tactical_modes() -> void:
	build_mode_type = -1
	blueprint_pointer_down = false
	road_build_mode = false
	road_start = Vector2.INF
	fob_build_mode = false
	material_order_mode = 0
	if build_ghost != null:
		build_ghost.queue_free()
		build_ghost = null
	build_ghost_material = null
	if road_preview != null:
		road_preview.queue_free()
		road_preview = null
	road_preview_material = null
	selecting = false
	selection_rect.visible = false

func _blueprint_at_screen(pos: Vector2) -> int:
	var size := get_viewport().get_visible_rect().size
	var ui_scale := clampf(minf(size.x / 1920.0, size.y / 1080.0), 0.62, 1.0)
	var logical := pos / ui_scale
	var logical_size := size / ui_scale
	var origin := Vector2((logical_size.x - 540.0) * 0.5, 12.0)
	if logical.x < origin.x or logical.x > origin.x + 540.0 or logical.y < origin.y + 30.0 or logical.y > origin.y + 156.0: return -1
	var col := int(clampf((logical.x - origin.x - 8.0) / 132.0, 0.0, 3.0))
	if logical.y < origin.y + 70.0:
		return _build_unit_type_at_index(col)
	if logical.y < origin.y + 108.0:
		return _build_unit_type_at_index(col + 4)
	if logical.y >= origin.y + 124.0:
		return 100 + col
	return -1


func _build_unit_shortcut_index(keycode: Key) -> int:
	match keycode:
		KEY_1: return 0
		KEY_4: return 1
		KEY_5: return 2
		KEY_9: return 3
		KEY_0: return 4
		KEY_P: return 5
	return -1


func _build_unit_catalog() -> Array:
	var catalog: Array = extension.call("get_build_catalog", HUMAN_PLAYER_ID) if extension != null else []
	var units: Array = catalog.filter(func(entry): return not bool(entry.get("is_structure", false)))
	units.sort_custom(func(left, right): return int(left.get("type", -1)) < int(right.get("type", -1)))
	return units


func _build_unit_type_at_index(index: int) -> int:
	var units := _build_unit_catalog()
	if index < 0 or index >= units.size():
		return -1
	return int(units[index].get("type", -1))

func _make_build_ghost(build_type: int) -> Node3D:
	var root := Node3D.new()
	var mesh := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(5, 0.5, 12) if build_type >= 100 and build_type - 100 == 2 else Vector3(5, 2, 5)
	mesh.mesh = box
	build_ghost_material = _demo_material(Color("#47d9ff"), 0.45)
	build_ghost_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	build_ghost_material.albedo_color.a = 0.35
	mesh.material_override = build_ghost_material
	root.add_child(mesh)
	return root

func _begin_road_placement() -> void:
	if _player_engineer_id() < 0 or not selected_ids.has(_player_engineer_id()):
		push_warning("Select the Field Engineer before building a road")
		return
	_cancel_tactical_modes()
	road_build_mode = true
	road_start = Vector2.INF
	_update_hud()

func _handle_road_click(target: Vector2) -> void:
	if road_start == Vector2.INF:
		road_start = target
		_update_road_preview(target)
		return
	if not bool(extension.call("validate_road_placement", road_start.x, road_start.y, target.x, target.y)):
		push_warning("Road placement rejected: road must remain on traversable land with manageable slope")
		return
	var engineer_id := _player_engineer_id()
	var queued := bool(extension.call("queue_road", engineer_id, HUMAN_PLAYER_ID, road_start.x, road_start.y, target.x, target.y))
	if not queued:
		push_warning("Road construction rejected: insufficient resources or unavailable engineer")
	_cancel_tactical_modes()

func _update_road_preview(target: Vector2) -> void:
	if road_preview == null:
		road_preview = MeshInstance3D.new()
		road_preview_material = _demo_material(Color("#47d9ff"), 0.55)
		road_preview_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		road_preview_material.albedo_color.a = 0.55
		road_preview.material_override = road_preview_material
		add_child(road_preview)
	if road_start == Vector2.INF:
		road_preview.visible = false
		return
	road_preview.visible = true
	road_preview.mesh = _make_road_mesh(road_start, target, 18.0)
	var valid := bool(extension.call("validate_road_placement", road_start.x, road_start.y, target.x, target.y)) if extension != null else false
	road_preview_material.albedo_color = Color("#47d9ff") if valid else Color("#ff334d")
	road_preview_material.albedo_color.a = 0.55

func _make_road_mesh(start: Vector2, finish: Vector2, width: float) -> ArrayMesh:
	var direction := finish - start
	if direction.length_squared() < 0.001:
		direction = Vector2.RIGHT
	var side := direction.normalized().orthogonal() * width * 0.5
	var vertices := PackedVector3Array([
		Vector3(start.x + side.x, _terrain_height_at(start.x, start.y) + 0.04, start.y + side.y),
		Vector3(start.x - side.x, _terrain_height_at(start.x, start.y) + 0.04, start.y - side.y),
		Vector3(finish.x + side.x, _terrain_height_at(finish.x, finish.y) + 0.04, finish.y + side.y),
		Vector3(finish.x - side.x, _terrain_height_at(finish.x, finish.y) + 0.04, finish.y - side.y),
	])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0, 1, 2, 2, 1, 3])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3.UP, Vector3.UP, Vector3.UP, Vector3.UP])
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh

func _sync_completed_roads() -> void:
	if extension == null:
		return
	var segments: PackedFloat32Array = extension.call("get_road_segments")
	for offset in range(0, segments.size(), 6):
		if offset + 5 >= segments.size():
			break
		var road_id := int(segments[offset])
		if road_views.has(road_id):
			continue
		var road := MeshInstance3D.new()
		road.name = "Road_%d" % road_id
		road.mesh = _make_road_mesh(Vector2(segments[offset + 1], segments[offset + 2]), Vector2(segments[offset + 3], segments[offset + 4]), segments[offset + 5])
		road.material_override = _demo_material(Color("#343c42"), 0.0)
		add_child(road)
		road_views[road_id] = road

func _set_build_ghost_valid(valid: bool) -> void:
	if build_ghost_material == null:
		return
	build_ghost_material.albedo_color = Color("#47d9ff") if valid else Color("#ff334d")
	build_ghost_material.albedo_color.a = 0.35

func _queue_commander_structure(structure_type: int, target: Vector2) -> bool:
	if not selected_ids.has(_player_engineer_id()):
		return false
	var line_id := int(extension.call("get_faction_production_line", HUMAN_PLAYER_ID))
	if bool(extension.call("queue_structure", line_id, HUMAN_PLAYER_ID, structure_type, target.x, target.y)):
		pending_structure_position = target
		_update_hud()
		return true
	else:
		push_warning("Field Engineer could not queue this structure")
		return false

func _spawn_completed_structure_view(structure_type: int, target: Vector2) -> void:
	var root := Node3D.new()
	root.name = "Built_%s" % ("Outpost" if structure_type == 0 else "RadarMast" if structure_type == 1 else "Airfield" if structure_type == 2 else "Floodlight")
	root.set_meta("structure_type", structure_type)
	root.position = Vector3(target.x, _terrain_height_at(target.x, target.y) + 0.8, target.y)
	var mesh := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(5.0, 1.6 if structure_type == 0 else 7.0 if structure_type == 1 else 0.5 if structure_type == 2 else 2.0, 5.0 if structure_type < 2 else 12.0 if structure_type == 2 else 5.0)
	mesh.mesh = box
	mesh.material_override = _demo_material(Color("#d09a45") if structure_type == 0 else Color("#55b9c9"), 0.0)
	root.add_child(mesh)
	if structure_type == 2:
		var runway := MeshInstance3D.new()
		var runway_mesh := BoxMesh.new()
		runway_mesh.size = Vector3(4.0, 0.08, 18.0)
		runway.mesh = runway_mesh
		runway.position.y = 0.35
		runway.material_override = _demo_material(Color("#8aa0a8"), 0.0)
		root.add_child(runway)
	if structure_type == 1:
		var mast := MeshInstance3D.new()
		var pole := CylinderMesh.new()
		pole.top_radius = 0.22; pole.bottom_radius = 0.35; pole.height = 10.0
		mast.mesh = pole; mast.position.y = 5.0; mast.material_override = _demo_material(Color("#b7d6dc"), 0.0)
		root.add_child(mast)
	if structure_type == 3:
		var mast := MeshInstance3D.new()
		var pole := CylinderMesh.new()
		pole.top_radius = 0.16; pole.bottom_radius = 0.30; pole.height = 15.0
		mast.mesh = pole; mast.position.y = 7.5; mast.material_override = _demo_material(Color("#314955"), 0.0)
		root.add_child(mast)
		var beacon_glow := OmniLight3D.new()
		beacon_glow.name = "Floodlight"
		beacon_glow.position.y = 14.0
		beacon_glow.light_color = Color("#d9efff")
		beacon_glow.light_energy = 6.0
		beacon_glow.omni_range = 360.0
		beacon_glow.omni_attenuation = 1.25
		beacon_glow.shadow_enabled = true
		root.add_child(beacon_glow)
		var lamp := MeshInstance3D.new()
		var lamp_mesh := SphereMesh.new()
		lamp_mesh.radius = 0.72; lamp_mesh.height = 1.44
		lamp.mesh = lamp_mesh; lamp.position.y = 14.0; lamp.material_override = _demo_material(Color("#bdefff"), 3.5)
		root.add_child(lamp)
	var status := Label3D.new()
	status.name = "StructureStatus"
	status.text = "%s\nWRENCH %.0f  //  BOLT %.0f" % [_structure_display_name(structure_type), _structure_material_cost(structure_type), _structure_energy_cost(structure_type)]
	status.position.y = 18.0 if structure_type == 3 else 11.0 if structure_type == 1 else 5.0
	status.font_size = 42
	status.outline_size = 8
	status.pixel_size = 0.12
	status.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	status.modulate = Color("#c9e9f8")
	status.outline_modulate = Color("#061018")
	status.visible = false
	root.add_child(status)
	add_child(root)
	completed_structure_views.append(root)
	_update_terrain_beacon_lighting()


func _update_terrain_beacon_lighting() -> void:
	var lights := PackedVector4Array()
	for structure in completed_structure_views:
		if not is_instance_valid(structure) or structure.get_node_or_null("Floodlight") == null:
			continue
		if lights.size() >= 8:
			break
		var beacon_position := structure.global_position
		lights.append(Vector4(beacon_position.x, beacon_position.z, 360.0, 1.25))
	while lights.size() < 8:
		lights.append(Vector4.ZERO)
	territory_material.set_shader_parameter("fog_beacon_lights", lights)


func _structure_display_name(structure_type: int) -> String:
	return "FORWARD OUTPOST" if structure_type == 0 else "RADAR MAST" if structure_type == 1 else "AIRFIELD" if structure_type == 2 else "FLOODLIGHT"


func _structure_material_cost(structure_type: int) -> float:
	return 300.0 if structure_type == 0 else 450.0 if structure_type == 1 else 600.0 if structure_type == 2 else 380.0


func _structure_energy_cost(structure_type: int) -> float:
	return 150.0 if structure_type == 0 else 250.0 if structure_type == 1 else 400.0 if structure_type == 2 else 620.0


func _sync_construction_frames(queue: Array) -> void:
	for child in construction_frames.get_children():
		child.queue_free()
	if queue.is_empty():
		return
	var queued: Dictionary = queue[0]
	var root := Node3D.new()
	root.name = "InProgress_%s" % String(queued.get("name", "UNIT"))
	var build_x := float(queued.get("target_x", 0.0))
	var build_z := float(queued.get("target_y", 0.0))
	root.position = Vector3(build_x, _terrain_height_at(build_x, build_z) + 0.1, build_z)
	construction_frames.add_child(root)
	var scaffold := _demo_material(Color("#47d9ff"), 0.75)
	scaffold.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	scaffold.albedo_color.a = 0.52
	# The same open frame encloses a structure foundation and the partially
	# assembled unit, making construction state visible before completion.
	for x in [-3.0, 3.0]:
		for z in [-2.5, 2.5]:
			_add_box_model(root, Vector3(0.18, 5.0, 0.18), Vector3(x, 2.5, z), scaffold)
	_add_box_model(root, Vector3(6.3, 0.18, 0.18), Vector3(0, 5.0, -2.5), scaffold)
	_add_box_model(root, Vector3(6.3, 0.18, 0.18), Vector3(0, 5.0, 2.5), scaffold)
	_add_box_model(root, Vector3(0.18, 0.18, 5.3), Vector3(-3.0, 5.0, 0), scaffold)
	_add_box_model(root, Vector3(0.18, 0.18, 5.3), Vector3(3.0, 5.0, 0), scaffold)
	var completion := clampf(float(queued.get("progress", 0.0)), 0.06, 1.0)
	_add_box_model(root, Vector3(4.7 * completion, 0.8, 2.8), Vector3(0, 0.45, 0), scaffold)


func _issue_attack_order(screen_position: Vector2) -> void:
	if selected_ids.is_empty():
		var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
		if commander_id <= 0:
			return
		_set_selected(commander_id, true)
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
	return Vector3(x, _terrain_height_at(x, z), z)


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
	_update_steering_debug()
	var scenario_name: String = scenario_definition.get("display_name", "Scale profile")
	debug_label.text = ("%s\nPlayer: %d  Enemy: %d  Selected: %d\nFPS: %d  Sim: %.2f ms" % [
		scenario_name,
		player_entity_ids.size(),
		ai_entity_ids.size(),
		selected_ids.size(),
		Engine.get_frames_per_second(),
		extension.call("get_simulation_tick_ms"),
	]).to_upper()
	var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
	var storage: Array = extension.call("economy_get_storage_info", commander_id)
	var selection_detail := "Drag-select a force to inspect it."
	if material_order_mode == 1:
		selection_detail = "CLAIM MODE  •  RIGHT-CLICK A MATERIAL FACILITY"
	elif material_order_mode == 2:
		selection_detail = "DEMOLISH MODE  •  RIGHT-CLICK A MATERIAL FACILITY"
	elif fob_build_mode:
		selection_detail = "FOB PLACEMENT  •  LEFT-CLICK TERRAIN"
	var selected_unit: Dictionary = {}
	var selected_off_road: PackedFloat32Array = PackedFloat32Array()
	if selected_ids.size() == 1:
		selected_unit = _selected_unit_snapshot(selected_ids[0])
		selected_off_road = extension.call("get_unit_off_road_state", selected_ids[0])
		if selected_off_road.size() >= 3:
			var travel_surface := "ROAD" if selected_off_road[2] > 0.98 else "OFF-ROAD"
			selection_detail = "%s  •  %s  •  WEAR %.1f  •  %.0f%% SPEED" % [
				selected_unit.get("name", "UNIT"),
				travel_surface,
				selected_off_road[0],
				selected_off_road[2] * 100.0,
			]
		else:
			selection_detail = "%s  •  AWAITING ORDERS" % selected_unit.get("name", "UNIT")
	elif selected_ids.size() > 1:
		selection_detail = "FORMATION READY  •  RIGHT-CLICK TO MOVE"
	var production_queue: Array = extension.call("get_production_queue", commander_id) if commander_id > 0 else []
	var fob_installation: Array = []
	if fob_build_target != Vector2.INF:
		fob_installation = extension.call("territory_get_installation_info", fob_build_target.x, fob_build_target.y)
		if not fob_installation.is_empty():
			var fob_constructing := int(fob_installation[3]) == 1
			if fob_was_constructing and not fob_constructing:
				fob_completion_notification = "FOB ONLINE  //  LOGISTICS LINK ESTABLISHED"
			fob_was_constructing = fob_constructing
	_sync_construction_frames(production_queue)
	if production_queue.is_empty() and not pending_completed_structures.is_empty():
		for structure in pending_completed_structures:
			_spawn_completed_structure_view(int(structure.type), structure.target)
		pending_completed_structures.clear()
	command_hud.refresh({
		"scenario": scenario_name,
		"friendly": player_entity_ids.size(),
		"enemy": ai_entity_ids.size(),
		"selected": selected_ids.size(),
		"selection_detail": selection_detail,
		"unit_name": selected_unit.get("name", ""),
		"unit_id": selected_unit.get("id", ""),
		"unit_health": selected_unit.get("health", -1.0),
		"unit_max_health": selected_unit.get("max_health", -1.0),
		"off_road_wear": float(selected_off_road[0]) if selected_off_road.size() >= 3 else -1.0,
		"off_road_distance": float(selected_off_road[1]) if selected_off_road.size() >= 3 else -1.0,
		"off_road_speed_multiplier": float(selected_off_road[2]) if selected_off_road.size() >= 3 else -1.0,
		"force_status": "READY" if selected_ids.is_empty() else "COMMAND LINKED",
		"hover_title": hover_context.get("title", "TACTICAL INSPECT"),
		"hover_detail": hover_context.get("detail", "HOVER A UNIT OR STRUCTURE TO INSPECT"),
		"hover_accent": hover_context.get("accent", Color("#7f9ba8")),
		"can_build": selected_ids.size() == 1 and selected_ids[0] == _player_engineer_id(),
		"queue": production_queue,
		"fob_installation": fob_installation,
		"fob_completion_notification": fob_completion_notification,
		"build_catalog": extension.call("get_build_catalog", HUMAN_PLAYER_ID) if commander_id > 0 else [],
		"material": _hud_resource(storage, 0),
		"energy": _hud_resource(storage, 1),
		"research": _hud_resource(storage, 2),
		"material_income": "",
		"energy_income": "",
		"research_income": "",
		"tick": extension.call("get_simulation_tick_ms"),
		"fps": Engine.get_frames_per_second(),
		"engine": "ONLINE",
	})


func _hud_resource(storage: Array, index: int) -> String:
	if storage.size() <= index:
		return "--"
	return "%.0f" % float(storage[index])


func _selected_unit_snapshot(entity_id: int) -> Dictionary:
	var root: Node3D = demo_unit_views.get(entity_id, null)
	var unit_type := int(root.get_meta("unit_type", -1)) if root != null else -1
	var health: Array = extension.call("get_unit_health", entity_id)
	return {
		"id": entity_id,
		"name": UNIT_TYPE_DISPLAY_NAME.get(unit_type, "COMBAT UNIT"),
		"health": float(health[0]) if health.size() >= 2 else -1.0,
		"max_health": float(health[1]) if health.size() >= 2 else -1.0,
	}
