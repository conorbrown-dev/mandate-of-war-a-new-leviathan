extends Node3D

const DEFAULT_UNIT_COUNT := 1000
const DEFAULT_SKIRMISH_PATH := "res://scenarios/commander_start_skirmish.json"
const SKIRMISH_PRESENTATION_STARTING_MATERIALS := 5000.0
const SKIRMISH_PRESENTATION_STARTING_FUEL := 5000.0
const UNIT_SPACING := 3.0
const UNIT_BASE_COLOR := Color(0.12, 0.62, 1.0, 1.0)
const UNIT_SELECTED_COLOR := Color(1.0, 0.72, 0.08, 1.0)
const PLAYER_FACTION_COLOR := Color(0.12, 0.62, 1.0, 1.0)
const AI_FACTION_COLOR := Color(0.92, 0.20, 0.18, 1.0)
const HUMAN_PLAYER_ID := 0
const FACTION_UNIT_VISUAL_OVERRIDES := {
	# The default Broken Strait player starts as faction 0 (Industrial). Its
	# runway fighter retains the stable native type 9 while using its own F-15C
	# presentation definition rather than the Elite donor visual.
	0: {9: "visual.industrial.fighter.f15c.prototype"}
}
const CAMERA_MIN_DISTANCE := 24.0
const CAMERA_MAX_DISTANCE := 26000.0
const CAMERA_ZOOM_LEVELS := [24.0, 120.0, 420.0, 1500.0, 4500.0, 12000.0, 26000.0]
# From this zoom a pixel covers metres of ground, so clicks are approximate.
const APPROXIMATE_CLICK_MIN_CAMERA_DISTANCE := 2000.0
const APPROXIMATE_CLICK_PIXELS := 24.0
const CAMERA_MIN_FAR_DISTANCE := 120000.0
# The near plane follows zoom. A fixed 5 cm near plane against a 120 km far
# plane leaves too little depth precision to keep flat paving off the ground.
const CAMERA_NEAR_PER_DISTANCE := 0.01
const CAMERA_MIN_NEAR := 0.05
const CAMERA_MAX_NEAR := 40.0
# The airfield fills the 250 x 750 m site the native rules level for it
# (structure_sites.hpp): local X across the site, local Z along it. All paving
# lies on one plane and the pieces only meet edge to edge, so none overlap.
const AirfieldSurfaceShader := preload("res://structures/airfield_surface.gdshader")
const AIRFIELD_PAVING_HEIGHT := 0.25
const AIRFIELD_RUNWAY_LENGTH := 700.0
const AIRFIELD_PAVING := [
	# Runway: 45 m wide. At 19.44 m an F-15C is about 1/36 of its length.
	{"kind": 0, "center": Vector2(-55.0, 0.0), "size": Vector2(45.0, AIRFIELD_RUNWAY_LENGTH), "yaw": 0.0},
	# Parallel taxiway and the two link taxiways at the runway ends.
	# Its runway-side edge line stops where the link taxiways join it.
	{"kind": 1, "center": Vector2(12.0, 0.0), "size": Vector2(18.0, 600.0), "yaw": 0.0, "edge_gap": Vector3(-1.0, 282.0, 301.0)},
	{"kind": 1, "center": Vector2(-14.75, 291.0), "size": Vector2(18.0, 35.5), "yaw": 90.0},
	{"kind": 1, "center": Vector2(-14.75, -291.0), "size": Vector2(18.0, 35.5), "yaw": 90.0},
	# Concrete apron in front of the hangar.
	{"kind": 2, "center": Vector2(52.0, -20.0), "size": Vector2(62.0, 200.0), "yaw": 0.0},
]
const AIRFIELD_HANGAR_SIZE := Vector3(36.0, 12.0, 56.0)
const AIRFIELD_HANGAR_POSITION := Vector3(101.0, 6.0, -40.0)
const AIRFIELD_TOWER_POSITION := Vector3(100.0, 10.0, 110.0)
const CAMERA_TERRAIN_CLEARANCE := 6.0
# Drawn flight height. Real cruise heights would put aircraft off screen at
# tactical zoom, so this is compressed like the rest of the battlefield.
# A drawn unit further than this from its new simulated position jumps
# straight there instead of sliding across the gap.
const UNIT_SNAP_DISTANCE := 60.0
# Half the distance over which the ground slope under a vehicle is measured.
const UNIT_TILT_SAMPLE_SPAN := 3.0
const AIRCRAFT_CRUISE_ALTITUDE := 160.0
const AIRCRAFT_CLIMB_RATE := 45.0
# Matches the water surface drawn by the Ocean and OpenSea meshes.
const AIRCRAFT_SEA_LEVEL := 0.0
const UNIT_MODEL_GROUND_CLEARANCE := 1.2
const ENGINEER_MODEL_GROUND_CLEARANCE := 0.05
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
const ResponsiveUiScript = preload("res://ui/responsive_ui.gd")
const StrategicUnitIconScript = preload("res://strategic_unit_icon.gd")
const MESH_BASE_PATH := "res://scenarios/meshes/"
const TERRAIN_SAMPLE_WIDTH := 320
const TERRAIN_SAMPLE_HEIGHT := 320
const CIVILIAN_DRESSING_PATH := "res://scenarios/civilian_dressing.json"
const FUEL_DEPOTS_PATH := "res://scenarios/fuel_depots.json"
const FUEL_STATUS_SECONDS := 8.0
const RECON_PLANE_UNIT_TYPE := 14
const FogShader := preload("res://fog_of_war.gdshader")
const MatchEndPanelScript := preload("res://ui/components/match_end_panel.gd")
const ExitConfirmScript := preload("res://ui/components/exit_confirm.gd")
const MandateTokens := preload("res://ui/theme/ui_tokens.gd")
const TerrainVegetation := preload("res://terrain_vegetation.gd")
const UnitCatalog := preload("res://unit_catalog.gd")
const TERRAIN_MASKS_PATH := "res://scenarios/terrain_masks.png"
const TREE_INSTANCE_TARGET := 960
const TREE_CLUSTER_COUNT := 9
const SkirmishConfigLoader := preload("res://skirmish_config.gd")
const ControlPointPresentationScript := preload("res://control_point_presentation.gd")
const ReinforcementPresentationScript := preload("res://reinforcement_delivery_presentation.gd")
const RangeEnvelopeRendererScript := preload("res://range_envelope_renderer.gd")
const OrderOverlayRendererScript := preload("res://order_overlay_renderer.gd")
const UnitRouteRendererScript := preload("res://unit_route_renderer.gd")
## How often held-Shift route lines re-read the native routes as units move.
const UNIT_ROUTE_REFRESH_SECONDS := 0.1
const CombatEffectsScript := preload("res://combat_effects.gd")
const UiAudioScript := preload("res://audio/ui_audio.gd")
const AudioMixScript := preload("res://audio/audio_mix.gd")
const AudioSettingsScript := preload("res://ui/components/audio_settings.gd")
const EngineAudioScript := preload("res://audio/engine_audio.gd")
const RadioAnnouncerScript := preload("res://audio/radio/radio_announcer.gd")
const RadioFeedScript := preload("res://ui/components/radio_feed.gd")
const StrategicCommandPanelScript := preload("res://ui/components/strategic_command_panel.gd")
const CommandPostModel := preload("res://structures/command_post_model.gd")
const PowerGridViewScript := preload("res://power_grid_view.gd")
const CommandPostPanelScript := preload("res://ui/components/command_post_panel.gd")
const MagnifierViewScript := preload("res://ui/components/magnifier_view.gd")
const ArmorTexture := preload("res://assets/units/near_future_armor_tile_v1.png")
const MATERIAL_SITES := [
	{"id": 900, "name": "RARE METALS", "position": Vector2(-10500, -3500), "owner": -1, "color": Color("#ffbd52")},
	{"id": 901, "name": "SILICON", "position": Vector2(-15500, 4000), "owner": -1, "color": Color("#47d9ff")},
	{"id": 902, "name": "POLYMER FEEDSTOCK", "position": Vector2(-17500, -8500), "owner": 1, "color": Color("#ce82ff")},
	{"id": 903, "name": "COMPOSITES", "position": Vector2(-12500, -9500), "owner": 1, "color": Color("#7ad99b")},
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
@onready var ui_root: Control = $HUD/UIRoot
@onready var debug_label: Label = $HUD/UIRoot/DebugPanel/DebugLabel
@onready var selection_rect: ColorRect = $HUD/UIRoot/SelectionRect
@onready var debug_panel: ColorRect = $HUD/UIRoot/DebugPanel
@onready var help_label: Label = $HUD/UIRoot/HelpLabel
@onready var command_hud: Control = $HUD/UIRoot/CommandHUD
@onready var strategic_icon_overlay: Control = $HUD/UIRoot/StrategicIconOverlay
@onready var hud_layer: CanvasLayer = $HUD
@onready var startup_overlay: ColorRect = $HUD/UIRoot/StartupOverlay
@onready var scenario_title: Label = %ScenarioTitle
@onready var scenario_factions: Label = %ScenarioFactions
@onready var scenario_description: Label = %ScenarioDescription
@onready var scenario_objective: Label = %ScenarioObjective
@onready var scenario_status: Label = %ScenarioStatus
@onready var start_button: Button = %StartButton
@onready var native_skirmish_button: Button = %NativeSkirmishButton
@onready var territory_debug_label: Label = $HUD/UIRoot/TerritoryDebugLabel
@onready var territory_material: ShaderMaterial = $Ocean.material_override
@onready var battlefield_environment: WorldEnvironment = $WorldEnvironment
@onready var battlefield_light: DirectionalLight3D = $DirectionalLight3D
@onready var moon_light: DirectionalLight3D = $MoonLight3D
@onready var hotkey_manager: Node = get_node_or_null("HotkeyManager")
@onready var hotkey_display: Control = get_node_or_null("HUD/UIRoot/HotkeyDisplay") as Control

var extension: Object
var order_overlay_renderer: Node3D
var unit_route_renderer: Node3D
var combat_effects: Node3D
var ui_audio: Node
var engine_audio: Node3D
var audio_settings: Control
var radio_announcer: Node
var radio_feed: Control
var magnifier_view: Control
var range_envelope_renderer: Node3D
var unit_route_refresh_seconds := 0.0
var unit_multimesh: MultiMesh
var demo_unit_views: Dictionary = {}
var demo_mode := false
var terrain_heights := PackedFloat32Array()
var terrain_mesh_arrays: Array = []
var terrain_revision_seen := 0
var terrain_vegetation: TerrainVegetation
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
var aircraft_heights: Dictionary = {}
var approach_poses: Dictionary = {}
var approach_from: Dictionary = {}
# Frame interpolation between simulation ticks: each unit's pose at the
# previous and the latest tick seen, as Vector3(x, z, heading).
var interpolation_tick := -1
var interpolation_from: Dictionary = {}
var simulated_poses: Dictionary = {}
var commander_ids: Dictionary = {}
var engineer_ids: Dictionary = {}
var hidden_base_ids: Dictionary = {}
var pending_player_build_types: Array[int] = []
var pending_structure_type := -1
var pending_structure_position := Vector2.ZERO
var completed_structure_views: Array[Node3D] = []
var build_mode_type := -1
# Metres beyond a structure footprint where the engineer stands to build it.
const BUILD_STANDOFF_MARGIN := 10.0
var build_ghost: Node3D
var build_ghost_material: ShaderMaterial
var road_build_mode := false
var road_start := Vector2.INF
var road_preview: MeshInstance3D
var road_preview_material: StandardMaterial3D
var road_views: Dictionary = {}
var transport_corridor_views: Dictionary = {}
var ground_transport_views: Dictionary = {}
var blueprint_pointer_down := false
var pending_completed_structures: Array[Dictionary] = []
var selected_structure_view: Node3D
var command_feedback_marker: MeshInstance3D
var command_feedback_pending_ids := PackedInt32Array()
var command_feedback_waiting_for_target := false
var civilian_building_instance_count := 0
var civilian_building_positions: Array[Vector2] = []
var tree_lod_distance := 5000.0
var tactical_tree_views: Array[MultiMeshInstance3D] = []
var strategic_tree_views: Array[MultiMeshInstance3D] = []
var forest_cluster_centers: Array[Vector2] = []
var forest_tree_positions: Array[Vector2] = []
var forest_tree_scales: Array[Vector2] = []
var forest_tree_variant_indices: Array[int] = []
var forest_tree_root_depths: Array[float] = []
var pending_build_order: Dictionary = {}
## Shift+click placements that follow pending_build_order, oldest first:
## {"type", "target"}. Each is sent to the engineer when the one before it is
## handed to the native queue; queue_faction_structure still decides each one.
var queued_build_orders: Array[Dictionary] = []
var queued_build_markers: Node3D
var material_site_state: Dictionary = {}
var fuel_depot_state: Dictionary = {}
var power_grid_view: Node3D
var fuel_supply_status := ""
var fuel_supply_reason_seen := ""
var fuel_supply_status_expires_msec := 0
var assault_enabled := true
var match_finished := false
var match_record_path := ""
var ai_style_state: Dictionary = {}
var match_end_panel: Control
var command_post_views: Dictionary = {}
# The player's command post is selected on its own: it is not in selected_ids,
# so no move, attack or stop order can ever reach it.
var command_post_selected := false
var command_post_panel: Control
# Why the last order to the selected post was refused, shown in its panel.
var command_post_notice := ""
var fog_view: MeshInstance3D
var fog_texture: ImageTexture
var vision_revision_seen := -1
var seen_enemy_ids: Dictionary = {}
var removed_entity_ids: Dictionary = {}
# Flat x, y, seconds since seen, unit type for each enemy now out of sight.
var last_known_contacts := PackedFloat32Array()
var radar_contacts := PackedFloat32Array()
var recon_targeting := false
# The unit type the next map click drops by parachute, or -1.
var paradrop_targeting_type := -1
var paradrop_markers := {}
var paradrops_lost_seen := 0
var enemy_post_discovered := false
# Enemy airfields the player has seen, keyed by rounded position. They stay
# drawn once found, like the enemy command post.
var enemy_airfield_views := {}
var material_order_mode := 0 # 0 normal, 1 claim/capture, 2 demolish
var fob_build_mode := false
var fob_build_target := Vector2.INF
var fob_was_constructing := false
var fob_completion_notification := ""
var hover_context: Dictionary = {}
var control_point_presentation: Node3D
var control_point_battle_enabled := false
var control_point_battle_status := ""
var reinforcement_presentation: Node3D
var reinforcement_delivery_enabled := false
var reinforcement_delivery_selecting := false
var reinforcement_delivery_status := ""
var reinforcement_delivery_waiting_for_airfield := false
var strategic_command_view: Control
var reinforcement_transport_has_landed := false
# Seconds of flight every delivery spends in the air (Simulation::kTransitSeconds).
const Simulation_TRANSIT_MS := 20000.0

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
# Tuned in the F8 panel on a clear day. 0.72 stays the reference level the
# light and terrain brightness scales are normalised against.
const MAP_BRIGHTNESS_DEFAULT := 0.93
var environment_debug_brightness := MAP_BRIGHTNESS_DEFAULT
var environment_debug_fog := 0.000009
var environment_debug_exposure := 1.08
# Fog of war darkness over unexplored ground (fog_of_war.gdshader fog_strength).
const FOG_OF_WAR_DEFAULT_STRENGTH := 0.33
var fog_of_war_strength := FOG_OF_WAR_DEFAULT_STRENGTH
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
var exit_confirm: ExitConfirm
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
	extension.call("economy_add_tactical_storage", 1, 0.0, 0.0,
		SKIRMISH_PRESENTATION_STARTING_MATERIALS, SKIRMISH_PRESENTATION_STARTING_FUEL, 0.0, 0.0)
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
	AudioMixScript.new().setup()
	_setup_audio_settings()
	engine_audio = EngineAudioScript.new()
	engine_audio.name = "EngineAudio"
	add_child(engine_audio)
	ui_audio = UiAudioScript.new()
	ui_audio.name = "UiAudio"
	add_child(ui_audio)
	_setup_radio()
	_setup_command_post_panel()
	_setup_strategic_command_view()
	_setup_magnifier_view()
	combat_effects = CombatEffectsScript.new()
	combat_effects.name = "CombatEffects"
	combat_effects.viewer = HUMAN_PLAYER_ID
	combat_effects.height_at = _terrain_height_at
	combat_effects.position_visible = func(event: Dictionary) -> bool:
		return bool(extension.call("can_see", HUMAN_PLAYER_ID, float(event.x), float(event.y), bool(event.airborne)))
	combat_effects.listener = camera
	add_child(combat_effects)
	# Territory data is still a development diagnostic; the centered table was
	# competing with the tactical view. Keep the update path available for a
	# future operational-theater panel, but never show it over the battlefield.
	territory_debug_label.visible = false
	_hotkey_setup_from_manager()
	var content_data_root := ""
	if OS.has_feature("standalone"):
		content_data_root = OS.get_executable_path().get_base_dir().path_join("data")
	else:
		content_data_root = ProjectSettings.globalize_path("res://../../data").simplify_path()
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
	_order_overlay_renderer_setup()
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
	native_skirmish_button.pressed.connect(_on_native_skirmish_pressed)
	command_hud.build_requested.connect(_begin_build_placement)

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
	# Sentence case for prose: easier to read than all-caps paragraphs.
	scenario_description.text = String(scenario_definition.description)
	scenario_factions.text = "ELITE PRECISION  VS  MASS WARFARE"
	scenario_objective.text = String(scenario_definition.victory.description).to_upper()
	scenario_status.text = "•  Build an airfield with your Field Engineer, then order forces in Strategic Command (G)."
	scenario_status.text += "\n•  Scout before advancing. The enemy's position is unknown and terrain blocks sight."
	scenario_status.text += "\n•  Ship fuel in or capture depots to keep vehicles moving."
	start_button.grab_focus.call_deferred()
	if OS.get_environment("RTS_CONTROL_POINT_SKIRMISH") == "1":
		_start_control_point_skirmish.call_deferred()
	elif OS.get_environment("RTS_REINFORCEMENT_DELIVERY_SMOKE") == "1":
		_start_reinforcement_delivery_smoke.call_deferred()
	elif OS.get_environment("RTS_AUTO_START_SKIRMISH") == "1":
		_on_start_skirmish_pressed.call_deferred()
	if OS.get_environment("RTS_AUTO_START_NATIVE_SKIRMISH") == "1":
		_on_native_skirmish_pressed.call_deferred()


func _on_native_skirmish_pressed() -> void:
	get_tree().change_scene_to_file("res://native_skirmish.tscn")


func _start_control_point_skirmish() -> void:
	assault_enabled = false
	if match_started:
		return
	_on_start_skirmish_pressed()
	if not match_started:
		return
	var point := Vector2(-12100.0, 0.0)
	var point_radius := 55.0
	var packed_points := PackedFloat32Array([point.x, point.y, point_radius])
	control_point_battle_enabled = bool(extension.call("control_point_battle_begin", packed_points, HUMAN_PLAYER_ID, 1, 3000.0))
	if not control_point_battle_enabled:
		push_error("Failed to begin native control-point battle")
		return
	for row in range(3):
		for column in range(4):
			var spawn := point + Vector2(-150.0 + float(column) * 10.0, -12.0 + float(row) * 12.0)
			var vehicle_id := int(extension.call("create_unit_with_type", spawn.x, spawn.y, 6, HUMAN_PLAYER_ID))
			if vehicle_id > 0:
				_register_presented_unit(vehicle_id, 6, HUMAN_PLAYER_ID, spawn)
	for index in range(1):
		var defender_spawn := point + Vector2(-15.0 + float(index) * 10.0, 12.0)
		var defender_id := int(extension.call("create_unit_with_type", defender_spawn.x, defender_spawn.y, 8, 1))
		if defender_id > 0:
			_register_presented_unit(defender_id, 8, 1, defender_spawn)
	if control_point_presentation == null:
		control_point_presentation = ControlPointPresentationScript.new()
		add_child(control_point_presentation)
	control_point_battle_status = "CONTROL POINT ALPHA // NEUTRAL // MOVE VEHICLES INTO THE ZONE"
	camera_target = Vector3(point.x - 80.0, _terrain_height_at(point.x - 80.0, point.y), point.y)
	camera_distance = 420.0
	target_camera_distance = 420.0
	_update_camera(1.0)
	_update_control_point_presentation()


func _start_reinforcement_delivery_smoke() -> void:
	assault_enabled = false
	if match_started:
		return
	_on_start_skirmish_pressed()
	if not match_started:
		return
	extension.call("reinforcement_delivery_set_resources", HUMAN_PLAYER_ID, 600.0, 400.0)
	reinforcement_delivery_waiting_for_airfield = true
	_activate_reinforcement_delivery_from_airfield()


func _activate_reinforcement_delivery_from_airfield() -> void:
	if reinforcement_delivery_enabled or not reinforcement_delivery_waiting_for_airfield or extension == null:
		return
	if bool(extension.call("reinforcement_delivery_configure", 0.0, 0.0, 1200.0, HUMAN_PLAYER_ID)):
		reinforcement_delivery_enabled = true
		reinforcement_delivery_waiting_for_airfield = false
		reinforcement_presentation = ReinforcementPresentationScript.new()
		add_child(reinforcement_presentation)
		reinforcement_delivery_status = "AIRFIELD ONLINE // DELIVERY ZONE READY // G: STRATEGIC COMMAND"
		var delivery: Dictionary = extension.call("reinforcement_delivery_state")
		var zone := Vector2(float(delivery.get("x", 0.0)), float(delivery.get("y", 0.0)))
		camera_target = Vector3(zone.x - 80.0, _terrain_height_at(zone.x - 80.0, zone.y), zone.y)
		camera_distance = 420.0
		target_camera_distance = 420.0
		_update_camera(1.0)
		_update_reinforcement_presentation()
	else:
		reinforcement_delivery_status = String(extension.call("reinforcement_delivery_state").get("reason", "AIRFIELD REQUIRED FOR REINFORCEMENTS"))


## The command network: native reports in, one spoken line and its text out.
func _setup_radio() -> void:
	radio_announcer = RadioAnnouncerScript.new()
	radio_announcer.name = "RadioAnnouncer"
	radio_announcer.listener_faction = HUMAN_PLAYER_ID
	radio_announcer.prefs.load_saved()
	add_child(radio_announcer)
	radio_feed = RadioFeedScript.new()
	radio_feed.name = "RadioFeed"
	ui_root.add_child(radio_feed)
	radio_feed.attach(radio_announcer)
	radio_feed.focus_requested.connect(_focus_camera_on_report)
	if audio_settings != null:
		audio_settings.attach_radio(radio_announcer)


func _focus_camera_on_report(spot: Vector2) -> void:
	camera_target = Vector3(spot.x, _terrain_height_at(spot.x, spot.y), spot.y)


func _setup_audio_settings() -> void:
	# A separate layer keeps the panel above startup and after-action overlays.
	var layer := CanvasLayer.new()
	layer.layer = 20
	add_child(layer)
	audio_settings = AudioSettingsScript.new()
	audio_settings.name = "AudioSettings"
	layer.add_child(audio_settings)
	var actions := startup_overlay.get_node("Panel/VBox/Footer/FooterRow") if startup_overlay.has_node("Panel/VBox/Footer/FooterRow") else null
	if actions != null:
		var button := preload("res://ui/components/mandate_button.gd").new()
		button.text = "AUDIO & CREDITS"
		button.tooltip_text = "Audio levels and asset credits  [F2]"
		button.variant = "ghost"
		button.pressed.connect(audio_settings.toggle)
		actions.add_child(button)
		actions.move_child(button, 1)
		actions.get_node("ControlsHint").text = "F1 CONTROLS"


func _setup_magnifier_view() -> void:
	magnifier_view = MagnifierViewScript.new()
	ui_root.add_child(magnifier_view)
	magnifier_view.set_enabled(false)


## Where the magnifier looks: the ground under the cursor, or null when the
## cursor is over a HUD control or off the map (the inset then holds its view).
func _magnifier_focus() -> Variant:
	var pointer := get_viewport().get_mouse_position()
	if not get_viewport().get_visible_rect().has_point(pointer):
		return null
	var hovered := get_viewport().gui_get_hovered_control()
	if hovered != null and hovered != command_hud and hovered != ui_root:
		return null
	var ground := _screen_to_world(pointer)
	if absf(ground.x) > terrain_world_width * 0.5 or absf(ground.y) > terrain_world_height * 0.5:
		return null
	return ground


func _update_magnifier() -> void:
	if magnifier_view == null or not magnifier_view.is_enabled():
		return
	var focus: Variant = _magnifier_focus()
	# Before the first battlefield hover, show the starting camera focus rather
	# than an uninitialised camera at the world origin.
	if focus == null and not magnifier_view.has_target():
		focus = Vector2(camera_target.x, camera_target.z)
	var height := 0.0
	if focus != null:
		height = _terrain_height_at((focus as Vector2).x, (focus as Vector2).y)
	magnifier_view.update_view(camera, camera_distance, focus, height)


func _setup_strategic_command_view() -> void:
	strategic_command_view = StrategicCommandPanelScript.new()
	strategic_command_view.name = "StrategicCommand"
	strategic_command_view.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	strategic_command_view.visible = false
	strategic_command_view.order_unit.connect(_on_strategic_command_order_unit)
	strategic_command_view.request_fuel.connect(_request_fuel_shipment)
	strategic_command_view.request_recon.connect(_begin_recon_targeting)
	strategic_command_view.order_paradrop.connect(_begin_paradrop_targeting)
	strategic_command_view.dismissed.connect(_close_strategic_command)
	ui_root.add_child(strategic_command_view)


func _toggle_strategic_command() -> void:
	if strategic_command_view == null:
		return
	if strategic_command_view.visible:
		_close_strategic_command()
		return
	if not match_started or extension == null:
		return
	strategic_command_view.visible = true
	# Modal: above the magnifier and every HUD card.
	strategic_command_view.move_to_front()
	ui_audio.play_cue("panel_open")
	_sync_strategic_command()


func _close_strategic_command() -> void:
	if strategic_command_view != null and strategic_command_view.visible:
		strategic_command_view.visible = false
		ui_audio.play_cue("panel_close")


func _on_strategic_command_select_zone() -> void:
	if not reinforcement_delivery_enabled:
		return
	reinforcement_delivery_selecting = true
	reinforcement_delivery_status = "SELECT A FRIENDLY AIRFIELD // LEFT-CLICK ITS STRUCTURE"
	_close_strategic_command()
	_update_hud()


func _on_strategic_command_request_package() -> void:
	if not reinforcement_delivery_enabled:
		return
	if bool(extension.call("reinforcement_delivery_request", HUMAN_PLAYER_ID)):
		reinforcement_delivery_status = "TRANSPORT INBOUND"
	else:
		reinforcement_delivery_status = String(extension.call("reinforcement_delivery_state").get("reason", "REQUEST REJECTED"))
	_sync_strategic_command()
	_update_hud()


func _sync_strategic_command() -> void:
	if strategic_command_view == null or not strategic_command_view.visible or extension == null:
		return
	var delivery: Dictionary = extension.call("delivery_state", HUMAN_PLAYER_ID)
	var status := reinforcement_delivery_status
	if status.is_empty():
		status = "FORCES FLY IN TO YOUR AIRFIELD" if bool(delivery.get("has_site", false)) else "BUILD AN AIRFIELD TO RECEIVE FORCES"
	strategic_command_view.set_state(extension.call("get_build_catalog", HUMAN_PLAYER_ID), delivery.get("inbound", []), _with_fuel_status(status))


# Forces are ordered here, not by the engineer. The unit is paid for now and
# lands at the airfield when its build and flight time are up.
func _on_strategic_command_order_unit(unit_type: int, mode: int = 0) -> void:
	var accepted := _order_unit_by_transport(unit_type) if mode == 0 else bool(extension.call("order_ground_transport", HUMAN_PLAYER_ID, unit_type, mode))
	ui_audio.play_cue("order_accepted" if accepted else "order_refused")
	var refusal := ""
	if not accepted:
		for entry in extension.call("get_build_catalog", HUMAN_PLAYER_ID):
			if int(entry.get("type", -1)) == unit_type:
				var reason_key := "reason" if mode == 0 else "road_reason" if mode == 1 else "rail_reason"
				refusal = String(entry.get(reason_key, ""))
		reinforcement_delivery_status = refusal if not refusal.is_empty() else "ORDER REFUSED"
	else:
		reinforcement_delivery_status = "FORCES ORDERED // %s INBOUND" % ["AIR", "ROAD", "RAIL"][clampi(mode, 0, 2)]
	_sync_strategic_command()
	_update_hud()


# The native call answers at once, so the panel can say whether the order was
# taken. A ground unit is paid for and sent; an aircraft joins the airfield queue.
func _order_unit_by_transport(unit_type: int) -> bool:
	if int(commander_ids.get(HUMAN_PLAYER_ID, -1)) < 0:
		return false
	return bool(extension.call("queue_faction_unit", HUMAN_PLAYER_ID, unit_type, 0.0, 0.0))


func _with_fuel_status(status: String) -> String:
	if fuel_supply_status.is_empty():
		return status
	return fuel_supply_status if status.is_empty() else "%s // %s" % [status, fuel_supply_status]


# The next map click sets the flight plan. The plane uses no airfield: it comes in
# from off the map, flies straight there, and leaves when its time on station ends.
func _begin_recon_targeting() -> void:
	_cancel_tactical_modes()
	_close_strategic_command()
	recon_targeting = true
	_announce_intel("SELECT RECON TARGET")


func _order_recon_flight(target: Vector2) -> bool:
	recon_targeting = false
	var before: int = (extension.call("get_hud_state", HUMAN_PLAYER_ID, int(commander_ids.get(HUMAN_PLAYER_ID, -1)), -1, 0.0, 0.0, false).get("production_queue", []) as Array).size()
	_order_commander_unit(RECON_PLANE_UNIT_TYPE, target)
	extension.call("update_simulation", 50.0)
	var queued: int = (extension.call("get_hud_state", HUMAN_PLAYER_ID, int(commander_ids.get(HUMAN_PLAYER_ID, -1)), -1, 0.0, 0.0, false).get("production_queue", []) as Array).size()
	var accepted := queued > before
	_announce_intel("RECON FLIGHT INBOUND" if accepted else "RECON FLIGHT REFUSED")
	_sync_strategic_command()
	_update_hud()
	return accepted


# The next map click picks the drop zone. Native rules decide: the ground must
# be watched right now, and the unit is paid for and counts against the cap.
func _begin_paradrop_targeting(unit_type: int) -> void:
	_cancel_tactical_modes()
	_close_strategic_command()
	paradrop_targeting_type = unit_type
	_announce_intel("SELECT DROP ZONE // GROUND YOU CAN SEE")


func _order_paradrop(target: Vector2) -> bool:
	var unit_type := paradrop_targeting_type
	paradrop_targeting_type = -1
	var refusal := String(extension.call("order_paradrop", HUMAN_PLAYER_ID, unit_type, target.x, target.y))
	_announce_intel("PARADROP INBOUND" if refusal.is_empty() else "PARADROP REFUSED // " + refusal)
	_sync_strategic_command()
	_update_hud()
	return refusal.is_empty()


# A marker over each drop zone with a paratroop drop still in the air.
func _sync_paradrop_markers() -> void:
	var active := {}
	var delivery: Dictionary = extension.call("delivery_state", HUMAN_PLAYER_ID)
	# A drop's transport flies in over the map and can be shot down.
	var lost := int(delivery.get("airdrops_lost", 0))
	if lost > paradrops_lost_seen:
		_announce_intel("PARADROP TRANSPORT SHOT DOWN")
	paradrops_lost_seen = lost
	for force in delivery.get("inbound", []):
		if not bool(force.get("airdrop", false)):
			continue
		var point := Vector2(float(force.x), float(force.y))
		var key := Vector2i(roundi(point.x), roundi(point.y))
		var seconds := float(force.remaining_ms) / 1000.0
		active[key] = minf(float(active.get(key, INF)), seconds)
	for key in paradrop_markers.keys():
		if not active.has(key):
			var stale: Node3D = paradrop_markers[key]
			if is_instance_valid(stale):
				stale.queue_free()
			paradrop_markers.erase(key)
	for key in active.keys():
		var marker: Node3D = paradrop_markers.get(key)
		if marker == null:
			marker = Node3D.new()
			marker.name = "DropZone_%d_%d" % [key.x, key.y]
			marker.position = Vector3(key.x, _terrain_height_at(key.x, key.y) + 2.0, key.y)
			var ring := MeshInstance3D.new()
			var torus := TorusMesh.new()
			torus.inner_radius = 140.0
			torus.outer_radius = 150.0
			ring.mesh = torus
			ring.material_override = _demo_material(Color("#7fd4ff"), 1.2)
			marker.add_child(ring)
			var label := Label3D.new()
			label.name = "Countdown"
			label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
			label.pixel_size = 0.6
			label.font_size = 48
			label.outline_size = 10
			label.position.y = 40.0
			marker.add_child(label)
			add_child(marker)
			paradrop_markers[key] = marker
		(marker.get_node("Countdown") as Label3D).text = "PARADROP  %.0f S" % float(active[key])


func _request_fuel_shipment() -> bool:
	var accepted := bool(extension.call("fuel_shipment_request", HUMAN_PLAYER_ID))
	ui_audio.play_cue("order_accepted" if accepted else "order_refused")
	_sync_fuel_supply()
	_sync_strategic_command()
	_update_hud()
	return accepted


func _sync_fuel_supply() -> void:
	var supply: Dictionary = extension.call("fuel_supply_state", HUMAN_PLAYER_ID)
	var owners_changed := false
	for depot in supply.get("depots", []):
		var site: Dictionary = fuel_depot_state.get(int(depot.id), {})
		if site.is_empty():
			continue
		owners_changed = owners_changed or int(site.owner) != int(depot.owner)
		site.owner = int(depot.owner)
		site.stock = float(depot.stock)
		site.capturing = bool(depot.capturing)
	for status in extension.call("material_sites_state", HUMAN_PLAYER_ID):
		var material_site: Dictionary = material_site_state.get(int(status.id), {})
		if material_site.is_empty() or bool(material_site.get("destroyed", false)):
			continue
		owners_changed = owners_changed or int(material_site.owner) != int(status.owner)
		material_site.owner = int(status.owner)
		material_site.capturing = bool(status.capturing)
	if owners_changed:
		_rebuild_material_site_views()
	var reason := String(supply.get("reason", ""))
	if reason != fuel_supply_reason_seen:
		fuel_supply_reason_seen = reason
		fuel_supply_status = reason
		fuel_supply_status_expires_msec = Time.get_ticks_msec() + int(FUEL_STATUS_SECONDS * 1000.0)
	var inbound := supply.has("shipment_remaining_ms")
	if inbound:
		fuel_supply_status = "FUEL SHIPMENT INBOUND // %.0f S" % (float(supply.shipment_remaining_ms) / 1000.0)
		fuel_supply_status_expires_msec = Time.get_ticks_msec() + int(FUEL_STATUS_SECONDS * 1000.0)
	elif Time.get_ticks_msec() > fuel_supply_status_expires_msec:
		fuel_supply_status = ""
	# Out of fuel outranks every other supply message: the army cannot move.
	var starved := int(supply.get("starved_units", 0))
	if starved > 0:
		fuel_supply_status = "OUT OF FUEL // %d VEHICLE%s HALTED" % [starved, "" if starved == 1 else "S"]
		fuel_supply_status_expires_msec = Time.get_ticks_msec() + int(FUEL_STATUS_SECONDS * 1000.0)
	# The shipment's cargo aircraft is a real unit now, drawn like any other
	# aircraft, so there is no separate scripted fly-in for it.


func _setup_environment_debug_panel() -> void:
	var environment := battlefield_environment.environment
	if environment == null:
		return
	environment_debug_fog = environment.fog_density
	environment_debug_exposure = environment.tonemap_exposure
	debug_panel.offset_right = 380.0
	debug_panel.offset_bottom = 386.0
	debug_label.visible = false
	var title := Label.new()
	title.text = "ENVIRONMENT DEBUG // F8"
	title.position = Vector2(12, 10)
	title.add_theme_color_override("font_color", Color("#47d9ff"))
	debug_panel.add_child(title)
	_add_environment_slider("brightness", "MAP BRIGHTNESS", 0.25, 1.8, environment_debug_brightness, 42.0)
	_add_environment_slider("fog", "FOG DENSITY", 0.0, 0.00006, environment_debug_fog, 82.0)
	_add_environment_slider("exposure", "EXPOSURE", 0.5, 1.8, environment_debug_exposure, 122.0)
	_add_environment_slider("war_fog", "FOG OF WAR", 0.0, 1.0, fog_of_war_strength, 162.0)
	var weather_label := Label.new()
	weather_label.text = "WEATHER"
	weather_label.position = Vector2(12, 204)
	weather_label.size = Vector2(132, 24)
	debug_panel.add_child(weather_label)
	environment_debug_weather_option = OptionButton.new()
	environment_debug_weather_option.position = Vector2(138, 202)
	environment_debug_weather_option.size = Vector2(190, 28)
	for weather_name in ["CLEAR", "OVERCAST", "STORM"]:
		environment_debug_weather_option.add_item(weather_name)
	environment_debug_weather_option.item_selected.connect(func(index: int): _apply_environment_weather(environment_debug_weather_option.get_item_text(index)))
	debug_panel.add_child(environment_debug_weather_option)
	environment_debug_weather_option.select(1)
	var time_label := Label.new()
	time_label.text = "TIME OF DAY"
	time_label.position = Vector2(12, 244)
	time_label.size = Vector2(132, 24)
	debug_panel.add_child(time_label)
	environment_debug_day_button = Button.new()
	environment_debug_day_button.text = "DAYTIME"
	environment_debug_day_button.position = Vector2(138, 242)
	environment_debug_day_button.size = Vector2(92, 28)
	environment_debug_day_button.pressed.connect(func(): _apply_time_of_day("DAYTIME"))
	debug_panel.add_child(environment_debug_day_button)
	environment_debug_night_button = Button.new()
	environment_debug_night_button.text = "NIGHTTIME"
	environment_debug_night_button.position = Vector2(236, 242)
	environment_debug_night_button.size = Vector2(92, 28)
	environment_debug_night_button.pressed.connect(func(): _apply_time_of_day("NIGHTTIME"))
	debug_panel.add_child(environment_debug_night_button)
	environment_debug_steering_label = Label.new()
	environment_debug_steering_label.position = Vector2(12, 284)
	environment_debug_steering_label.size = Vector2(356, 28)
	environment_debug_steering_label.autowrap_mode = TextServer.AUTOWRAP_OFF
	environment_debug_steering_label.add_theme_color_override("font_color", Color("#b6c7cf"))
	debug_panel.add_child(environment_debug_steering_label)
	var reset := Button.new()
	reset.text = "RESET DEFAULTS"
	reset.position = Vector2(208, 324)
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
		"war_fog":
			fog_of_war_strength = value
			_apply_fog_of_war_strength()
			(environment_debug_value_labels.get(key) as Label).text = "%.2f" % value
		"exposure":
			environment_debug_exposure = value
			battlefield_environment.environment.tonemap_exposure = value
			(environment_debug_value_labels.get(key) as Label).text = "%.2f" % value


func _apply_fog_of_war_strength() -> void:
	if fog_view == null:
		return
	var material := fog_view.material_override as ShaderMaterial
	if material != null:
		material.set_shader_parameter("fog_strength", fog_of_war_strength)


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
			environment.fog_density = 0.000009
			environment.fog_light_color = Color("#9ab0bd")
			environment.fog_light_energy = 0.65
			sky_material.sky_top_color = Color("#1a3a5c")
			sky_material.sky_horizon_color = Color("#8ea5b3")
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
	for key in ["brightness", "fog", "exposure", "war_fog"]:
		var slider := environment_debug_sliders.get(key) as HSlider
		if slider != null:
			slider.value = MAP_BRIGHTNESS_DEFAULT if key == "brightness" else 0.000009 if key == "fog" else 1.08 if key == "exposure" else FOG_OF_WAR_DEFAULT_STRENGTH
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
			terrain_mesh_arrays = HeightMap.build_terrain_arrays(heights, terrain_world_width, terrain_world_height)
			_apply_terrain_mesh()
			terrain_view.visible = true
			for landmass in landmass_nodes:
				landmass.visible = false
			_rebuild_terrain_trees()
			_apply_terrain_masks()


func _start_scale_profile() -> void:
	extension.call("start_simulation")
	match_started = true
	if combat_effects != null:
		combat_effects.clear()
	if engine_audio != null:
		engine_audio.clear()
	if radio_announcer != null:
		radio_announcer.clear()
	if magnifier_view != null:
		magnifier_view.set_enabled(true)
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
	terrain_revision_seen = int(extension.call("get_terrain_revision"))
	extension.call("start_simulation")
	match_started = true
	if combat_effects != null:
		combat_effects.clear()
	if engine_audio != null:
		engine_audio.clear()
	if radio_announcer != null:
		radio_announcer.clear()
	if magnifier_view != null:
		magnifier_view.set_enabled(true)
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
	recon_targeting = false
	paradrop_targeting_type = -1
	paradrops_lost_seen = 0
	for marker in paradrop_markers.values():
		if is_instance_valid(marker):
			marker.queue_free()
	paradrop_markers.clear()
	removed_entity_ids.clear()
	last_known_contacts = PackedFloat32Array()
	radar_contacts = PackedFloat32Array()
	material_site_state.clear()
	fuel_depot_state.clear()
	fuel_supply_status = ""
	fuel_supply_reason_seen = ""
	match_finished = false
	command_post_views.clear()
	command_post_selected = false
	seen_enemy_ids.clear()
	vision_revision_seen = -1
	enemy_post_discovered = false
	for airfield in enemy_airfield_views.values():
		if is_instance_valid(airfield):
			airfield.queue_free()
	enemy_airfield_views.clear()
	material_order_mode = 0
	if not _spawn_commander_start():
		push_error("Failed to create the authoritative engineer-start forces")
		extension.call("stop_simulation")
		match_started = false
		return
	# Every regular presentation skirmish waits for its first friendly airfield.
	# Once native construction registers it, the existing delivery route opens.
	reinforcement_delivery_waiting_for_airfield = true
	reinforcement_delivery_enabled = false
	reinforcement_delivery_selecting = false
	reinforcement_delivery_status = "AIRFIELD REQUIRED FOR STRATEGIC COMMAND"
	# This presentation skirmish is a build-test sandbox, not an economy slice.
	# Resource generation and capture remain authoritative native systems.
	extension.call("reinforcement_delivery_set_resources", HUMAN_PLAYER_ID,
		SKIRMISH_PRESENTATION_STARTING_MATERIALS, SKIRMISH_PRESENTATION_STARTING_FUEL)
	# The AI starts with exactly what the player starts with: no bonus, no handicap.
	extension.call("reinforcement_delivery_set_resources", int(scenario_definition.ai.faction_id),
		SKIRMISH_PRESENTATION_STARTING_MATERIALS, SKIRMISH_PRESENTATION_STARTING_FUEL)
	_spawn_material_sites()
	_spawn_fuel_depots()
	_spawn_power_grid()
	_spawn_civilian_buildings()
	_begin_battle()
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
	_update_command_feedback(delta)
	# Middle-button release can be missed if focus changes while the pointer is
	# outside the window. Never leave tactical panning latched in that case.
	if panning and not Input.is_mouse_button_pressed(MOUSE_BUTTON_MIDDLE):
		panning = false
	if road_build_mode:
		var road_target := _screen_to_world(get_viewport().get_mouse_position())
		_update_road_preview(road_target)
	_update_unit_routes(delta)
	if extension == null or not match_started or match_finished:
		engine_audio.clear()
		return

	var simulation_start_us := Time.get_ticks_usec()
	_enforce_game_speed_policy()
	extension.call("update_simulation", delta * 1000.0)
	_update_match_state()
	_sync_vision()
	_process_pending_build_order()
	var simulation_call_ms := float(Time.get_ticks_usec() - simulation_start_us) / 1000.0
	combat_effects.consume(extension.call("get_combat_events", combat_effects.cursor, HUMAN_PLAYER_ID))
	radio_announcer.map_rect = Rect2(-terrain_world_width * 0.5, -terrain_world_height * 0.5, terrain_world_width, terrain_world_height)
	radio_announcer.consume(extension.call("get_radio_events", radio_announcer.cursor, HUMAN_PLAYER_ID))
	_sync_new_entities()
	_sync_completed_roads()
	_update_keyboard_pan(delta)
	_update_camera(delta)
	_update_build_ghost()
	if order_overlay_renderer != null:
		order_overlay_renderer.set_camera_distance(camera_distance)
	_sync_tree_lod()
	var sync_timings := _sync_unit_transforms()
	_sync_ground_transport_presentation()
	engine_audio.update_from_view(self, delta)
	_update_range_envelopes()
	_record_profile_frame(delta, simulation_call_ms, sync_timings.x, sync_timings.y)
	_update_hover_context()
	_update_magnifier()

	hud_elapsed += delta
	if hud_elapsed >= 0.2:
		hud_elapsed = 0.0
		_update_hud()

	_update_territory()
	_update_control_point_presentation()
	_update_reinforcement_presentation()


func _update_control_point_presentation() -> void:
	if not control_point_battle_enabled or extension == null:
		return
	var battle: Dictionary = extension.call("control_point_battle_state")
	var points: Array = battle.get("points", [])
	if control_point_presentation != null:
		control_point_presentation.sync(points, func(x: float, z: float): return _terrain_height_at(x, z))
	if points.is_empty():
		return
	var point: Dictionary = points[0]
	var result := int(battle.get("result", 0))
	var state_name: String = ["NEUTRAL", "FRIENDLY", "ENEMY", "CONTESTED"][clampi(int(point.get("state", 0)), 0, 3)]
	if result == 2:
		control_point_battle_status = "VICTORY // ALPHA SECURED"
	elif result == 3:
		control_point_battle_status = "DEFEAT // ALPHA LOST"
	else:
		control_point_battle_status = "ALPHA // %s // CAPTURE %.0f%% // HOLD %.1f / %.1f SEC" % [state_name,
			absf(float(point.get("capture_progress", 0.0))) * 100.0,
			float(battle.get("hold_elapsed_ms", 0.0)) / 1000.0,
			float(battle.get("hold_duration_ms", 0.0)) / 1000.0]


func _update_reinforcement_presentation() -> void:
	if not reinforcement_delivery_enabled or extension == null:
		return
	var delivery: Dictionary = extension.call("reinforcement_delivery_state")
	if reinforcement_presentation != null:
		reinforcement_presentation.sync(_transport_flight_snapshot(delivery), func(x: float, z: float): return _terrain_height_at(x, z))
	if not String(delivery.get("reason", "")).is_empty() and String(delivery.get("reason", "")) != "AIRFIELD SELECTED":
		reinforcement_delivery_status = String(delivery.get("reason", reinforcement_delivery_status))


# The F-15C is drawn flying in for the last stretch of the next delivery: its
# approach follows how much of the flight time is left. Between deliveries it
# sits parked on the airfield, as before.
func _transport_flight_snapshot(anchor: Dictionary) -> Dictionary:
	var flight_ms := Simulation_TRANSIT_MS
	var inbound: Array = extension.call("delivery_state", HUMAN_PLAYER_ID).get("inbound", [])
	var snapshot := anchor.duplicate()
	snapshot["runway_offset"] = AIRFIELD_PAVING[0].center
	snapshot["duration_ms"] = flight_ms
	if inbound.is_empty():
		snapshot["state"] = 4 if reinforcement_transport_has_landed else int(anchor.get("state", 0))
		return snapshot
	var remaining := float(inbound[0].remaining_ms)
	if remaining > flight_ms:
		snapshot["state"] = 4 if reinforcement_transport_has_landed else 2
		return snapshot
	reinforcement_transport_has_landed = true
	snapshot["state"] = 3
	snapshot["elapsed_ms"] = flight_ms - remaining
	return snapshot


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
	if exit_confirm != null and exit_confirm.visible:
		if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_ESCAPE:
			exit_confirm.hide()
		get_viewport().set_input_as_handled()
		return
	if audio_settings != null and audio_settings.visible:
		get_viewport().set_input_as_handled()
		return
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F2 and not event.ctrl_pressed and not event.meta_pressed and not event.alt_pressed:
		audio_settings.toggle()
		get_viewport().set_input_as_handled()
		return
	if match_started and not match_finished:
		if event is InputEventMagnifyGesture:
			# macOS delivers incremental magnification factors, not wheel clicks.
			# Spreading fingers increases magnification, so camera distance shrinks.
			if is_finite(event.factor) and event.factor > 0.0:
				_zoom_toward_cursor(event.position, 1.0 / event.factor)
			get_viewport().set_input_as_handled()
			return
		if event is InputEventKey and event.pressed and not event.ctrl_pressed and not event.meta_pressed and not event.alt_pressed:
			if event.keycode in [KEY_SLASH, KEY_KP_DIVIDE]:
				if not event.echo:
					_cycle_camera_zoom()
				get_viewport().set_input_as_handled()
				return
			if event.keycode in [KEY_PLUS, KEY_EQUAL, KEY_KP_ADD, KEY_MINUS, KEY_KP_SUBTRACT]:
				if not event.echo:
					_change_game_speed(-1 if event.keycode in [KEY_MINUS, KEY_KP_SUBTRACT] else 1)
				get_viewport().set_input_as_handled()
				return

	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F3 and radio_feed != null:
		radio_feed.toggle_log()
		get_viewport().set_input_as_handled()
		return
	if event is InputEventKey and event.pressed and not event.echo and event.keycode == KEY_F1:
		if hotkey_display != null:
			hotkey_display.toggle()
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE and hotkey_display != null and hotkey_display.visible:
		hotkey_display.set_sheet_visible(false)
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_G:
		_toggle_strategic_command()
		return
	if event is InputEventKey and event.pressed and not event.echo and magnifier_view != null:
		if event.keycode == KEY_M:
			magnifier_view.toggle()
			return
		if event.keycode == KEY_BRACKETRIGHT or event.keycode == KEY_BRACKETLEFT:
			magnifier_view.cycle_zoom(1 if event.keycode == KEY_BRACKETRIGHT else -1)
			return
	if event is InputEventKey and event.pressed and event.keycode == KEY_F8:
		debug_panel.visible = not debug_panel.visible
		return
	if event is InputEventKey and event.pressed and event.keycode == KEY_ESCAPE:
		if strategic_command_view != null and strategic_command_view.visible:
			_close_strategic_command()
			return
		if build_mode_type >= 0 or fob_build_mode or material_order_mode != 0 or recon_targeting or paradrop_targeting_type >= 0:
			_cancel_tactical_modes()
			return
		_show_exit_confirm()
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
		if event.keycode == KEY_J:
			_request_fuel_shipment()
			return
		if reinforcement_delivery_enabled and event.keycode == KEY_D:
			reinforcement_delivery_selecting = true
			reinforcement_delivery_status = "SELECT A FRIENDLY AIRFIELD // LEFT-CLICK ITS STRUCTURE"
			return
		if reinforcement_delivery_enabled and event.keycode == KEY_F:
			if bool(extension.call("reinforcement_delivery_request", HUMAN_PLAYER_ID)):
				reinforcement_delivery_status = "TRANSPORT INBOUND"
			else:
				reinforcement_delivery_status = String(extension.call("reinforcement_delivery_state").get("reason", "REQUEST REJECTED"))
			return
		if event.keycode == KEY_8:
			fob_build_mode = selected_ids.size() > 0
			material_order_mode = 0
			_update_hud()
			return
		if event.keycode == KEY_6:
			_begin_build_placement(100)
			return
		if event.keycode == KEY_7:
			_begin_build_placement(101)
			return
		if event.keycode == KEY_T:
			_begin_build_placement(102)
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
				if recon_targeting:
					_order_recon_flight(_screen_to_world(event.position))
					return
				if paradrop_targeting_type >= 0:
					_order_paradrop(_screen_to_world(event.position))
					return
				if reinforcement_delivery_selecting:
					var airfield_target := _screen_to_world(event.position)
					reinforcement_delivery_selecting = false
					if bool(extension.call("reinforcement_delivery_select_zone", HUMAN_PLAYER_ID, airfield_target.x, airfield_target.y)):
						reinforcement_delivery_status = "AIRFIELD SELECTED"
					else:
						reinforcement_delivery_status = String(extension.call("reinforcement_delivery_state").get("reason", "ZONE REJECTED"))
					return
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
					if _resolve_build_target(build_mode_type, target) == Vector2.INF:
						ui_audio.play_cue("order_refused")
						return
					if event.shift_pressed:
						# Shift adds to the engineer's build queue and keeps placing.
						_queue_build_placement(build_mode_type, target)
						selecting = false
						selection_rect.visible = false
						return
					# A plain click replaces the queue, as a plain move order does.
					queued_build_orders.clear()
					_order_commander_to_build(build_mode_type, target)
					_sync_queued_build_markers()
					build_mode_type = -1
					if build_mode_type < 0 and build_ghost != null: build_ghost.queue_free(); build_ghost = null
					selecting = false
					selection_rect.visible = false
				else:
					_finish_selection(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			if build_mode_type >= 0 or fob_build_mode or recon_targeting or paradrop_targeting_type >= 0:
				_cancel_tactical_modes()
				return
			if material_order_mode != 0:
				_issue_material_site_order(event.position)
				return
			# RTS primary command: right-click sends selected units to the terrain
			# position.  Attack is deliberately an explicit modifier so an empty
			# patch of ground can never turn a move order into a rejected attack.
			# Shift appends a waypoint to each selected unit's queue instead of
			# replacing its order. Shift + left click (add to selection) and
			# held Shift (route overlay) use other buttons or no click at all.
			if event.ctrl_pressed:
				_issue_attack_order(event.position)
			else:
				_issue_move_order(event.position, event.shift_pressed)

	if event is InputEventKey and event.pressed:
		if _handle_hotkey(event.keycode, event):
			return

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
	approach_poses.clear()
	approach_from.clear()

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
	# First choice: just outside the command post's south gate, not inside it.
	var offsets: Array[Vector2] = [Vector2(0.0, CommandPostModel.FOOTPRINT.y * 0.5 + 10.0)]
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
		site.discovered = false
		site.known_owner = -1
		material_site_state[int(site.id)] = site
		var position: Vector2 = site.position
		# ResourceNode METAL is the game's single shared Materials economy; the
		# labels describe the near-future field source, not separate inventories.
		extension.call("economy_add_resource_node", int(site.id), position.x, position.y, 50000.0, 0)
		# Sites the enemy holds when the match starts. Later captures need a unit there.
		if int(site.owner) >= 0:
			extension.call("economy_assign_material_site", int(site.id), int(site.owner))
	_rebuild_material_site_views()


# Depots hold a finite stock that was shipped in before the battle. Nothing on
# the map produces fuel, so a captured depot runs dry for good.
func _spawn_fuel_depots() -> void:
	var file := FileAccess.open(FUEL_DEPOTS_PATH, FileAccess.READ)
	if file == null:
		push_warning("Fuel depot data is unavailable")
		return
	var content: Variant = JSON.parse_string(file.get_as_text())
	if not content is Dictionary:
		push_warning("Fuel depot data is malformed")
		return
	for definition in content.get("depots", []):
		var coordinates: Array = definition.get("position", [0, 0])
		var position := Vector2(float(coordinates[0]), float(coordinates[1]))
		var stock := float(definition.get("stock", 0.0))
		var id := int(definition.get("id", 0))
		var owner := int(definition.get("owner", -1))
		extension.call("economy_add_resource_node", id, position.x, position.y, stock, 1)
		if owner >= 0:
			extension.call("economy_assign_fuel_depot", id, owner)
		fuel_depot_state[id] = {
			"id": id,
			"name": String(definition.get("name", "FUEL DEPOT")),
			"kind": String(definition.get("kind", "civilian")),
			"position": position,
			"owner": owner,
			"stock": stock,
			"capacity": stock,
			"capturing": false,
			"discovered": false,
			"known_owner": -1,
		}
	_rebuild_material_site_views()


# Stations, pylons and poles. The simulation decides who each line powers;
# power_grid_view.gd draws what the player's side knows of it.
func _spawn_power_grid() -> void:
	if power_grid_view == null:
		power_grid_view = PowerGridViewScript.new()
		power_grid_view.name = "PowerGrid"
		power_grid_view.friendly_color = PLAYER_FACTION_COLOR
		power_grid_view.hostile_color = AI_FACTION_COLOR
		add_child(power_grid_view)
	power_grid_view.setup(extension, HUMAN_PLAYER_ID, _terrain_height_at,
		func(x: float, y: float) -> bool: return bool(extension.call("can_see", HUMAN_PLAYER_ID, x, y)))
	var path := String(scenario_definition.get("power_grid", ""))
	if path.is_empty():
		power_grid_view.clear()
		return
	if not power_grid_view.load_grid(path):
		push_warning("Power grid data is unavailable: %s" % path)


func _sync_power_grid() -> void:
	if power_grid_view == null or not power_grid_view.active:
		return
	power_grid_view.sync()
	var notice := String(power_grid_view.take_notice())
	if not notice.is_empty():
		_announce_intel(notice)


func _begin_battle() -> void:
	_apply_ai_style()
	extension.call("match_begin", int(commander_ids.get(HUMAN_PLAYER_ID, -1)), int(commander_ids.get(1, -1)))
	var victory: Dictionary = scenario_definition.get("victory", {})
	if victory.has("key_sites"):
		if not bool(extension.call("match_configure_key_sites", victory.get("key_sites", []), int(victory.get("required", 0)), int(victory.get("hold_seconds", 0)))):
			push_error("Broken Strait key-site victory could not be configured")
	_apply_force_arrival_rules()
	match_record_path = ""
	extension.call("match_record_set_info", "scenario", String(scenario_definition.get("id", "")))
	extension.call("match_record_set_info", "scenario_name", String(scenario_definition.get("display_name", "")))
	extension.call("match_record_set_info", "started_at", Time.get_datetime_string_from_system(true))
	extension.call("match_record_set_info", "player_faction", str(int(scenario_definition.player.faction_id)))
	extension.call("match_record_set_info", "ai_faction", str(int(scenario_definition.ai.faction_id)))
	for entry in [[scenario_definition.player, PLAYER_FACTION_COLOR], [scenario_definition.ai, AI_FACTION_COLOR]]:
		var side: Dictionary = entry[0]
		_add_command_post_view(int(side.faction_id), Vector2(float(side.spawn[0]), float(side.spawn[1])), entry[1])
	var assault: Dictionary = scenario_definition.ai.get("assault", {})
	if assault_enabled and not assault.is_empty():
		# The sector the briefing names. The AI is told no more than the player.
		var area: Array = assault.search_area
		extension.call("ai_configure_assault", float(area[0]), float(area[1]), float(area[2]), float(area[3]),
			int(assault.get("first_wave", 6)), int(assault.get("wave_growth", 4)), int(assault.get("max_wave", 24)))
	control_point_battle_status = String(scenario_definition.get("objective", ""))
	_setup_fog()


# Forces fly in rather than being built on the field. Each side lands them at
# an airfield it must build first. The AI builds its own through its Field
# Engineer (native AIManager::build_airfield), at the same price and time.
func _apply_force_arrival_rules() -> void:
	# Ground vehicles drive on shipped or captured fuel, from the same store.
	extension.call("ground_fuel_burn_set", bool(scenario_definition.get("ground_vehicles_burn_fuel", false)))
	if not bool(scenario_definition.get("forces_by_transport", false)):
		return
	extension.call("forces_by_transport_set", true)
	for side: Dictionary in scenario_definition.get("transport_routes", []):
		var faction_id := int(side.get("faction_id", -1))
		for mode_name in ["road", "rail"]:
			var route: Dictionary = side.get(mode_name, {})
			var entry: Array = route.get("entry", [])
			var terminal: Array = route.get("terminal", [])
			if entry.size() != 2 or terminal.size() != 2:
				continue
			var start := Vector2(float(entry[0]), float(entry[1]))
			var finish := Vector2(float(terminal[0]), float(terminal[1]))
			var mode := 1 if mode_name == "road" else 2
			if bool(extension.call("configure_transport_route", faction_id, mode, start.x, start.y, finish.x, finish.y)):
				_add_transport_corridor_view(faction_id, mode, start, finish)
			else:
				push_error("%s transport corridor for faction %d is invalid" % [mode_name, faction_id])
	if transport_corridor_views.has("0:1") and transport_corridor_views.has("0:2") and not reinforcement_delivery_enabled:
		reinforcement_delivery_status = "ROAD/RAIL CORRIDORS READY // AIRLIFT NEEDS AIRFIELD"
		_update_hud()


func _add_transport_corridor_view(faction_id: int, mode: int, start: Vector2, finish: Vector2) -> void:
	var root := Node3D.new()
	root.name = "%s_Corridor_%d" % ["Road" if mode == 1 else "Rail", faction_id]
	add_child(root)
	root.visible = faction_id == HUMAN_PLAYER_ID
	transport_corridor_views["%d:%d" % [faction_id, mode]] = root
	var bed := MeshInstance3D.new()
	bed.name = "Roadbed" if mode == 1 else "RailBallast"
	bed.mesh = _make_road_mesh(start, finish, 12.0 if mode == 1 else 9.0)
	bed.material_override = _demo_material(Color("#3b4242") if mode == 1 else Color("#514f4a"), 0.0)
	root.add_child(bed)
	var terminal_label := Label3D.new()
	terminal_label.name = "TerminalLabel"
	terminal_label.text = "ROAD TERMINAL" if mode == 1 else "RAILHEAD"
	terminal_label.font_size = 28
	terminal_label.pixel_size = 0.12
	terminal_label.outline_size = 3
	terminal_label.modulate = Color("#d6ddd7")
	terminal_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	terminal_label.position = Vector3(finish.x, _terrain_height_at(finish.x, finish.y) + 6.0, finish.y)
	root.add_child(terminal_label)
	if mode == 2:
		var side := (finish - start).normalized().orthogonal()
		var tie_mesh := BoxMesh.new()
		tie_mesh.size = Vector3(8.4, 0.16, 0.9)
		var ties := MultiMesh.new()
		ties.transform_format = MultiMesh.TRANSFORM_3D
		ties.mesh = tie_mesh
		ties.instance_count = maxi(1, int(start.distance_to(finish) / 8.0))
		var rail_yaw := atan2(finish.x - start.x, finish.y - start.y)
		for index in range(ties.instance_count):
			var fraction := (float(index) + 0.5) / float(ties.instance_count)
			var point := start.lerp(finish, fraction)
			var position := Vector3(point.x, _terrain_height_at(point.x, point.y) + 0.20, point.y)
			ties.set_instance_transform(index, Transform3D(Basis(Vector3.UP, rail_yaw), position))
		var tie_view := MultiMeshInstance3D.new()
		tie_view.name = "RailTies"
		tie_view.multimesh = ties
		tie_view.material_override = _demo_material(Color("#363b3c"), 0.0)
		root.add_child(tie_view)
		for offset in [-2.7, 2.7]:
			var rail := MeshInstance3D.new()
			rail.name = "SteelRail"
			rail.mesh = _make_road_mesh(start + side * offset, finish + side * offset, 0.28, 0.24)
			rail.material_override = _demo_material(Color("#a6aaa5"), 0.0)
			root.add_child(rail)


func _sync_ground_transport_presentation() -> void:
	if extension == null:
		return
	var inbound: Array = extension.call("delivery_state", HUMAN_PLAYER_ID).get("inbound", [])
	var seen := {}
	for force: Dictionary in inbound:
		var mode := int(force.get("route_mode", 0))
		if mode < 1 or not bool(force.get("in_transit", false)):
			continue
		var order_id := int(force.get("order_id", 0))
		seen[order_id] = true
		if not ground_transport_views.has(order_id):
			ground_transport_views[order_id] = _make_ground_transport_view(mode)
		var vehicle: Node3D = ground_transport_views[order_id]
		var start: Vector2 = force.get("entry", Vector2.ZERO)
		var finish: Vector2 = force.get("terminal", Vector2.ZERO)
		var point := start.lerp(finish, float(force.get("progress", 0.0)))
		vehicle.position = Vector3(point.x, _terrain_height_at(point.x, point.y) + 2.0, point.y)
		vehicle.rotation.y = atan2(finish.x - start.x, finish.y - start.y)
	for order_id in ground_transport_views.keys():
		if not seen.has(order_id):
			ground_transport_views[order_id].queue_free()
			ground_transport_views.erase(order_id)


func _make_ground_transport_view(mode: int) -> Node3D:
	var root := Node3D.new()
	root.name = "RailConvoy" if mode == 2 else "RoadConvoy"
	add_child(root)
	var body := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(3.8, 3.2, 26.0) if mode == 2 else Vector3(3.0, 2.8, 11.0)
	body.mesh = box
	body.material_override = _demo_material(Color("#596568") if mode == 2 else Color("#535e56"), 0.0)
	root.add_child(body)
	var cab := MeshInstance3D.new()
	var cab_box := BoxMesh.new()
	cab_box.size = Vector3(3.8, 4.1, 6.0) if mode == 2 else Vector3(3.0, 3.5, 3.4)
	cab.mesh = cab_box
	cab.position.z = 11.0 if mode == 2 else 4.0
	cab.material_override = _demo_material(Color("#424e52"), 0.0)
	root.add_child(cab)
	return root


func _setup_fog() -> void:
	if fog_view == null:
		fog_view = MeshInstance3D.new()
		fog_view.name = "FogOfWar"
		var quad := QuadMesh.new()
		fog_view.mesh = quad
		fog_view.extra_cull_margin = 16384.0
		fog_view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		var material := ShaderMaterial.new()
		material.shader = FogShader
		material.render_priority = 100
		fog_view.material_override = material
		add_child(fog_view)
	var fog_material := fog_view.material_override as ShaderMaterial
	fog_material.set_shader_parameter("world_size", Vector2(terrain_world_width, terrain_world_height))
	fog_material.set_shader_parameter("fog_strength", fog_of_war_strength)
	fog_texture = null
	_sync_vision()


# Only native sight reveals an enemy. Everything shown about them goes through here.
func _sync_vision() -> void:
	var revision := int(extension.call("vision_revision"))
	if revision == vision_revision_seen:
		return
	vision_revision_seen = revision
	var state: Dictionary = extension.call("vision_state", HUMAN_PLAYER_ID)
	var cells: PackedByteArray = state.get("cells", PackedByteArray())
	var width := int(state.get("width", 0))
	var height := int(state.get("height", 0))
	if width > 0 and height > 0 and cells.size() == width * height:
		var image := Image.create_from_data(width, height, false, Image.FORMAT_R8, cells)
		if fog_texture == null:
			fog_texture = ImageTexture.create_from_image(image)
			(fog_view.material_override as ShaderMaterial).set_shader_parameter("vision_texture", fog_texture)
		else:
			fog_texture.update(image)
	seen_enemy_ids.clear()
	for id in state.get("seen_enemies", PackedInt32Array()):
		seen_enemy_ids[int(id)] = true
	last_known_contacts = state.get("last_known", PackedFloat32Array())
	radar_contacts = state.get("radar_contacts", PackedFloat32Array())
	removed_entity_ids.clear()
	for id in extension.call("get_removed_entities", PackedInt32Array(entity_ids)):
		removed_entity_ids[int(id)] = true
		aircraft_heights.erase(int(id))
		approach_poses.erase(int(id))
		approach_from.erase(int(id))
		simulated_poses.erase(int(id))
		interpolation_from.erase(int(id))
	var enemy_post := int(commander_ids.get(1, -1))
	if not enemy_post_discovered and seen_enemy_ids.has(enemy_post):
		enemy_post_discovered = true
		var post: Node3D = command_post_views.get(1)
		if post != null:
			post.visible = true
		_announce_intel("ENEMY COMMAND POST LOCATED")
	for mode in [1, 2]:
		var key := "1:%d" % mode
		var route_view: Node3D = transport_corridor_views.get(key)
		if route_view != null and not route_view.visible:
			var route: Dictionary = extension.call("transport_route", 1, mode)
			var terminal: Vector2 = route.get("terminal", Vector2.INF)
			if terminal != Vector2.INF and bool(extension.call("can_see", HUMAN_PLAYER_ID, terminal.x, terminal.y)):
				route_view.visible = true
	_refresh_site_intel()


func _is_unit_shown(entity_id: int) -> bool:
	if removed_entity_ids.has(entity_id):
		return false
	return not ai_entity_ids.has(entity_id) or seen_enemy_ids.has(entity_id)


func _announce_intel(message: String) -> void:
	fuel_supply_status = message
	fuel_supply_status_expires_msec = Time.get_ticks_msec() + int(FUEL_STATUS_SECONDS * 1000.0)


# Who holds a site is only known from the last time it was seen.
func _refresh_site_intel() -> void:
	var changed := false
	for site in material_site_state.values():
		if bool(site.get("destroyed", false)):
			continue
		var position: Vector2 = site.position
		var newly_visible := bool(extension.call("can_see", HUMAN_PLAYER_ID, position.x, position.y))
		if newly_visible and (not bool(site.get("discovered", false)) or int(site.get("known_owner", -1)) != int(site.owner)):
			site.discovered = true
			site.known_owner = int(site.owner)
			changed = true
	for site in fuel_depot_state.values():
		var position: Vector2 = site.position
		if not bool(extension.call("can_see", HUMAN_PLAYER_ID, position.x, position.y)):
			continue
		if not bool(site.discovered) or int(site.known_owner) != int(site.owner):
			if not bool(site.discovered) and String(site.kind) == "military":
				_announce_intel("ENEMY FUEL DEPOT LOCATED")
			site.discovered = true
			site.known_owner = int(site.owner)
			changed = true
	if changed:
		_rebuild_material_site_views()
	var airfields: PackedFloat32Array = extension.call("enemy_airfields_in_sight", HUMAN_PLAYER_ID)
	for index in range(0, airfields.size() - 1, 2):
		var position := Vector2(airfields[index], airfields[index + 1])
		var key := Vector2i(roundi(position.x), roundi(position.y))
		if not enemy_airfield_views.has(key):
			enemy_airfield_views[key] = _add_enemy_airfield_view(position)
			_announce_intel("ENEMY AIRFIELD LOCATED")
	_sync_power_grid()


func _add_enemy_airfield_view(position: Vector2) -> Node3D:
	var root := Node3D.new()
	root.name = "EnemyAirfield_%d_%d" % [roundi(position.x), roundi(position.y)]
	root.set_meta("structure_type", 2)
	root.position = Vector3(position.x, _terrain_height_at(position.x, position.y) + 0.05, position.y)
	_add_airfield_paving(root)
	_add_airfield_buildings(root)
	add_child(root)
	return root


func _add_command_post_view(faction_id: int, position: Vector2, color: Color) -> void:
	# The pad sits on the highest ground under its footprint; its skirt reaches
	# the lowest, so the post is level and never floats over a slope.
	var half: Vector2 = CommandPostModel.FOOTPRINT * 0.5
	var highest := -INF
	var lowest := INF
	for sample_x in [-1.0, 0.0, 1.0]:
		for sample_z in [-1.0, 0.0, 1.0]:
			var height := _terrain_height_at(position.x + sample_x * half.x, position.y + sample_z * half.y)
			highest = maxf(highest, height)
			lowest = minf(lowest, height)
	var root := CommandPostModel.build(color, highest - lowest + 0.6)
	root.name = "CommandPost_%d" % faction_id
	root.set_meta("faction_id", faction_id)
	root.position = Vector3(position.x, highest, position.y)
	root.visible = faction_id == HUMAN_PLAYER_ID
	add_child(root)
	command_post_views[faction_id] = root


# The enemy's style and difficulty: RTS_AI_DOCTRINE and RTS_AI_DIFFICULTY
# override the scenario, which overrides data/ai_doctrines.json's defaults.
func _apply_ai_style() -> void:
	var styles: Dictionary = extension.call("ai_styles")
	var side: Dictionary = scenario_definition.get("ai", {})
	var doctrine := String(side.get("doctrine", styles.default_doctrine))
	var difficulty := String(side.get("difficulty", styles.default_difficulty))
	if OS.get_environment("RTS_AI_DOCTRINE") != "":
		doctrine = OS.get_environment("RTS_AI_DOCTRINE")
	if OS.get_environment("RTS_AI_DIFFICULTY") != "":
		difficulty = OS.get_environment("RTS_AI_DIFFICULTY")
	if not bool(extension.call("ai_set_style", doctrine, difficulty)):
		push_warning("Unknown AI style '%s' on '%s'; using the defaults" % [doctrine, difficulty])
		extension.call("ai_set_style", String(styles.default_doctrine), String(styles.default_difficulty))
	ai_style_state = extension.call("ai_style")


# Every finished match is kept for study and, later, for training the AI.
func _save_match_record() -> String:
	var folder := OS.get_environment("RTS_MATCH_RECORD_DIR")
	if folder == "":
		folder = "user://match_records"
	if DirAccess.make_dir_recursive_absolute(folder) != OK and not DirAccess.dir_exists_absolute(folder):
		push_warning("Could not create the match record folder %s" % folder)
		return ""
	var stamp := Time.get_datetime_string_from_system(true).replace(":", "-")
	var path := "%s/%s_%s.json" % [folder, String(scenario_definition.get("id", "match")), stamp]
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		push_warning("Could not write the match record %s" % path)
		return ""
	file.store_string(String(extension.call("match_record_json")))
	file.close()
	return path


func _update_match_state() -> void:
	var state: Dictionary = extension.call("match_state")
	var result := int(state.get("result", -1))
	if result < 0:
		if int(state.get("key_sites_required", 0)) > 0:
			control_point_battle_status = "COMMAND POST OR KEY SITES %d/%d // HOLD %.0f/%.0f SEC // ENEMY %.0f SEC" % [
				int(state.get("key_sites_player", 0)), int(state.get("key_sites_required", 0)),
				float(state.get("key_site_hold_player_seconds", 0.0)), float(state.get("key_site_hold_target_seconds", 0.0)),
				float(state.get("key_site_hold_enemy_seconds", 0.0))]
		return
	match_finished = true
	if match_record_path == "" and bool(extension.call("match_record_state").get("finished", false)):
		match_record_path = _save_match_record()
	var post: Node3D = command_post_views.get(1 if result == 0 else HUMAN_PLAYER_ID)
	if post != null and result != 2 and int(state.get("victory_reason", 0)) == 0:
		post.visible = false
	var seconds := int(float(state.get("seconds", 0.0)))
	var titles := ["VICTORY", "DEFEAT", "DRAW"]
	var subtitles := ["ENEMY COMMAND POST DESTROYED", "YOUR COMMAND POST WAS DESTROYED", "BOTH COMMAND POSTS DESTROYED"]
	if int(state.get("victory_reason", 0)) == 1:
		subtitles = ["KEY SITES HELD", "ENEMY HELD THE KEY SITES", "BOTH SIDES HELD THE KEY SITES"]
	if match_end_panel == null:
		match_end_panel = MatchEndPanelScript.new()
		match_end_panel.name = "MatchEnd"
		match_end_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		match_end_panel.play_again_requested.connect(func(): get_tree().reload_current_scene())
		match_end_panel.quit_requested.connect(func(): get_tree().quit())
		ui_root.add_child(match_end_panel)
	var accents := [MandateTokens.SUCCESS, MandateTokens.DANGER, MandateTokens.WARNING]
	match_end_panel.show_result(titles[result], subtitles[result],
		"MATCH TIME %d:%02d  //  ENEMY WAVES %d\nENEMY COMMANDER  %s  //  %s" % [seconds / 60, seconds % 60, int(state.get("waves", 0)),
			String(ai_style_state.get("doctrine_name", "")).to_upper(), String(ai_style_state.get("difficulty_name", "")).to_upper()], accents[result])


func _clear_material_sites() -> void:
	for child in resource_sites.get_children():
		resource_sites.remove_child(child)
		child.queue_free()


func _rebuild_material_site_views() -> void:
	_clear_material_sites()
	for site in material_site_state.values():
		if bool(site.get("destroyed", false)) or not bool(site.get("discovered", false)):
			continue
		var root := Node3D.new()
		root.name = "MaterialFacility_%d" % int(site.id)
		var position: Vector2 = site.position
		root.position = Vector3(position.x, _terrain_height_at(position.x, position.y), position.y)
		resource_sites.add_child(root)
		var owner := int(site.get("known_owner", -1))
		var signal_color: Color = PLAYER_FACTION_COLOR if owner == HUMAN_PLAYER_ID else AI_FACTION_COLOR if owner == 1 else Color("#ffbd52")
		var hull := _demo_material(Color("#2b3537"))
		var signal_material := _demo_material(signal_color, 1.2)
		_add_box_model(root, Vector3(4.4, 0.55, 3.4), Vector3(0, 0.28, 0), hull)
		_add_cylinder_model(root, 0.95, 1.35, Vector3(0, 0.95, 0), hull)
		_add_box_model(root, Vector3(0.22, 2.2, 0.22), Vector3(-1.55, 1.25, -0.85), signal_material)
		_add_box_model(root, Vector3(0.22, 2.2, 0.22), Vector3(1.55, 1.25, -0.85), signal_material)
		_add_cylinder_model(root, 0.25, 0.16, Vector3(0, 1.72, 0), signal_material)
	for site in fuel_depot_state.values():
		if bool(site.discovered):
			_add_fuel_depot_view(site)


func _add_fuel_depot_view(site: Dictionary) -> void:
	var root := Node3D.new()
	root.name = "FuelDepot_%d" % int(site.id)
	var position: Vector2 = site.position
	root.position = Vector3(position.x, _terrain_height_at(position.x, position.y), position.y)
	resource_sites.add_child(root)
	var owner := int(site.get("known_owner", -1))
	var signal_color: Color = PLAYER_FACTION_COLOR if owner == HUMAN_PLAYER_ID else AI_FACTION_COLOR if owner == 1 else Color("#d9d3c4")
	var tank := _demo_material(Color("#5d6468"))
	var signal_material := _demo_material(signal_color, 1.2)
	_add_box_model(root, Vector3(9.0, 0.3, 6.0), Vector3(0, 0.15, 0), _demo_material(Color("#2b3537")))
	for offset in [Vector3(-2.6, 0, 0), Vector3(0, 0, 0), Vector3(2.6, 0, 0)]:
		_add_cylinder_model(root, 1.15, 2.4, offset + Vector3(0, 1.5, 0), tank)
		_add_cylinder_model(root, 1.2, 0.18, offset + Vector3(0, 1.9, 0), signal_material)


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
	var scenes: Array[PackedScene] = []
	var scene_bounds: Array[AABB] = []
	var type_cycle: Array[int] = []
	for definition in type_definitions:
		var dimensions: Array = definition.get("size", [60, 30, 50])
		transforms.append([])
		colors.append(Color(String(definition.get("color", "#68736f"))))
		sizes.append(Vector3(float(dimensions[0]), float(dimensions[1]), float(dimensions[2])))
		var scene_path := String(definition.get("scene", ""))
		var scene := load(scene_path) as PackedScene if not scene_path.is_empty() else null
		if not scene_path.is_empty() and scene == null:
			push_warning("Civilian model could not be loaded: %s" % scene_path)
		scenes.append(scene)
		var model_bounds := AABB()
		if scene != null:
			var sample := scene.instantiate() as Node3D
			if sample != null:
				var limits: Array = [Vector3(INF, INF, INF), Vector3(-INF, -INF, -INF)]
				_civilian_collect_bounds(sample, Transform3D.IDENTITY, limits)
				if (limits[0] as Vector3).x < INF:
					model_bounds = AABB(limits[0], (limits[1] as Vector3) - (limits[0] as Vector3))
				sample.free()
		scene_bounds.append(model_bounds)
		for _weight in range(maxi(1, int(definition.get("weight", 1)))):
			type_cycle.append(transforms.size() - 1)
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
		var attempts := int(cluster.get("count", 0)) * 24
		var placed := 0
		for _attempt in range(attempts):
			if placed >= int(cluster.get("count", 0)):
				break
			var candidate := center + Vector2(rng.randf_range(-spread.x, spread.x), rng.randf_range(-spread.y, spread.y))
			if not bool(extension.call("is_land_position", candidate.x, candidate.y)):
				continue
			var type_index: int = type_cycle[(placed + int(cluster.get("seed", 0))) % type_cycle.size()]
			var building_size: Vector3 = sizes[type_index]
			# Civilian dressing is authored scenery rather than a military build
			# order. A building still needs ground that supports its whole footprint.
			var local_height := _terrain_height_at(candidate.x, candidate.y)
			var local_height_min := local_height
			var local_height_max := local_height
			for sample_offset in [Vector2(-building_size.x * 0.5, 0.0), Vector2(building_size.x * 0.5, 0.0),
				Vector2(0.0, -building_size.z * 0.5), Vector2(0.0, building_size.z * 0.5)]:
				var sample_height := _terrain_height_at(candidate.x + sample_offset.x, candidate.y + sample_offset.y)
				local_height_min = minf(local_height_min, sample_height)
				local_height_max = maxf(local_height_max, sample_height)
			if local_height_max - local_height_min > 12.0:
				continue
			var too_close := false
			for protected in spawn_origins:
				if candidate.distance_to(protected) < 650.0:
					too_close = true
					break
			if too_close:
				continue
			var scale := rng.randf_range(0.82, 1.16)
			var height := _terrain_height_at(candidate.x, candidate.y)
			var angle := rng.randf_range(0.0, TAU)
			if scenes[type_index] != null and scene_bounds[type_index].size.y > 0.0:
				var view := Node3D.new()
				view.name = "Civilian_%s_%d" % [String(type_definitions[type_index].get("id", type_index)), civilian_building_instance_count]
				view.position = Vector3(candidate.x, height, candidate.y)
				view.rotation.y = angle
				view.scale = Vector3.ONE * scale
				var model := scenes[type_index].instantiate() as Node3D
				var bounds := scene_bounds[type_index]
				var model_scale: float = building_size.y / bounds.size.y
				model.scale = Vector3.ONE * model_scale
				model.position = -Vector3(bounds.get_center().x, bounds.position.y, bounds.get_center().z) * model_scale
				view.add_child(model)
				civilian_buildings.add_child(view)
			else:
				var transform := Transform3D(
					Basis(Vector3.UP, angle).scaled(Vector3(scale, scale, scale)),
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


func _civilian_collect_bounds(node: Node3D, parent_transform: Transform3D, limits: Array) -> void:
	var world_transform := parent_transform * node.transform
	if node is MeshInstance3D and (node as MeshInstance3D).mesh != null:
		var mesh_bounds := (node as MeshInstance3D).mesh.get_aabb()
		for x in [0.0, 1.0]:
			for y in [0.0, 1.0]:
				for z in [0.0, 1.0]:
					var point := world_transform * (mesh_bounds.position + mesh_bounds.size * Vector3(x, y, z))
					limits[0] = (limits[0] as Vector3).min(point)
					limits[1] = (limits[1] as Vector3).max(point)
	for child in node.get_children():
		if child is Node3D:
			_civilian_collect_bounds(child, world_transform, limits)


func _set_material_order_mode(mode: int) -> void:
	var commander_id := _player_engineer_id()
	if commander_id <= 0:
		return
	# Arm the mode even when nothing friendly is selected. A selected force stays
	# selected, because any friendly unit can capture a fuel depot.
	var friendly_selected := false
	for id in selected_ids:
		friendly_selected = friendly_selected or player_entity_ids.has(id)
	if not friendly_selected:
		_clear_selection()
		_set_selected(commander_id, true)
	material_order_mode = mode
	_update_hud()


func _issue_material_site_order(screen_position: Vector2) -> void:
	var target := _screen_ground_point(screen_position)
	if target == Vector2.INF:
		return
	var selected_site: Dictionary = {}
	var nearest_distance := maxf(8.0, _click_tolerance_meters())
	for site in material_site_state.values():
		if bool(site.get("destroyed", false)):
			continue
		var position: Vector2 = site.position
		var distance := target.distance_to(position)
		if distance <= nearest_distance:
			nearest_distance = distance
			selected_site = site
	for site in fuel_depot_state.values():
		if not bool(site.discovered):
			continue
		var distance := target.distance_to(site.position)
		if distance <= nearest_distance:
			nearest_distance = distance
			selected_site = site
	var commander_id := _player_engineer_id()
	if selected_site.is_empty():
		# Nothing of the Materials or fuel economy there: a power station or line?
		if power_grid_view != null and power_grid_view.active:
			var worker := commander_id
			for id in selected_ids:
				if player_entity_ids.has(id):
					worker = id
					break
			var result: Dictionary = power_grid_view.order_at(target, _click_tolerance_meters(), material_order_mode, worker, commander_id)
			if bool(result.handled):
				_announce_intel(String(result.message))
				material_order_mode = 0
				_update_hud()
		return
	var site_position: Vector2 = selected_site.position
	if fuel_depot_state.has(int(selected_site.id)):
		# Any friendly unit can take a depot; the slow engineer is only the fallback.
		var capturer := commander_id
		for id in selected_ids:
			if player_entity_ids.has(id):
				capturer = id
				break
		if material_order_mode == 1:
			extension.call("issue_harvest_commands", PackedInt32Array([capturer]), HUMAN_PLAYER_ID, site_position.x, site_position.y)
		elif material_order_mode == 2 and extension.call("destroy_resource_site", capturer, site_position.x, site_position.y):
			fuel_depot_state.erase(int(selected_site.id))
		material_order_mode = 0
		_rebuild_material_site_views()
		_update_hud()
		return
	if material_order_mode == 1:
		var accepted: int = extension.call("issue_harvest_commands", PackedInt32Array([commander_id]), HUMAN_PLAYER_ID, site_position.x, site_position.y)
		# The site changes hands when the engineer reaches it (native), and
		# _sync_fuel_supply picks the new holder up from material_sites_state.
		if accepted == 1:
			selected_site.capturing = true
	elif material_order_mode == 2:
		if extension.call("destroy_resource_site", commander_id, site_position.x, site_position.y):
			selected_site.destroyed = true
			material_site_state[int(selected_site.id)] = selected_site
	material_order_mode = 0
	_rebuild_material_site_views()
	_update_hud()


func _visual_id_for_unit_faction(unit_type: int, faction_id: int) -> String:
	var faction_overrides: Dictionary = FACTION_UNIT_VISUAL_OVERRIDES.get(faction_id, {})
	if faction_overrides.has(unit_type):
		return String(faction_overrides[unit_type])
	return String(extension.call("get_unit_visual_id", unit_type))


func _unit_model_ground_clearance(unit_type: int) -> float:
	return ENGINEER_MODEL_GROUND_CLEARANCE if UnitCatalog.is_construction_unit(unit_type) else UNIT_MODEL_GROUND_CLEARANCE


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
		var visual_id := _visual_id_for_unit_faction(unit_type, faction_id)
		var wrapper = visual_spawn_bridge.spawn(self, visual_registry, visual_id, Transform3D(Basis.IDENTITY, Vector3(world.x, _terrain_height_at(world.x, world.y) + _unit_model_ground_clearance(unit_type), world.y)), color)
		wrapper.set_strategic_icon(unit_type, color)
		_configure_unit_ranges(wrapper, entity_id)
		wrapper.set_range_terrain_height_sampler(func(world_x: float, world_z: float): return _terrain_height_at(world_x, world_z))
		prototype_visual_views[entity_id] = wrapper
	else:
		_create_demo_unit(entity_id, unit_type, color, Vector3(world.x, _terrain_height_at(world.x, world.y), world.y))
	if player_controlled:
		player_entity_ids.append(entity_id)
	else:
		ai_entity_ids.append(entity_id)
		# Opponents never see the enemy's range outlines, even on a seen unit.
		if prototype_visual_views.has(entity_id):
			prototype_visual_views[entity_id].set_range_overlays_visible(false)


func _sync_new_entities() -> void:
	var newly_presented: Array = extension.call("get_unpresented_entities", PackedInt32Array(entity_ids))
	for entity in newly_presented:
		var entity_id := int(entity.get("id", -1))
		if hidden_base_ids.has(entity_id):
			continue
		if entity_to_instance.has(entity_id):
			continue
		var faction_id := int(entity.get("faction_id", -1))
		if faction_id < 0 or faction_id > 2:
			continue
		var position := Vector2(float(entity.get("x", 0.0)), float(entity.get("y", 0.0)))
		var unit_type := maxi(int(entity.get("unit_type", 0)), 0)
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
				var visual_id := _visual_id_for_unit_faction(unit_type, faction_id)
				var wrapper = visual_spawn_bridge.spawn(self, visual_registry, visual_id, Transform3D(Basis.IDENTITY, Vector3(world_x, 0.4, world_y)), color)
				wrapper.set_strategic_icon(unit_type, color)
				_configure_unit_ranges(wrapper, entity_id)
				# Ground ranges are drawn merged from the first frame; per-unit hex
				# meshes rebuilt on every move are far too slow for a crowd.
				wrapper.set_merged_ground_ranges(true)
				wrapper.set_range_terrain_height_sampler(func(world_x: float, world_z: float): return _terrain_height_at(world_x, world_z))
				prototype_visual_views[entity_id] = wrapper
			if player_controlled:
				player_entity_ids.append(entity_id)
			else:
				ai_entity_ids.append(entity_id)
				if prototype_visual_views.has(entity_id):
					prototype_visual_views[entity_id].set_range_overlays_visible(false)
			side_index += 1
	return true


# Ground units' range hexes, merged per side and kind into one outline each.
func _update_range_envelopes() -> void:
	if range_envelope_renderer == null:
		range_envelope_renderer = RangeEnvelopeRendererScript.new()
		range_envelope_renderer.name = "RangeEnvelopes"
		add_child(range_envelope_renderer)
	var player_ids := {}
	for id in player_entity_ids:
		player_ids[id] = true
	var hexes_by_key := {}
	for entity_id in prototype_visual_views:
		var wrapper = prototype_visual_views[entity_id]
		if not is_instance_valid(wrapper):
			continue
		wrapper.set_merged_ground_ranges(true)
		if not wrapper.visible or wrapper.ranges_airborne():
			continue
		var side := HUMAN_PLAYER_ID if player_ids.has(entity_id) else 1
		var center := Vector2(wrapper.global_position.x, wrapper.global_position.z)
		for envelope in wrapper.range_envelopes():
			var key := "%d:%s" % [side, envelope.kind]
			if not hexes_by_key.has(key):
				hexes_by_key[key] = []
			hexes_by_key[key].append({"center": center, "radius": envelope.radius, "dash_phase": envelope.dash_phase})
	range_envelope_renderer.update_envelopes(hexes_by_key, func(x: float, z: float): return _terrain_height_at(x, z))


func _configure_unit_ranges(wrapper: Node3D, entity_id: int) -> void:
	var ranges: PackedFloat32Array = extension.call("get_unit_range_profile", entity_id)
	if ranges.size() == 3:
		wrapper.set_gameplay_ranges(ranges[0], ranges[1], ranges[2])


# The same presented pose drives the aircraft model, range plane and icon leader.
func _unit_overlay_anchor(entity_id: int) -> Vector3:
	var wrapper := prototype_visual_views.get(entity_id) as Node3D
	if wrapper != null:
		return wrapper.global_position + Vector3.UP * 5.0
	var index := int(entity_to_instance.get(entity_id, -1))
	if index >= 0 and index * 2 + 1 < unit_positions.size():
		var x := unit_positions[index * 2]
		var z := unit_positions[index * 2 + 1]
		return Vector3(x, _terrain_height_at(x, z) + 5.0, z)
	return Vector3.ZERO


func _sync_unit_transforms() -> Vector2:
	var fetch_start_us := Time.get_ticks_usec()
	var latest_transforms: PackedFloat32Array = extension.call("get_unit_transforms", entity_ids)
	var fetch_ms := float(Time.get_ticks_usec() - fetch_start_us) / 1000.0
	if latest_transforms.size() != entity_ids.size() * 3:
		push_error("Native transform snapshot size did not match entity IDs")
		return Vector2(fetch_ms, 0.0)
	unit_positions = PackedFloat32Array()
	unit_positions.resize(entity_ids.size() * 2)
	# Model detail follows the closest camera drawing units, so the magnifier
	# keeps models loaded while the main view shows map-scale icons.
	var unit_lod_distance := camera_distance
	if magnifier_view != null and magnifier_view.is_enabled():
		unit_lod_distance = minf(camera_distance, magnifier_view.distance_for(camera_distance))
	var flight_poses: PackedFloat32Array = extension.call("get_aircraft_approach_poses", entity_ids)
	var airborne: PackedByteArray = extension.call("get_airborne_flags", entity_ids)
	if airborne.size() != entity_ids.size():
		airborne.resize(entity_ids.size())
	var climb_step := AIRCRAFT_CLIMB_RATE * maxf(get_process_delta_time(), 1.0 / 60.0)
	# The simulation advances in 50 ms ticks. Drawing only the latest tick makes
	# units move and turn in visible steps, so the view blends each unit from
	# its previous simulated pose to its latest one by the time elapsed since
	# that tick. Like Forged Alliance, the picture runs up to one tick behind.
	var clock: PackedFloat64Array = extension.call("get_tick_clock")
	var tick := int(clock[0]) if clock.size() == 2 else -1
	var blend := float(clock[1]) if clock.size() == 2 else 1.0
	var new_tick := tick != interpolation_tick
	interpolation_tick = tick
	var upload_start_us := Time.get_ticks_usec()
	for index in range(entity_ids.size()):
		var entity_id: int = entity_ids[index]
		var sim_x := latest_transforms[index * 3]
		var sim_z := latest_transforms[index * 3 + 1]
		var sim_heading := latest_transforms[index * 3 + 2] + PI
		unit_positions[index * 2] = sim_x
		unit_positions[index * 2 + 1] = sim_z
		var target := Vector3(sim_x, sim_z, sim_heading)
		if new_tick or not interpolation_from.has(entity_id):
			var from: Vector3 = simulated_poses.get(entity_id, target)
			# A unit that jumped (spawned, teleported, fast-forwarded) snaps.
			if Vector2(from.x - sim_x, from.y - sim_z).length() > UNIT_SNAP_DISTANCE:
				from = target
			interpolation_from[entity_id] = from
			simulated_poses[entity_id] = target
		var start: Vector3 = interpolation_from[entity_id]
		var x := lerpf(start.x, sim_x, blend)
		var z := lerpf(start.y, sim_z, blend)
		var heading := lerp_angle(start.z, sim_heading, blend)
		unit_visual_headings[entity_id] = heading
		var basis := Basis(Vector3.UP, heading)
		var approach := Vector3(-1.0, 0.0, 0.0)
		if flight_poses.size() == entity_ids.size() * 3 and flight_poses[index * 3] >= 0.0:
			var latest := Vector3(flight_poses[index * 3], flight_poses[index * 3 + 1], flight_poses[index * 3 + 2])
			if new_tick or not approach_from.has(entity_id):
				approach_from[entity_id] = approach_poses.get(entity_id, latest)
				approach_poses[entity_id] = latest
			approach = (approach_from[entity_id] as Vector3).lerp(latest, blend)
			basis = basis * Basis(Vector3.RIGHT, approach.y) * Basis(Vector3.FORWARD, approach.z)
		if prototype_visuals_enabled and prototype_visual_views.has(entity_ids[index]):
			var wrapper = prototype_visual_views[entity_ids[index]]
			var ground := _terrain_height_at(x, z)
			var height := ground + _unit_model_ground_clearance(int(entity_unit_types.get(entity_ids[index], 0)))
			if approach.x >= 0.0:
				height += approach.x
				aircraft_heights.erase(entity_id)
			else:
				height = _presented_flight_height(entity_ids[index], airborne[index] != 0, x, z, height, climb_step)
			# Vehicles on land sit on the slope beneath them; aircraft in the air
			# and vessels on the water stay level.
			if approach.x < 0.0 and not aircraft_heights.has(entity_id) and ground > AIRCRAFT_SEA_LEVEL + 0.5:
				basis = Basis(Quaternion(Vector3.UP, _terrain_normal_at(x, z))) * basis
			wrapper.set_ranges_airborne(airborne[index] != 0)
			wrapper.apply_simulation_transform(Transform3D(basis, Vector3(x, height, z)))
			wrapper.update_lod(unit_lod_distance)
			wrapper.visible = _is_unit_shown(entity_ids[index])
		else:
			var shown := _is_unit_shown(entity_ids[index])
			unit_multimesh.set_instance_transform(index, Transform3D(basis if shown else basis.scaled(Vector3.ZERO), Vector3(x, 0.4, z)))
			var demo_root: Node3D = demo_unit_views.get(entity_ids[index], null)
			if demo_root != null:
				demo_root.visible = shown
				demo_root.transform = Transform3D(
					basis.scaled(demo_root.scale),
					Vector3(x, _terrain_height_at(x, z), z)
				)
				_set_unit_lod(demo_root)
	var upload_ms := float(Time.get_ticks_usec() - upload_start_us) / 1000.0
	return Vector2(fetch_ms, upload_ms)


# Ground normal from the heights either side of a point, about a hull's length.
func _terrain_normal_at(x: float, z: float) -> Vector3:
	var span := UNIT_TILT_SAMPLE_SPAN
	var normal := Vector3(
		_terrain_height_at(x - span, z) - _terrain_height_at(x + span, z),
		2.0 * span,
		_terrain_height_at(x, z - span) - _terrain_height_at(x, z + span))
	return normal.normalized()


# Aircraft in flight are drawn at cruise altitude over whatever is higher, the
# ground or the sea, and climb or descend to it smoothly. Units on the ground
# keep their ground height. The simulation stays 2D; this is presentation only.
func _presented_flight_height(entity_id: int, in_flight: bool, x: float, z: float, ground_height: float, climb_step: float) -> float:
	if not in_flight and not aircraft_heights.has(entity_id):
		return ground_height
	var target := ground_height
	if in_flight:
		target = maxf(_terrain_height_at(x, z), AIRCRAFT_SEA_LEVEL) + AIRCRAFT_CRUISE_ALTITUDE
	# An aircraft first seen in flight (a ferry arriving) is already at altitude.
	var current: float = aircraft_heights.get(entity_id, target)
	current = move_toward(current, target, climb_step)
	# Never let a climb or descent cut through rising ground.
	current = maxf(current, ground_height)
	if not in_flight and is_equal_approx(current, ground_height):
		aircraft_heights.erase(entity_id)
	else:
		aircraft_heights[entity_id] = current
	return current


func _validate_state() -> void:
	var count: int = extension.call("get_entity_count")
	if count != entity_ids.size():
		push_error("Entity count drift: expected %d, got %d" % [entity_ids.size(), count])
	
	var latest_transforms: PackedFloat32Array = extension.call("get_unit_transforms", entity_ids)
	if latest_transforms.size() != entity_ids.size() * 3:
		push_error("Transform snapshot size mismatch: expected %d, got %d" % [entity_ids.size() * 3, latest_transforms.size()])
		return
	
	for i in range(entity_ids.size()):
		var x: float = latest_transforms[i * 3]
		var z: float = latest_transforms[i * 3 + 1]
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
	if audio_settings != null and audio_settings.visible:
		return
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
			_pan_camera(Vector3(free_direction.x, 0.0, free_direction.y) * speed * delta)
		else:
			var forward := Vector2(sin(tactical_camera_yaw), cos(tactical_camera_yaw))
			var right := Vector2(cos(tactical_camera_yaw), -sin(tactical_camera_yaw))
			var yawed_direction := right * direction.x + forward * direction.y
			_pan_camera(Vector3(yawed_direction.x, 0.0, yawed_direction.y) * speed * delta)


# The camera moves with its target, so a pan covers exactly speed x time. Easing
# the camera after a moving target lurched forward after every long frame.
func _pan_camera(step: Vector3) -> void:
	camera_target += step
	if camera != null:
		camera.global_position += step


func _rotate_camera_facing(direction: float, delta: float) -> void:
	if free_float_camera:
		free_camera_yaw += direction * CAMERA_YAW_SPEED * delta
	else:
		tactical_camera_yaw += direction * CAMERA_YAW_SPEED * delta


func _update_camera(delta: float) -> void:
	# Exponential easing covers the same share of the gap per second at any frame
	# rate. The old clamp(delta * k) eased unevenly and snapped after a long frame.
	camera_distance = lerpf(camera_distance, target_camera_distance, 1.0 - exp(-9.0 * delta))
	camera.near = clampf(camera_distance * CAMERA_NEAR_PER_DISTANCE, CAMERA_MIN_NEAR, CAMERA_MAX_NEAR)
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
	camera.global_position = camera.global_position.lerp(desired_position, 1.0 - exp(-10.0 * delta))
	camera.look_at(camera_target, Vector3.UP)
	# Unit models are on their own layer: the main view draws them only up close,
	# so the magnifier can still show them when the main view is zoomed out.
	var model_layer := int(UnitVisualRoot.MODEL_LAYER)
	camera.cull_mask = (camera.cull_mask | model_layer) if camera_distance < UnitVisualRoot.MODEL_HIDE_DISTANCE else (camera.cull_mask & ~model_layer)
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


func _cycle_camera_zoom() -> void:
	var next_distance: float = CAMERA_ZOOM_LEVELS[0]
	for level: float in CAMERA_ZOOM_LEVELS:
		if level > target_camera_distance + 0.01:
			next_distance = level
			break
	_zoom_toward_cursor(get_viewport().get_mouse_position(), next_distance / target_camera_distance)


func _can_change_game_speed() -> bool:
	if extension == null or bool(extension.call("is_multiplayer_session")):
		return false
	# Godot scene networking is separate from the native TCP transport.
	return not multiplayer.has_multiplayer_peer() or multiplayer.multiplayer_peer is OfflineMultiplayerPeer


func _enforce_game_speed_policy() -> void:
	if not _can_change_game_speed() and float(extension.call("get_game_speed")) != 1.0:
		extension.call("set_game_speed", 1.0)


func _change_game_speed(direction: int) -> void:
	if _can_change_game_speed():
		var current := float(extension.call("get_game_speed"))
		var next := clampf(current + 0.25 * direction, 0.25, 1.75)
		if next != current:
			extension.call("set_game_speed", next)
			ui_audio.play_cue("speed_step")
	else:
		_enforce_game_speed_policy()
	_update_hud()


func _show_exit_confirm() -> void:
	if exit_confirm == null:
		exit_confirm = ExitConfirmScript.new()
		exit_confirm.name = "ExitConfirm"
		exit_confirm.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		exit_confirm.exit_requested.connect(func(): get_tree().quit())
		ui_root.add_child(exit_confirm)
	exit_confirm.show()


func _zoom_toward_cursor(screen_position: Vector2, multiplier: float) -> void:
	if not is_finite(multiplier) or multiplier <= 0.0 or multiplier == 1.0:
		return
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

	# Units take the click first; then the player's own command post. The
	# enemy's post is never selectable, only hovered once discovered.
	if _command_post_under_cursor(HUMAN_PLAYER_ID, screen_position):
		_select_command_post()
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


# True when the cursor is over a command post's compound. The enemy's post only
# counts once it has been discovered and is drawn.
func _command_post_under_cursor(faction_id: int, screen_position: Vector2) -> bool:
	var post: Node3D = command_post_views.get(faction_id)
	if post == null or not is_instance_valid(post) or not post.visible:
		return false
	if faction_id != HUMAN_PLAYER_ID and not enemy_post_discovered:
		return false
	if camera.is_position_behind(post.global_position):
		return false
	# Close up: the ground under the cursor is inside the compound (with a margin
	# for the buildings standing above it). Far out: near its projected centre.
	var ground := _screen_ground_point(screen_position)
	var half: Vector2 = CommandPostModel.FOOTPRINT * 0.5 + Vector2(6.0, 6.0)
	if ground != Vector2.INF:
		var local := ground - Vector2(post.global_position.x, post.global_position.z)
		if absf(local.x) <= half.x and absf(local.y) <= half.y:
			return true
	var center := post.global_position + Vector3(0.0, 4.0, 0.0)
	return camera.unproject_position(center).distance_to(screen_position) <= clampf(camera_distance * 0.22, 20.0, 32.0)


func _select_command_post() -> void:
	var post: Node3D = command_post_views.get(HUMAN_PLAYER_ID)
	if post == null or not is_instance_valid(post) or not post.visible:
		return
	_clear_selection()
	command_post_selected = true
	var ring := post.get_node_or_null("SelectionRing") as Node3D
	if ring != null:
		ring.visible = true
	if strategic_command_view != null and not strategic_command_view.visible:
		_toggle_strategic_command()


func _deselect_command_post() -> void:
	command_post_selected = false
	command_post_notice = ""
	var post: Node3D = command_post_views.get(HUMAN_PLAYER_ID)
	if post != null and is_instance_valid(post):
		var ring := post.get_node_or_null("SelectionRing") as Node3D
		if ring != null:
			ring.visible = false


# Name, integrity, role and inbound logistics for the command post panel. All
# numbers come from native state; the post's own unit record is a placeholder
# type, so its name is fixed here rather than read from the prototype table.
func _command_post_snapshot(faction_id: int, with_logistics := true) -> Dictionary:
	var post_id := int(commander_ids.get(faction_id, -1))
	var health: Array = extension.call("get_unit_health", post_id) if post_id > 0 else []
	var snapshot := {
		"id": post_id,
		"name": "FORWARD COMMAND POST" if faction_id == HUMAN_PLAYER_ID else "ENEMY COMMAND POST",
		"role": "HEADQUARTERS  ·  ITS LOSS ENDS THE MATCH" if faction_id == HUMAN_PLAYER_ID else "HEADQUARTERS  ·  DESTROY IT TO WIN",
		"health": float(health[0]) if health.size() >= 2 else -1.0,
		"max_health": float(health[1]) if health.size() >= 2 else -1.0,
	}
	if faction_id != HUMAN_PLAYER_ID or not with_logistics:
		return snapshot
	var names := {}
	for entry in extension.call("get_build_catalog", HUMAN_PLAYER_ID):
		names[int(entry.get("type", -1))] = String(entry.get("name", "UNIT")).to_upper()
	var inbound: Array = []
	for force in extension.call("delivery_state", HUMAN_PLAYER_ID).get("inbound", []):
		inbound.append({
			"name": ("PARADROP " if bool(force.get("airdrop", false)) else "") + String(names.get(int(force.type), "UNIT")),
			"seconds": float(force.remaining_ms) / 1000.0,
		})
	var supply: Dictionary = extension.call("fuel_supply_state", HUMAN_PLAYER_ID)
	if supply.has("shipment_remaining_ms"):
		inbound.push_front({"name": "FUEL SHIPMENT  %.0f" % float(supply.get("shipment_load", 0.0)), "seconds": float(supply.shipment_remaining_ms) / 1000.0, "fuel": true})
	snapshot["inbound"] = inbound
	snapshot["fuel_load"] = float(supply.get("shipment_load", 0.0))
	snapshot["fuel_cost"] = float(supply.get("shipment_material_cost", 0.0))
	snapshot["fuel_inbound"] = supply.has("shipment_remaining_ms")
	snapshot["status"] = command_post_notice if not command_post_notice.is_empty() else _with_fuel_status(reinforcement_delivery_status)
	return snapshot


func _setup_command_post_panel() -> void:
	command_post_panel = CommandPostPanelScript.new()
	command_post_panel.name = "CommandPostPanel"
	command_post_panel.visible = false
	command_post_panel.open_strategic_command.connect(_toggle_strategic_command)
	command_post_panel.request_fuel.connect(_request_fuel_shipment)
	ui_root.add_child(command_post_panel)


func _sync_command_post_panel() -> void:
	if command_post_panel == null:
		return
	var post: Node3D = command_post_views.get(HUMAN_PLAYER_ID)
	if command_post_selected and (post == null or not is_instance_valid(post) or not post.visible or match_finished):
		_deselect_command_post()
	command_post_panel.visible = command_post_selected
	if command_post_selected:
		command_post_panel.set_state(_command_post_snapshot(HUMAN_PLAYER_ID))


func _update_hover_context(pointer := Vector2.INF) -> void:
	if pointer == Vector2.INF:
		pointer = get_viewport().get_mouse_position()
	var nearest_distance := clampf(camera_distance * 0.22, 20.0, 32.0)
	var next_context: Dictionary = command_hud.get_build_hover_context()
	if next_context.is_empty():
		for entity_id in entity_ids:
			if not _is_unit_shown(entity_id):
				continue
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
					"detail": "MATERIALS %.0f  //  TACTICAL INSTALLATION" % _structure_material_cost(structure_type),
					"accent": Color("#47d9ff"),
				}
		for site in fuel_depot_state.values():
			if not bool(site.discovered):
				continue
			var site_position: Vector2 = site.position
			var world_position := Vector3(site_position.x, _terrain_height_at(site_position.x, site_position.y), site_position.y)
			if camera.is_position_behind(world_position):
				continue
			var distance := camera.unproject_position(world_position).distance_to(pointer)
			if distance < nearest_distance:
				nearest_distance = distance
				var owner := int(site.known_owner)
				var allegiance: String = "FRIENDLY" if owner == HUMAN_PLAYER_ID else "HOSTILE" if owner >= 0 else "UNCLAIMED"
				next_context = {
					"key": "fuel:%d:%d:%d" % [int(site.id), owner, int(float(site.stock) / 50.0)],
					"title": "%s // %s" % [allegiance, site.name],
					"detail": "FUEL STOCK %.0f / %.0f  //  2 THEN RIGHT-CLICK TO CAPTURE" % [float(site.stock), float(site.capacity)],
					"accent": Color("#7ad99b") if owner == HUMAN_PLAYER_ID else Color("#ff6d65") if owner >= 0 else Color("#ffbd52"),
				}
	if next_context.is_empty() and match_started and power_grid_view != null and power_grid_view.active:
		var ground := _screen_ground_point(pointer)
		if ground != Vector2.INF:
			var meters_per_pixel := 2.0 * camera_distance * tan(deg_to_rad(camera.fov) * 0.5) / maxf(get_viewport().get_visible_rect().size.y, 1.0)
			next_context = power_grid_view.hover_context(ground, maxf(12.0, meters_per_pixel * 14.0))
	# Command posts: our own always, the enemy's only once discovered. Its
	# integrity shows only while it is in sight; otherwise the last fix.
	if next_context.is_empty() and match_started:
		for faction_id in [HUMAN_PLAYER_ID, 1]:
			if not _command_post_under_cursor(faction_id, pointer):
				continue
			var post := _command_post_snapshot(faction_id, false)
			var friendly: bool = faction_id == HUMAN_PLAYER_ID
			var in_sight: bool = friendly or seen_enemy_ids.has(int(post.id))
			var integrity := "INTEGRITY %.0f / %.0f" % [post.health, post.max_health] if in_sight and float(post.max_health) > 0.0 else "LAST KNOWN POSITION"
			next_context = {
				"key": "post:%d:%d:%s" % [faction_id, int(float(post.health) / 10.0), str(in_sight)],
				"title": "%s // %s" % ["FRIENDLY" if friendly else "HOSTILE", post.name],
				"detail": "%s  //  %s" % [integrity, "CLICK TO OPEN COMMAND POST" if friendly else "CTRL RIGHT-CLICK TO ATTACK"],
				"accent": Color("#7ad99b") if friendly else Color("#ff6d65"),
			}
			break
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
	if order_overlay_renderer != null:
		order_overlay_renderer.clear_order()
	_set_selected_structure(null)
	_deselect_command_post()


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
	if selected and command_post_selected:
		_deselect_command_post()
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
	if not selected and order_overlay_renderer != null and command_feedback_pending_ids.has(entity_id):
		order_overlay_renderer.clear_order()


func _set_instance_color(entity_id: int, color: Color) -> void:
	var instance_index: int = entity_to_instance.get(entity_id, -1)
	if demo_mode:
		_set_demo_unit_color(entity_id, color)
	elif instance_index >= 0:
		unit_multimesh.set_instance_color(instance_index, color)


func _terrain_height_at(world_x: float, world_z: float) -> float:
	if terrain_heights.is_empty():
		return 0.4
	var grid_x := clampf((world_x / terrain_world_width + 0.5) * float(TERRAIN_SAMPLE_WIDTH - 1), 0.0, float(TERRAIN_SAMPLE_WIDTH - 1))
	var grid_z := clampf((world_z / terrain_world_height + 0.5) * float(TERRAIN_SAMPLE_HEIGHT - 1), 0.0, float(TERRAIN_SAMPLE_HEIGHT - 1))
	var x0 := mini(floori(grid_x), TERRAIN_SAMPLE_WIDTH - 2)
	var z0 := mini(floori(grid_z), TERRAIN_SAMPLE_HEIGHT - 2)
	var tx := grid_x - float(x0)
	var tz := grid_z - float(z0)
	var h00 := _terrain_vertex_height(x0, z0)
	var h10 := _terrain_vertex_height(x0 + 1, z0)
	var h01 := _terrain_vertex_height(x0, z0 + 1)
	var h11 := _terrain_vertex_height(x0 + 1, z0 + 1)
	# Match generate_terrain_mesh()'s two triangles, rather than bilinearly
	# smoothing across their diagonal and drifting away from the rendered mesh.
	if tx + tz <= 1.0:
		return h00 + tx * (h10 - h00) + tz * (h01 - h00)
	return h11 + (1.0 - tz) * (h10 - h11) + (1.0 - tx) * (h01 - h11)


func _terrain_vertex_height(sample_x: int, sample_z: int) -> float:
	var vertex_world_x := (float(sample_x) / float(TERRAIN_SAMPLE_WIDTH - 1) - 0.5) * terrain_world_width
	var vertex_world_z := (float(sample_z) / float(TERRAIN_SAMPLE_HEIGHT - 1) - 0.5) * terrain_world_height
	return HeightMap.presentation_height(terrain_heights[sample_z * TERRAIN_SAMPLE_WIDTH + sample_x], vertex_world_x, terrain_world_width, vertex_world_z)


func _apply_terrain_masks() -> void:
	var masks := HeightMap.load_masks(TERRAIN_MASKS_PATH)
	territory_material.set_shader_parameter("world_size", terrain_world_width)
	territory_material.set_shader_parameter("masks_enabled", not masks.is_empty())
	if masks.is_empty():
		return
	territory_material.set_shader_parameter("terrain_masks", HeightMap.masks_texture(masks))
	_ensure_open_sea()
	if terrain_vegetation == null:
		terrain_vegetation = TerrainVegetation.new()
		terrain_vegetation.name = "TerrainVegetation"
		add_child(terrain_vegetation)
	var spawn: Array = scenario_definition.get("player", {}).get("spawn", [0.0, 0.0])
	terrain_vegetation.rebuild(masks, terrain_world_width, terrain_world_height, _terrain_height_at, Vector2(float(spawn[0]), float(spawn[1])))


# The heightfield stops at the theater edge. A ring around it, never beneath it,
# carries the same water to the horizon without depth conflicts over the land.
func _ensure_open_sea() -> void:
	if has_node("OpenSea"):
		return
	var half := Vector2(terrain_world_width, terrain_world_height) * 0.5
	var reach := camera.far * 1.5
	var vertices := PackedVector3Array()
	var uvs := PackedVector2Array()
	var indices := PackedInt32Array()
	var bands := [
		[-reach, -reach, reach, -half.y], [-reach, half.y, reach, reach],
		[-reach, -half.y, -half.x, half.y], [half.x, -half.y, reach, half.y],
	]
	for band in bands:
		var first := vertices.size()
		for corner in [Vector2(band[0], band[1]), Vector2(band[2], band[1]), Vector2(band[0], band[3]), Vector2(band[2], band[3])]:
			vertices.append(Vector3(corner.x, -0.7, corner.y))
			uvs.append(Vector2(corner.x / terrain_world_width + 0.5, corner.y / terrain_world_height + 0.5))
		indices.append_array(PackedInt32Array([first, first + 2, first + 1, first + 1, first + 2, first + 3]))
	var normals := PackedVector3Array()
	var colors := PackedColorArray()
	for _vertex in vertices:
		normals.append(Vector3.UP)
		colors.append(Color(0.0, 0.0, 0.0, 1.0))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors
	arrays[Mesh.ARRAY_TEX_UV] = uvs
	arrays[Mesh.ARRAY_INDEX] = indices
	var ring := ArrayMesh.new()
	ring.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var sea := MeshInstance3D.new()
	sea.name = "OpenSea"
	sea.mesh = ring
	sea.material_override = territory_material
	sea.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(sea)


func _rebuild_terrain_trees() -> void:
	for tree in forest_landmarks.get_children():
		tree.queue_free()
	_clear_tree_variant_views()

	if prototype_visuals_enabled:
		_rebuild_terrain_trees_prototype()
	else:
		_rebuild_terrain_trees_procedural()


func _rebuild_terrain_trees_procedural() -> void:
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
	tactical_tree_views.append(terrain_trees)
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
	strategic_tree_views.append(strategic_trees)
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


func _rebuild_terrain_trees_prototype() -> void:
	var registry := VisualDefinitionRegistry.new()
	if not registry.load_definitions():
		_rebuild_terrain_trees_procedural()
		return

	var visual_id := "visual.nature.tree.prototype"
	var definition := registry.resolve(visual_id)
	if definition.is_empty() or bool(definition.get("fallback", false)):
		_rebuild_terrain_trees_procedural()
		return

	var tactical_mesh := _tree_mesh_from_resource(registry.load_model(visual_id))
	var strategic_path := String(definition.get("strategic_model_path", ""))
	var strategic_mesh := _tree_mesh_from_resource(load(strategic_path)) if not strategic_path.is_empty() else tactical_mesh
	if tactical_mesh == null or strategic_mesh == null:
		push_warning("Imported tree visual failed to load; retaining procedural forest")
		_rebuild_terrain_trees_procedural()
		return

	var tactical_paths: Array = definition.get("tactical_model_paths", [String(definition.get("model_path", ""))])
	var share_primary_material := bool(definition.get("share_primary_tree_material", true))
	var tactical_meshes: Array[Mesh] = []
	for path_value in tactical_paths:
		var path := String(path_value)
		var loaded_mesh := tactical_mesh if path == String(definition.get("model_path", "")) else _tree_mesh_from_resource(load(path))
		if loaded_mesh == null:
			push_warning("Imported tree variant failed to load: %s" % path)
			_rebuild_terrain_trees_procedural()
			return
		var variant_mesh := loaded_mesh.duplicate() as Mesh
		if share_primary_material:
			for surface in range(mini(variant_mesh.get_surface_count(), tactical_mesh.get_surface_count())):
				variant_mesh.surface_set_material(surface, tactical_mesh.surface_get_material(surface))
		tactical_meshes.append(variant_mesh)
	if tactical_meshes.is_empty():
		_rebuild_terrain_trees_procedural()
		return

	var transforms_by_variant: Array[Array] = []
	for ignored in tactical_meshes:
		transforms_by_variant.append([])
	var rng := RandomNumberGenerator.new()
	rng.seed = 640640
	forest_tree_positions = _clustered_forest_positions(rng)
	forest_tree_scales.clear()
	forest_tree_root_depths.clear()
	var variant_counts: Array[int] = []
	if tactical_meshes.size() == 3:
		variant_counts.append_array([440, 440, 80])
	else:
		for variant_index in range(tactical_meshes.size()):
			variant_counts.append(int(TREE_INSTANCE_TARGET / tactical_meshes.size()))
		for remainder_index in range(TREE_INSTANCE_TARGET % tactical_meshes.size()):
			variant_counts[remainder_index] += 1
	forest_tree_variant_indices.clear()
	for variant_index in range(variant_counts.size()):
		for ignored in range(variant_counts[variant_index]):
			forest_tree_variant_indices.append(variant_index)
	# Keep the authored population ratio while distributing every oak form through
	# every grove.  Sequential assignment made whole groves read as one species.
	for index in range(forest_tree_variant_indices.size() - 1, 0, -1):
		var swap_index := rng.randi_range(0, index)
		var swapped_variant := forest_tree_variant_indices[index]
		forest_tree_variant_indices[index] = forest_tree_variant_indices[swap_index]
		forest_tree_variant_indices[swap_index] = swapped_variant
	for index in range(forest_tree_positions.size()):
		var variant_index := forest_tree_variant_indices[index]
		var position := forest_tree_positions[index]
		var scale_profiles: Array[Vector2] = [Vector2(0.75, 1.10), Vector2(0.70, 1.25), Vector2(0.80, 1.35)]
		var scale_profile: Vector2 = scale_profiles[variant_index]
		var scale_xy := rng.randf_range(scale_profile.x, scale_profile.y)
		var scale_z := rng.randf_range(scale_profile.x, scale_profile.y)
		forest_tree_scales.append(Vector2(scale_xy, scale_z))
		# Ground the lowest actual mesh vertex, rather than assuming a GLB's
		# origin is at its root.  A small 8 cm embed keeps roots visually seated.
		var local_root_y := tactical_meshes[variant_index].get_aabb().position.y
		var terrain_height := _terrain_height_at(position.x, position.y)
		var root_y := terrain_height - 0.08 - local_root_y * scale_z
		forest_tree_root_depths.append(root_y + local_root_y * scale_z - terrain_height)
		var rotation := Basis(Vector3.UP, rng.randf_range(0.0, TAU))
		var scale := Basis.from_scale(Vector3(scale_xy, scale_z, scale_xy))
		var transform := Transform3D(rotation * scale, Vector3(position.x, root_y, position.y))
		transforms_by_variant[variant_index].append(transform)

	for variant_index in range(tactical_meshes.size()):
		var tactical_view := terrain_trees if variant_index == 0 else _new_tree_variant_view("TerrainOakVariant%d" % variant_index)
		var tactical_multimesh := _tree_multimesh(tactical_meshes[variant_index], transforms_by_variant[variant_index])
		tactical_view.multimesh = tactical_multimesh
		tactical_view.custom_aabb = _forest_tree_aabb()
		tactical_view.visible = true
		tactical_tree_views.append(tactical_view)

		var strategic_view := strategic_trees if variant_index == 0 else _new_tree_variant_view("StrategicOakVariant%d" % variant_index)
		var strategic_transforms: Array = []
		for source_index in range(transforms_by_variant[variant_index].size()):
			if source_index % 2 == 0:
				var transform: Transform3D = transforms_by_variant[variant_index][source_index]
				strategic_transforms.append(Transform3D(Basis(Vector3.UP, transform.basis.get_euler().y).scaled(transform.basis.get_scale() * 0.72), Vector3(transform.origin.x, transform.origin.y, transform.origin.z)))
		strategic_view.multimesh = _tree_multimesh(strategic_mesh, strategic_transforms)
		strategic_view.custom_aabb = _forest_tree_aabb()
		strategic_view.visible = false
		strategic_tree_views.append(strategic_view)

	for side in [-1.0, 1.0]:
		for row in range(4):
			for column in range(3):
				var landmark := Marker3D.new()
				landmark.position = Vector3(
					side * (terrain_world_width * 0.31 + float(column) * 12.0),
					_terrain_height_at(side * (terrain_world_width * 0.31 + float(column) * 12.0), -120.0 + float(row) * 78.0) + 7.0,
					-120.0 + float(row) * 78.0
				)
				forest_landmarks.add_child(landmark)
	_sync_tree_lod()


func _clustered_forest_positions(rng: RandomNumberGenerator) -> Array[Vector2]:
	var positions: Array[Vector2] = []
	forest_cluster_centers.clear()
	for cluster_index in range(TREE_CLUSTER_COUNT):
		var side := -1.0 if cluster_index % 2 == 0 else 1.0
		var center := Vector2(side * rng.randf_range(terrain_world_width * 0.29, terrain_world_width * 0.43), rng.randf_range(-terrain_world_height * 0.37, terrain_world_height * 0.37))
		forest_cluster_centers.append(center)
		for tree_index in range(96):
			# A grove should read as a forest from the tactical camera, rather than a
			# collection of evenly scattered landmark trees.  The square-root term
			# retains a natural falloff while the compact radius keeps the 96 trees
			# visibly clustered.
			var radius := sqrt(rng.randf()) * rng.randf_range(80.0, 360.0)
			var position := center + Vector2(cos(rng.randf_range(0.0, TAU)), sin(rng.randf_range(0.0, TAU))) * radius
			position.x = clampf(position.x, -terrain_world_width * 0.46, terrain_world_width * 0.46)
			position.y = clampf(position.y, -terrain_world_height * 0.46, terrain_world_height * 0.46)
			if absf(position.x) >= terrain_world_width * 0.24:
				positions.append(position)
	while positions.size() < TREE_INSTANCE_TARGET:
		var position := Vector2(rng.randf_range(-terrain_world_width * 0.46, terrain_world_width * 0.46), rng.randf_range(-terrain_world_height * 0.46, terrain_world_height * 0.46))
		if absf(position.x) >= terrain_world_width * 0.24:
			positions.append(position)
	return positions.slice(0, TREE_INSTANCE_TARGET)


func _tree_multimesh(mesh: Mesh, transforms: Array) -> MultiMesh:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = mesh
	multimesh.instance_count = transforms.size()
	for index in range(transforms.size()):
		multimesh.set_instance_transform(index, transforms[index])
	return multimesh


func _forest_tree_aabb() -> AABB:
	return AABB(Vector3(-terrain_world_width * 0.5, -4.0, -terrain_world_height * 0.5), Vector3(terrain_world_width, 64.0, terrain_world_height))


func _new_tree_variant_view(view_name: String) -> MultiMeshInstance3D:
	var view := MultiMeshInstance3D.new()
	view.name = view_name
	add_child(view)
	return view


func _clear_tree_variant_views() -> void:
	for view in tactical_tree_views:
		if view != terrain_trees and is_instance_valid(view):
			view.queue_free()
	for view in strategic_tree_views:
		if view != strategic_trees and is_instance_valid(view):
			view.queue_free()
	tactical_tree_views.clear()
	strategic_tree_views.clear()


func _tree_mesh_from_resource(resource: Resource) -> Mesh:
	if resource is Mesh:
		return resource as Mesh
	if not resource is PackedScene:
		return null
	var instance := (resource as PackedScene).instantiate()
	var mesh_nodes := instance.find_children("*", "MeshInstance3D", true, false)
	for node in mesh_nodes:
		var mesh_instance := node as MeshInstance3D
		if mesh_instance != null and mesh_instance.mesh != null:
			instance.queue_free()
			return mesh_instance.mesh
	instance.queue_free()
	return null


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
	for view in tactical_tree_views:
		view.visible = not strategic
	for view in strategic_tree_views:
		view.visible = strategic


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


## Sends selected units to the clicked ground. `queued` (Shift + right click)
## appends the spot as a waypoint after each unit's current orders; otherwise
## it replaces them.
func _issue_move_order(screen_position: Vector2, queued := false) -> void:
	# The command post is a fixed installation: an empty selection used to fall
	# back to it (commander_ids holds the post), which drove the post's native
	# entity away from its drawn compound. Nothing selected now means no order.
	if selected_ids.is_empty():
		_refuse_command_post_order()
		return

	var target := _screen_to_world(screen_position)
	# A queued arrow starts where the units' queues currently end.
	var queue_end := _queued_order_end() if queued else Vector2.INF
	var accepted_count: int = extension.call(
		"issue_move_commands",
		selected_ids,
		HUMAN_PLAYER_ID,
		target.x,
		target.y,
		UNIT_SPACING,
		queued
	)
	if accepted_count != selected_ids.size():
		push_warning("Move order rejected: accepted=%d/%d entity=%d target=(%.6f,%.6f)" % [accepted_count, selected_ids.size(), selected_ids[0], target.x, target.y])
		return
	_show_command_feedback(target, queue_end)
	# Held Shift shows the new leg as soon as the simulation accepts it.
	unit_route_refresh_seconds = 0.0


## Mean of the selected units' last queued destinations, or INF when none of
## them has a move order.
func _queued_order_end() -> Vector2:
	var waypoints: Array = extension.call("get_unit_waypoints", selected_ids, HUMAN_PLAYER_ID)
	var total := Vector2.ZERO
	var count := 0
	for unit_waypoints in waypoints:
		var points := unit_waypoints as PackedVector2Array
		if not points.is_empty():
			total += points[points.size() - 1]
			count += 1
	return total / float(count) if count > 0 else Vector2.INF


func _show_command_feedback(target: Vector2, from := Vector2.INF) -> void:
	_order_overlay_renderer_setup()
	var origin := Vector2.ZERO
	var count := 0
	for entity_id in selected_ids:
		var unit_position: Vector3 = _entity_world_position(entity_id)
		origin += Vector2(unit_position.x, unit_position.z)
		count += 1
	if count > 0:
		origin /= float(count)
	if from != Vector2.INF:
		origin = from
	order_overlay_renderer.show_move_order(origin, target)
	command_feedback_marker = order_overlay_renderer.get("destination_marker").get_child(0)
	command_feedback_marker.visible = true
	command_feedback_pending_ids = selected_ids.duplicate()
	command_feedback_waiting_for_target = true

func _order_overlay_renderer_setup() -> void:
	if order_overlay_renderer != null:
		return
	order_overlay_renderer = OrderOverlayRendererScript.new()
	order_overlay_renderer.name = "OrderVisualizationRoot"
	order_overlay_renderer.set_terrain_height_sampler(func(world_x: float, world_z: float): return _terrain_height_at(world_x, world_z))
	add_child(order_overlay_renderer)
	order_overlay_renderer.set_camera_distance(camera_distance)


func _update_command_feedback(_delta: float) -> void:
	if order_overlay_renderer == null or not order_overlay_renderer.visible:
		return
	var pending := false
	for entity_id in command_feedback_pending_ids:
		if bool(extension.call("has_unit_move_target", entity_id)):
			pending = true
			break
	if pending:
		command_feedback_waiting_for_target = false
		return
	if command_feedback_waiting_for_target:
		return
	if not pending:
		order_overlay_renderer.clear_order()
		if command_feedback_marker != null:
			command_feedback_marker.visible = false
		command_feedback_pending_ids = PackedInt32Array()


## While Shift is held, draw each selected unit's remaining route as the
## native movement system will drive it, turns included. Release hides it.
func _update_unit_routes(delta: float) -> void:
	var showing := Input.is_key_pressed(KEY_SHIFT) and not selected_ids.is_empty()
	showing = showing and extension != null and match_started and not match_finished
	if not showing:
		if unit_route_renderer != null and unit_route_renderer.visible:
			unit_route_renderer.clear_routes()
		unit_route_refresh_seconds = 0.0
		return
	if unit_route_renderer == null:
		unit_route_renderer = UnitRouteRendererScript.new()
		unit_route_renderer.name = "UnitRouteRoot"
		unit_route_renderer.set_terrain_height_sampler(func(world_x: float, world_z: float): return _terrain_height_at(world_x, world_z))
		add_child(unit_route_renderer)
	unit_route_renderer.set_camera_distance(camera_distance)
	unit_route_refresh_seconds -= delta
	if unit_route_refresh_seconds > 0.0:
		return
	unit_route_refresh_seconds = UNIT_ROUTE_REFRESH_SECONDS
	var routes: Array = extension.call("get_unit_routes", selected_ids, HUMAN_PLAYER_ID)
	var waypoints: Array = extension.call("get_unit_waypoints", selected_ids, HUMAN_PLAYER_ID)
	unit_route_renderer.show_routes(routes, waypoints)


func _refuse_command_post_order() -> void:
	if not command_post_selected:
		return
	command_post_notice = "COMMAND POST IS A FIXED INSTALLATION // SELECT UNITS TO GIVE ORDERS"
	_announce_intel(command_post_notice)
	if ui_audio != null:
		ui_audio.play_cue("order_refused")
	_update_hud()


func _issue_stop_order() -> void:
	if selected_ids.is_empty():
		return
	var accepted_count: int = extension.call("issue_stop_commands", selected_ids, HUMAN_PLAYER_ID)
	if accepted_count != selected_ids.size():
		push_warning("Stop order rejected by authoritative command validation")
		return
	command_feedback_pending_ids = PackedInt32Array()
	command_feedback_waiting_for_target = false
	if command_feedback_marker != null:
		command_feedback_marker.visible = false
	if order_overlay_renderer != null:
		order_overlay_renderer.clear_order()


func _order_fob(screen_position: Vector2) -> void:
	var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
	if selected_ids.is_empty() or commander_id < 0:
		fob_build_mode = false
		return
	var target := _screen_ground_point(screen_position)
	if target == Vector2.INF:
		return
	var accepted := int(extension.call("issue_install_commands", selected_ids, HUMAN_PLAYER_ID, target.x, target.y, 1))
	if accepted == selected_ids.size():
		fob_build_target = target
		fob_was_constructing = false
		fob_completion_notification = ""
	else:
		push_warning("FOB placement rejected: select a unit with CONSTRUCT_FOB capability")
	fob_build_mode = false


func _queue_commander_unit(unit_type: int, target: Vector2 = Vector2.ZERO) -> void:
	if selected_ids.has(_player_engineer_id()):
		_order_commander_unit(unit_type, target)


# Strategic Command orders come through here too, whatever is selected.
func _order_commander_unit(unit_type: int, target: Vector2) -> void:
	var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
	if commander_id < 0:
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
	target = _resolve_build_target(build_type, target)
	if target == Vector2.INF:
		push_warning("Structure placement rejected at the cursor location")
		return
	var commander_position := _native_unit_position(commander_id)
	var direction := (target - commander_position).normalized()
	if direction.length_squared() < 0.001:
		direction = Vector2.RIGHT
	# Stop beyond the scaffold footprint rather than pathing the commander into
	# the finished building. The original target remains the construction site.
	var approach_target := target - direction * _build_standoff(build_type, direction)
	# The authoritative move validator rejects zero formation spacing. A single
	# commander still uses the normal RTS spacing contract used by manual moves.
	var accepted := int(extension.call("issue_move_commands", PackedInt32Array([commander_id]), HUMAN_PLAYER_ID, approach_target.x, approach_target.y, UNIT_SPACING))
	if accepted != 1:
		push_warning("Field Engineer could not reach the build location")
		return
	pending_build_order = {"type": build_type, "target": target, "approach": approach_target}

## How far from the site centre the engineer stops, along `direction` (site
## toward engineer, negated): on clear ground just past the structure's footprint.
func _build_standoff(build_type: int, direction: Vector2) -> float:
	if build_type < 100:
		return 7.0
	var site: Dictionary = extension.call("get_structure_site", build_type - 100, 0.0, 0.0)
	var half := Vector2(float(site.get("clearance_x", 0.0)), float(site.get("clearance_y", 0.0)))
	if half.x <= 0.0 or half.y <= 0.0:
		return 7.0
	var edge := INF
	if absf(direction.x) > 0.0001:
		edge = minf(edge, half.x / absf(direction.x))
	if absf(direction.y) > 0.0001:
		edge = minf(edge, half.y / absf(direction.y))
	return edge + BUILD_STANDOFF_MARGIN


func _process_pending_build_order() -> void:
	if pending_build_order.is_empty():
		return
	var commander_id := _player_engineer_id()
	var target: Vector2 = pending_build_order.target
	var approach: Vector2 = pending_build_order.approach
	var commander_position := _native_unit_position(commander_id)
	var dx := commander_position.x - approach.x
	var dz := commander_position.y - approach.y
	if dx * dx + dz * dz > 16.0:
		return
	var build_type := int(pending_build_order.type)
	if build_type >= 100:
		# The order was given with the engineer selected; selecting other units
		# while it drives there must not drop it.
		if _queue_commander_structure(build_type - 100, target, false):
			pending_completed_structures.append({"type": build_type - 100, "target": target})
	else:
		_queue_commander_unit(build_type, target)
	pending_build_order.clear()
	_advance_build_queue()


## Shift+click: the first placement goes straight to the engineer, later ones
## wait their turn. Returns whether the placement was taken.
func _queue_build_placement(build_type: int, target: Vector2) -> bool:
	if pending_build_order.is_empty() and queued_build_orders.is_empty():
		_order_commander_to_build(build_type, target)
		_sync_queued_build_markers()
		return not pending_build_order.is_empty()
	var resolved := _resolve_build_target(build_type, target)
	if resolved == Vector2.INF:
		push_warning("Structure placement rejected at the cursor location")
		return false
	queued_build_orders.append({"type": build_type, "target": resolved})
	_sync_queued_build_markers()
	return true


# Send the engineer to the next queued placement. One the engineer cannot
# reach is skipped, so a bad entry never stalls the rest of the queue.
func _advance_build_queue() -> void:
	while pending_build_order.is_empty() and not queued_build_orders.is_empty():
		var next: Dictionary = queued_build_orders.pop_front()
		_order_commander_to_build(int(next.type), next.target)
	_sync_queued_build_markers()


## One translucent footprint with its place in the queue over every build the
## engineer has been ordered to make and has not yet handed to the native queue.
func _sync_queued_build_markers() -> void:
	if queued_build_markers == null:
		queued_build_markers = Node3D.new()
		queued_build_markers.name = "QueuedBuildMarkers"
		add_child(queued_build_markers)
	for child in queued_build_markers.get_children():
		queued_build_markers.remove_child(child)
		child.queue_free()
	var orders: Array[Dictionary] = []
	if not pending_build_order.is_empty():
		orders.append(pending_build_order)
	orders.append_array(queued_build_orders)
	if orders.is_empty():
		return
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = Color("#47d9ff", 0.30)
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	for index in range(orders.size()):
		var order: Dictionary = orders[index]
		var build_type := int(order.type)
		var target: Vector2 = order.target
		var marker := _make_build_preview(build_type, material)
		marker.name = "QueuedBuild_%d" % (index + 1)
		marker.set_meta("build_type", build_type)
		marker.position = Vector3(target.x, _terrain_height_at(target.x, target.y) + 0.08, target.y)
		var label := Label3D.new()
		label.name = "Order"
		label.text = "%d  %s" % [index + 1, _structure_display_name(build_type - 100) if build_type >= 100 else "BUILD"]
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.pixel_size = 0.12
		label.font_size = 30
		label.outline_size = 4
		label.modulate = Color("#47d9ff")
		label.position.y = 8.0
		marker.add_child(label)
		queued_build_markers.add_child(marker)


func _native_unit_position(entity_id: int) -> Vector2:
	var position: PackedFloat32Array = extension.call("get_unit_position", entity_id)
	if position.size() != 2:
		push_error("Native unit position snapshot is invalid for entity %d" % entity_id)
		return Vector2.ZERO
	return Vector2(position[0], position[1])


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

func _screen_ground_point(screen_position: Vector2) -> Vector2:
	if camera.project_ray_normal(screen_position).y >= -0.00001:
		return Vector2.INF
	return _screen_to_world(screen_position)


func _click_tolerance_meters() -> float:
	if camera_distance < APPROXIMATE_CLICK_MIN_CAMERA_DISTANCE:
		return 0.0
	var meters_per_pixel := 2.0 * camera_distance * tan(deg_to_rad(camera.fov) * 0.5) / get_viewport().get_visible_rect().size.y
	return meters_per_pixel * APPROXIMATE_CLICK_PIXELS


func _resolve_build_target(build_type: int, target: Vector2) -> Vector2:
	if build_type < 100:
		return target
	var structure_type := build_type - 100
	# An occupied site stays under the cursor and turns the ghost red. Do not
	# search for a nearby substitute when the player points at a structure.
	if _build_site_occupied(structure_type, target):
		return Vector2.INF
	if bool(extension.call("validate_structure_placement", structure_type, target.x, target.y)):
		return target
	var tolerance := _click_tolerance_meters()
	if tolerance <= 0.0:
		return Vector2.INF
	for ring in range(1, 5):
		var radius := tolerance * float(ring) / 4.0
		for step in range(16):
			var candidate := target + Vector2.RIGHT.rotated(TAU * float(step) / 16.0) * radius
			if not _build_site_occupied(structure_type, candidate) and bool(extension.call("validate_structure_placement", structure_type, candidate.x, candidate.y)):
				return candidate
	return Vector2.INF


func _build_site_occupied(structure_type: int, target: Vector2) -> bool:
	var site: Dictionary = extension.call("get_structure_site", structure_type, target.x, target.y)
	if bool(site.get("claimed", false)):
		return true
	var half := Vector2(float(site.get("footprint_x", 0.0)), float(site.get("footprint_y", 0.0))) * 0.5
	var margin := float(site.get("claim_margin", 0.0))
	var orders: Array[Dictionary] = []
	if not pending_build_order.is_empty():
		orders.append(pending_build_order)
	orders.append_array(queued_build_orders)
	for order in orders:
		var other_type := int(order.get("type", -1))
		if other_type < 100:
			continue
		var other_target: Vector2 = order.target
		var other: Dictionary = extension.call("get_structure_site", other_type - 100, other_target.x, other_target.y)
		var other_half := Vector2(float(other.get("footprint_x", 0.0)), float(other.get("footprint_y", 0.0))) * 0.5
		if absf(target.x - other_target.x) < half.x + other_half.x + margin and absf(target.y - other_target.y) < half.y + other_half.y + margin:
			return true
	return false


func _begin_build_placement(build_type: int) -> void:
	var commander_id := _player_engineer_id()
	if commander_id < 0 or not selected_ids.has(commander_id):
		push_warning("Select the Field Engineer before placing a build")
		return
	build_mode_type = build_type
	selecting = false
	selection_rect.visible = false


func _cancel_tactical_modes() -> void:
	recon_targeting = false
	paradrop_targeting_type = -1
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
	var ui_scale := ResponsiveUiScript.layout_scale(size)
	var logical := pos / ui_scale
	var origin := ResponsiveUiScript.centered_origin(size, ResponsiveUiScript.PANEL_MAX_WIDTH)
	if logical.x < origin.x or logical.x > origin.x + 540.0 or logical.y < origin.y + 30.0 or logical.y > origin.y + 156.0: return -1
	var col := int(clampf((logical.x - origin.x - 8.0) / 132.0, 0.0, 3.0))
	if logical.y < origin.y + 70.0:
		return _build_unit_type_at_index(col)
	if logical.y < origin.y + 108.0:
		return _build_unit_type_at_index(col + 4)
	if logical.y >= origin.y + 124.0:
		return 100 + col
	return -1


func _hotkey_setup_from_manager() -> void:
	if hotkey_manager == null:
		hotkey_manager = preload("res://hotkey_manager.gd").new()
		hotkey_manager.name = "HotkeyManager"
		add_child(hotkey_manager)
	if hotkey_display == null:
		var ui_root := get_node_or_null("HUD/UIRoot")
		if ui_root != null:
			hotkey_display = preload("res://hotkey_display.gd").new()
			hotkey_display.name = "HotkeyDisplay"
			ui_root.add_child(hotkey_display)
	if hotkey_display != null:
		hotkey_display.hotkey_manager = hotkey_manager


func _handle_hotkey(keycode: Key, event: InputEventKey) -> bool:
	if not is_instance_valid(hotkey_manager):
		return false

	var key_name := "KEY_" + OS.get_keycode_string(keycode).to_upper().replace(" ", "_")
	var action: String = String(hotkey_manager.get_action(key_name))

	match action:
		"quit_or_cancel":
			if build_mode_type >= 0 or fob_build_mode or material_order_mode != 0:
				_cancel_tactical_modes()
				return true
			_show_exit_confirm()
			return true
		"toggle_debug":
			debug_panel.visible = not debug_panel.visible
			return true
		"free_camera":
			if not event.echo:
				free_float_camera = event.pressed
				if free_float_camera:
					free_camera_yaw = 0.0
					free_camera_pitch = 38.0
			return true
		"stop_order":
			_issue_stop_order()
			return true
		"reinforcement_select":
			if reinforcement_delivery_enabled:
				reinforcement_delivery_selecting = true
				reinforcement_delivery_status = "SELECT DELIVERY ZONE // LEFT-CLICK THE CYAN AREA"
				return true
		"fuel_shipment_request":
			_request_fuel_shipment()
			return true
		"reinforcement_request":
			if reinforcement_delivery_enabled:
				if bool(extension.call("reinforcement_delivery_request", HUMAN_PLAYER_ID)):
					reinforcement_delivery_status = "TRANSPORT INBOUND"
				else:
					reinforcement_delivery_status = String(extension.call("reinforcement_delivery_state").get("reason", "REQUEST REJECTED"))
				return true
		"fob_build":
			fob_build_mode = selected_ids.size() > 0
			material_order_mode = 0
			_update_hud()
			return true
		"build_structure_1":
			_begin_build_placement(100)
			return true
		"build_structure_2":
			_begin_build_placement(101)
			return true
		"build_structure_3":
			_begin_build_placement(103)
			return true
		"build_airfield":
			_begin_build_placement(102)
			return true
		"road_build":
			_begin_road_placement()
			return true
		"material_claim":
			_set_material_order_mode(1)
			return true
		"material_demolish":
			_set_material_order_mode(2)
			return true

	return false


# The Field Engineer builds structures and earthworks only. Forces are ordered
# from Strategic Command and flown in, so the engineer's blueprints list no units.
func _build_unit_catalog() -> Array:
	return []


func _build_unit_type_at_index(index: int) -> int:
	var units := _build_unit_catalog()
	if index < 0 or index >= units.size():
		return -1
	return int(units[index].get("type", -1))

func _make_build_ghost(build_type: int) -> Node3D:
	build_ghost_material = _make_agent_orange_ghost_material()
	return _make_build_preview(build_type, build_ghost_material)


# Placement and construction share geometry; only their materials differ.
func _make_build_preview(build_type: int, material: Material) -> Node3D:
	var root := Node3D.new()
	var structure_type := build_type - 100 if build_type >= 100 else -1
	match structure_type:
		0:
			_add_build_ghost_box(root, Vector3(5.0, 1.6, 5.0), Vector3(0.0, 0.8, 0.0))
		1:
			_add_build_ghost_cylinder(root, 0.35, 8.0, Vector3(0.0, 4.0, 0.0))
		2:
			# Native placement reserves the whole 250 x 750 m site, centred on
			# the click. Show that centre and boundary around the offset runway.
			_add_build_ghost_box(root, Vector3(250.0, 0.2, 2.0), Vector3(0.0, 0.5, -374.0))
			_add_build_ghost_box(root, Vector3(250.0, 0.2, 2.0), Vector3(0.0, 0.5, 374.0))
			_add_build_ghost_box(root, Vector3(2.0, 0.2, 750.0), Vector3(-124.0, 0.5, 0.0))
			_add_build_ghost_box(root, Vector3(2.0, 0.2, 750.0), Vector3(124.0, 0.5, 0.0))
			# Match the completed airfield's paved areas and buildings.
			for piece in AIRFIELD_PAVING:
				_add_build_ghost_box(root, Vector3(piece.size.x, 0.4, piece.size.y), Vector3(piece.center.x, 0.2, piece.center.y))
			_add_build_ghost_box(root, AIRFIELD_HANGAR_SIZE, AIRFIELD_HANGAR_POSITION)
			_add_build_ghost_cylinder(root, 7.0, 20.0, AIRFIELD_TOWER_POSITION, 5.0)
		3:
			_add_build_ghost_cylinder(root, 0.3, 15.0, Vector3(0.0, 7.5, 0.0))
		_:
			# Existing buildable ground units retain a compact volume preview.
			_add_build_ghost_box(root, Vector3(5.0, 2.0, 5.0), Vector3(0.0, 1.0, 0.0))
	for child in root.get_children():
		(child as MeshInstance3D).material_override = material
	return root


func _make_agent_orange_ghost_material() -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode blend_mix, unshaded, cull_disabled, depth_draw_always;
uniform vec4 tint : source_color = vec4(1.0, 0.30, 0.06, 0.52);
varying vec3 ghost_local_position;
void vertex() {
    ghost_local_position = VERTEX;
}
void fragment() {
    // Each runway is authored long on local Z, so these bands cross the
    // runway width. fwidth softens the edges at tactical-camera distances.
    float phase = fract(ghost_local_position.z / 24.0);
    float feather = max(fwidth(phase), 0.012);
    float stripe = smoothstep(0.50 - feather, 0.50 + feather, phase);
    vec3 dark_orange = tint.rgb * 0.28;
    ALBEDO = mix(dark_orange, tint.rgb, stripe);
    EMISSION = ALBEDO * 0.34;
    ALPHA = tint.a;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_shader_parameter("tint", Color("#ff6b2c", 0.52))
	return material


func _add_build_ghost_box(root: Node3D, size: Vector3, position: Vector3, yaw_degrees: float = 0.0) -> void:
	var mesh := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	mesh.mesh = box
	mesh.position = position
	mesh.rotation_degrees.y = yaw_degrees
	root.add_child(mesh)


func _add_build_ghost_cylinder(root: Node3D, bottom_radius: float, height: float, position: Vector3, top_radius: float = -1.0) -> void:
	var mesh := MeshInstance3D.new()
	var cylinder := CylinderMesh.new()
	cylinder.bottom_radius = bottom_radius
	cylinder.top_radius = bottom_radius if top_radius < 0.0 else top_radius
	cylinder.height = height
	mesh.mesh = cylinder
	mesh.position = position
	root.add_child(mesh)

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

func _make_road_mesh(start: Vector2, finish: Vector2, width: float, height_offset := 0.12) -> ArrayMesh:
	var direction := finish - start
	if direction.length_squared() < 0.001:
		direction = Vector2.RIGHT
	var side := direction.normalized().orthogonal() * width * 0.5
	# Sample both edges along the road, not just its center endpoints. Long
	# quads cut through hills even when native placement accepts the slope.
	var steps := clampi(int(ceilf(start.distance_to(finish) / maxf(width, 1.0))), 1, 4096)
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	for index in range(steps + 1):
		var center := start.lerp(finish, float(index) / float(steps))
		for edge in [center + side, center - side]:
			vertices.append(Vector3(edge.x, _terrain_height_at(edge.x, edge.y) + height_offset, edge.y))
			normals.append(Vector3.UP)
		if index < steps:
			var first := index * 2
			# Counter-clockwise from above. The previous winding faced into the
			# terrain, so culling hid both built roads and authored corridors.
			indices.append_array(PackedInt32Array([first, first + 2, first + 1, first + 2, first + 3, first + 1]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_INDEX] = indices
	arrays[Mesh.ARRAY_NORMAL] = normals
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

func _update_build_ghost() -> void:
	if build_mode_type < 0:
		return
	var ghost_target := _screen_to_world(get_viewport().get_mouse_position())
	if build_ghost == null:
		build_ghost = _make_build_ghost(build_mode_type)
		add_child(build_ghost)
	var ghost_valid := true
	var ghost_earthworks := false
	if build_mode_type >= 100 and extension != null:
		var resolved_target := _resolve_build_target(build_mode_type, ghost_target)
		ghost_valid = resolved_target != Vector2.INF
		if ghost_valid:
			ghost_target = resolved_target
			var site: Dictionary = extension.call("get_structure_site", build_mode_type - 100, ghost_target.x, ghost_target.y)
			ghost_earthworks = bool(site.get("earthworks", false))
	_set_build_ghost_valid(ghost_valid, ghost_earthworks)
	build_ghost.position = Vector3(ghost_target.x, _terrain_height_at(ghost_target.x, ghost_target.y) + 0.08, ghost_target.y)


func _set_build_ghost_valid(valid: bool, earthworks := false) -> void:
	if build_ghost_material == null:
		return
	var tint := Color("#ff334d", 0.50)
	if valid:
		tint = Color("#ffc83d", 0.55) if earthworks else Color("#ff6b2c", 0.52)
	build_ghost_material.set_shader_parameter("tint", tint)

func _queue_commander_structure(structure_type: int, target: Vector2, require_selection := true) -> bool:
	if require_selection and not selected_ids.has(_player_engineer_id()):
		return false
	if bool(extension.call("queue_faction_structure", HUMAN_PLAYER_ID, structure_type, target.x, target.y)):
		pending_structure_position = target
		_update_hud()
		return true
	else:
		push_warning("Field Engineer could not queue this structure")
		return false

var _airfield_materials := {}


func _airfield_material(piece: Dictionary) -> ShaderMaterial:
	var kind: int = piece.kind
	var size: Vector2 = piece.size
	var edge_gap: Vector3 = piece.get("edge_gap", Vector3.ZERO)
	var key := "%d:%s:%s" % [kind, size, edge_gap]
	if _airfield_materials.has(key):
		return _airfield_materials[key]
	var material := ShaderMaterial.new()
	material.shader = AirfieldSurfaceShader
	material.set_shader_parameter("asphalt_albedo", load("res://assets/airfield/asphalt_track_diff_2k.jpg"))
	material.set_shader_parameter("asphalt_normal", load("res://assets/airfield/asphalt_track_nor_gl_2k.jpg"))
	material.set_shader_parameter("concrete_albedo", load("res://assets/airfield/concrete_floor_02_diff_2k.jpg"))
	material.set_shader_parameter("concrete_normal", load("res://assets/airfield/concrete_floor_02_nor_gl_2k.jpg"))
	material.set_shader_parameter("surface_kind", kind)
	material.set_shader_parameter("surface_width", size.x)
	material.set_shader_parameter("surface_length", size.y)
	material.set_shader_parameter("edge_gap_side", edge_gap.x)
	material.set_shader_parameter("edge_gap_range", Vector2(edge_gap.y, edge_gap.z))
	_airfield_materials[key] = material
	return material


func _add_airfield_paving(root: Node3D) -> void:
	for piece in AIRFIELD_PAVING:
		var paving := MeshInstance3D.new()
		paving.name = "Runway" if piece.kind == 0 else "Taxiway" if piece.kind == 1 else "Apron"
		var plane := PlaneMesh.new()
		plane.size = piece.size
		# Enough vertices that the lit surface keeps its shape at grazing angles.
		plane.subdivide_depth = int(piece.size.y / 50.0)
		paving.mesh = plane
		paving.position = Vector3(piece.center.x, AIRFIELD_PAVING_HEIGHT, piece.center.y)
		paving.rotation_degrees.y = piece.yaw
		paving.material_override = _airfield_material(piece)
		paving.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		root.add_child(paving)
	# Runway designators, painted 9 m tall beyond each threshold. The runway
	# runs north-south (-Z is north), so it is 36 from the south and 18 from
	# the north; each number reads upright to a pilot landing over it.
	var runway: Dictionary = AIRFIELD_PAVING[0]
	for end in [1.0, -1.0]:
		var number := Label3D.new()
		number.name = "RunwayDesignator%s" % ("36" if end > 0.0 else "18")
		number.text = "36" if end > 0.0 else "18"
		number.font_size = 96
		number.outline_size = 0
		number.pixel_size = 0.115
		number.modulate = Color(0.86, 0.87, 0.84, 0.92)
		number.shaded = true
		number.double_sided = false
		number.alpha_cut = Label3D.ALPHA_CUT_DISCARD
		number.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
		number.position = Vector3(runway.center.x, AIRFIELD_PAVING_HEIGHT + 0.04, end * (AIRFIELD_RUNWAY_LENGTH * 0.5 - 48.0))
		number.rotation_degrees = Vector3(-90.0, 0.0 if end > 0.0 else 180.0, 0.0)
		number.scale = Vector3(0.7, 1.0, 1.0)
		root.add_child(number)


func _matte_material(color: Color, roughness := 0.8, metallic := 0.1) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metallic
	return material


func _add_airfield_part(root: Node3D, mesh: Mesh, position: Vector3, material: Material, rotation := Vector3.ZERO, scale := Vector3.ONE) -> MeshInstance3D:
	var part := MeshInstance3D.new()
	part.mesh = mesh
	part.position = position
	part.rotation_degrees = rotation
	part.scale = scale
	part.material_override = material
	root.add_child(part)
	return part


# A steel hangar with an arched roof and its doors facing the apron, and a
# concrete control tower with a glazed cab overlooking the runway.
func _add_airfield_buildings(root: Node3D) -> void:
	var size := AIRFIELD_HANGAR_SIZE
	var wall_height := size.y * 0.6
	var cladding := _matte_material(Color("#7b8079"), 0.6, 0.35)
	var walls := BoxMesh.new()
	walls.size = Vector3(size.x, wall_height, size.z)
	var hangar := _add_airfield_part(root, walls, AIRFIELD_HANGAR_POSITION + Vector3(0.0, wall_height * 0.5 - size.y * 0.5, 0.0), cladding)
	hangar.name = "Hangar"
	# The arched roof is a cylinder along the hangar's length, flattened.
	var roof := CylinderMesh.new()
	roof.top_radius = size.x * 0.5
	roof.bottom_radius = size.x * 0.5
	roof.height = size.z
	roof.radial_segments = 32
	_add_airfield_part(root, roof, AIRFIELD_HANGAR_POSITION + Vector3(0.0, wall_height - size.y * 0.5, 0.0),
		_matte_material(Color("#6a7068"), 0.5, 0.45), Vector3(90.0, 0.0, 0.0), Vector3(1.0, 1.0, (size.y - wall_height) / (size.x * 0.5)))
	# Sliding doors across most of the apron-facing wall.
	var doors := BoxMesh.new()
	doors.size = Vector3(0.4, wall_height * 0.9, size.z * 0.8)
	_add_airfield_part(root, doors, AIRFIELD_HANGAR_POSITION + Vector3(-size.x * 0.5 - 0.2, wall_height * 0.45 - size.y * 0.5, 0.0),
		_matte_material(Color("#3b4146"), 0.55, 0.4))
	var concrete := _matte_material(Color("#9a978d"), 0.9, 0.0)
	var shaft := CylinderMesh.new()
	shaft.top_radius = 3.0
	shaft.bottom_radius = 3.6
	shaft.height = 18.0
	var base := AIRFIELD_TOWER_POSITION - Vector3(0.0, AIRFIELD_TOWER_POSITION.y, 0.0)
	_add_airfield_part(root, shaft, base + Vector3(0.0, 9.0, 0.0), concrete).name = "ControlTower"
	var cab := CylinderMesh.new()
	cab.top_radius = 5.8
	cab.bottom_radius = 4.6
	cab.height = 3.6
	cab.radial_segments = 8
	_add_airfield_part(root, cab, base + Vector3(0.0, 19.8, 0.0), _matte_material(Color("#1d3138"), 0.15, 0.6))
	var cap := CylinderMesh.new()
	cap.top_radius = 6.2
	cap.bottom_radius = 6.2
	cap.height = 0.6
	cap.radial_segments = 8
	_add_airfield_part(root, cap, base + Vector3(0.0, 21.9, 0.0), concrete)


func _spawn_completed_structure_view(structure_type: int, target: Vector2) -> void:
	var root := Node3D.new()
	root.name = "Built_%s" % ("Outpost" if structure_type == 0 else "RadarMast" if structure_type == 1 else "Airfield" if structure_type == 2 else "Floodlight")
	root.set_meta("structure_type", structure_type)
	root.position = Vector3(target.x, _terrain_height_at(target.x, target.y) + 0.05, target.y)
	if structure_type != 2:
		var mesh := MeshInstance3D.new()
		var box := BoxMesh.new()
		box.size = Vector3(5.0, 1.6 if structure_type == 0 else 7.0 if structure_type == 1 else 2.0, 5.0)
		mesh.mesh = box
		mesh.position.y = box.size.y * 0.5
		mesh.material_override = _demo_material(Color("#d09a45") if structure_type == 0 else Color("#55b9c9"), 0.0)
		root.add_child(mesh)
	if structure_type == 2:
		_add_airfield_paving(root)
		_add_airfield_buildings(root)
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
	status.text = "%s\nMATERIALS %.0f" % [_structure_display_name(structure_type), _structure_material_cost(structure_type)]
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


func _sync_construction_frames(queue: Array) -> void:
	for child in construction_frames.get_children():
		child.queue_free()
	if queue.is_empty():
		return
	var queued: Dictionary = queue[0]
	var scaffold := _demo_material(Color("#47d9ff"), 0.75)
	scaffold.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	scaffold.albedo_color.a = 0.52
	var build_type := int(queued.get("type", -1))
	var root := _make_build_preview(build_type, scaffold) if build_type >= 100 else Node3D.new()
	root.name = "InProgress_%s" % String(queued.get("name", "UNIT"))
	var build_x := float(queued.get("target_x", 0.0))
	var build_z := float(queued.get("target_y", 0.0))
	root.position = Vector3(build_x, _terrain_height_at(build_x, build_z) + 0.08, build_z)
	construction_frames.add_child(root)
	if build_type >= 100:
		return
	# Legacy unit production keeps its compact assembly frame.
	for x in [-3.0, 3.0]:
		for z in [-2.5, 2.5]:
			_add_box_model(root, Vector3(0.18, 5.0, 0.18), Vector3(x, 2.5, z), scaffold)
	_add_box_model(root, Vector3(6.3, 0.18, 0.18), Vector3(0, 5.0, -2.5), scaffold)
	_add_box_model(root, Vector3(6.3, 0.18, 0.18), Vector3(0, 5.0, 2.5), scaffold)
	_add_box_model(root, Vector3(0.18, 0.18, 5.3), Vector3(-3.0, 5.0, 0), scaffold)
	_add_box_model(root, Vector3(0.18, 0.18, 5.3), Vector3(3.0, 5.0, 0), scaffold)
	var completion := clampf(float(queued.get("progress", 0.0)), 0.06, 1.0)
	_add_box_model(root, Vector3(4.7 * completion, 0.8, 2.8), Vector3(0, 0.45, 0), scaffold)


func _issue_attack_order(screen_position: Vector2) -> int:
	# As for moves: the fixed command post never takes the order.
	if selected_ids.is_empty():
		_refuse_command_post_order()
		return -1

	var target := _screen_ground_point(screen_position)
	if target == Vector2.INF:
		return -1

	var mouse_x := target.x
	var mouse_z := target.y

	var nearest_enemy_id: int = -1
	var nearest_distance := 9999.0

	for entity_id in ai_entity_ids:
		if not seen_enemy_ids.has(entity_id):
			continue
		var entity_pos := _entity_world_position(entity_id)
		var dx := entity_pos.x - mouse_x
		var dz := entity_pos.z - mouse_z
		var dist := sqrt(dx * dx + dz * dz)
		if dist < nearest_distance:
			nearest_distance = dist
			nearest_enemy_id = entity_id

	var enemy_post := int(commander_ids.get(1, -1))
	if enemy_post > 0 and seen_enemy_ids.has(enemy_post) and _native_unit_position(enemy_post).distance_to(Vector2(mouse_x, mouse_z)) < nearest_distance:
		nearest_enemy_id = enemy_post

	if nearest_enemy_id >= 0:
		var accepted_count: int = extension.call("issue_attack_commands", selected_ids, HUMAN_PLAYER_ID, nearest_enemy_id)
		if accepted_count != selected_ids.size():
			push_warning("Attack order rejected by authoritative command validation")
			return -1
	return nearest_enemy_id


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


func _apply_terrain_mesh() -> void:
	var mesh := terrain_view.mesh as ArrayMesh
	if mesh == null:
		mesh = ArrayMesh.new()
	var surface_material := terrain_view.get_surface_override_material(0) if terrain_view.get_surface_override_material_count() > 0 else null
	mesh.clear_surfaces()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, terrain_mesh_arrays)
	terrain_view.mesh = mesh
	if surface_material != null:
		terrain_view.set_surface_override_material(0, surface_material)


func _sync_terrain_edits() -> void:
	var revision := int(extension.call("get_terrain_revision"))
	if revision == terrain_revision_seen:
		return
	terrain_revision_seen = revision
	var heights: PackedFloat32Array = extension.call("get_terrain_heights")
	if heights.size() != terrain_heights.size():
		return
	var first := Vector2i(TERRAIN_SAMPLE_WIDTH, TERRAIN_SAMPLE_HEIGHT)
	var last := Vector2i(-1, -1)
	for index in range(heights.size()):
		if heights[index] != terrain_heights[index]:
			var cell := Vector2i(index % TERRAIN_SAMPLE_WIDTH, index / TERRAIN_SAMPLE_WIDTH)
			first = Vector2i(mini(first.x, cell.x), mini(first.y, cell.y))
			last = Vector2i(maxi(last.x, cell.x), maxi(last.y, cell.y))
	terrain_heights = heights
	if last.x < 0:
		return
	HeightMap.update_terrain_arrays(terrain_mesh_arrays, heights, Rect2i(first, last - first), terrain_world_width, terrain_world_height)
	for wrapper in prototype_visual_views.values():
		wrapper.invalidate_range_terrain()
	_apply_terrain_mesh()
	var cell_size := Vector2(terrain_world_width, terrain_world_height) / Vector2(TERRAIN_SAMPLE_WIDTH - 1, TERRAIN_SAMPLE_HEIGHT - 1)
	var world_origin := Vector2(first) * cell_size - Vector2(terrain_world_width, terrain_world_height) * 0.5
	var area := Rect2(world_origin, Vector2(last - first) * cell_size)
	if terrain_vegetation != null:
		terrain_vegetation.clear_area(area)


func _update_hud() -> void:
	if extension != null and match_started:
		_sync_terrain_edits()
		_sync_fuel_supply()
		_sync_power_grid()
		_sync_strategic_command()
		_sync_paradrop_markers()
	_activate_reinforcement_delivery_from_airfield()
	_update_steering_debug()
	var scenario_name: String = scenario_definition.get("display_name", "Scale profile")
	var commander_id := int(commander_ids.get(HUMAN_PLAYER_ID, -1))
	var selected_for_hud := selected_ids[0] if selected_ids.size() == 1 else -1
	var inspect_fob := fob_build_target != Vector2.INF
	var fob_x := fob_build_target.x if inspect_fob else 0.0
	var fob_y := fob_build_target.y if inspect_fob else 0.0
	var hud_state: Dictionary = extension.call(
		"get_hud_state", HUMAN_PLAYER_ID, commander_id, selected_for_hud, fob_x, fob_y, inspect_fob
	)
	debug_label.text = ("%s\nPlayer: %d  Enemy: %d  Selected: %d\nFPS: %d  Sim: %.2f ms" % [
		scenario_name,
		player_entity_ids.size(),
		ai_entity_ids.size(),
		selected_ids.size(),
		Engine.get_frames_per_second(),
		hud_state.get("tick_ms", 0.0),
	]).to_upper()
	var tactical_resources: Array = hud_state.get("tactical_resources", [])
	var selection_detail := "Drag-select a force to inspect it."
	if material_order_mode == 1:
		selection_detail = "CLAIM MODE  •  RIGHT-CLICK A MATERIAL FACILITY, FUEL DEPOT, POWER STATION OR POWER LINE"
	elif material_order_mode == 2:
		selection_detail = "DEMOLISH MODE  •  RIGHT-CLICK A MATERIAL FACILITY, FUEL DEPOT OR POWER LINE"
	elif fob_build_mode:
		selection_detail = "FOB PLACEMENT  •  LEFT-CLICK TERRAIN"
	var selected_unit: Dictionary = {}
	var selected_off_road: PackedFloat32Array = hud_state.get("selected_off_road", PackedFloat32Array())
	if selected_ids.size() == 1:
		selected_unit = _selected_unit_snapshot(selected_ids[0], hud_state.get("selected_health", []))
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
	var production_queue: Array = hud_state.get("production_queue", [])
	var fob_installation: Array = hud_state.get("fob_installation", [])
	if inspect_fob:
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
		"match_seconds": float(extension.call("match_state").get("seconds", 0.0)),
		"game_speed": roundi(float(extension.call("get_game_speed")) * 100.0),
		"speed_locked": not _can_change_game_speed(),
		"friendly": player_entity_ids.size(),
		"enemy": seen_enemy_ids.size(),
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
		"build_catalog": hud_state.get("build_catalog", []),
		"materials": _hud_resource_pair(tactical_resources, 0, 1),
		"fuel": _hud_resource_pair(tactical_resources, 2, 3),
		"power": _hud_power(tactical_resources),
		"power_reserve": _hud_power_reserve(tactical_resources),
		"power_deficit": tactical_resources.size() >= 7 and float(tactical_resources[6]) < 0.0,
		"tactical_resources": tactical_resources,
		"tick": hud_state.get("tick_ms", 0.0),
		"fps": Engine.get_frames_per_second(),
		"engine": "ONLINE",
		"objective_status": control_point_battle_status,
		"reinforcement_status": _with_fuel_status(reinforcement_delivery_status),
	})
	_sync_strategic_command()
	_sync_command_post_panel()


func _hud_resource_pair(resources: Array, current_index: int, capacity_index: int) -> String:
	if resources.size() <= capacity_index:
		return "--"
	return "%.0f / %.0f" % [float(resources[current_index]), float(resources[capacity_index])]


func _hud_power(resources: Array) -> String:
	if resources.size() <= 5:
		return "--"
	return "%.0f MW / %.0f MW" % [float(resources[4]), float(resources[5])]


func _hud_power_reserve(resources: Array) -> String:
	if resources.size() <= 6:
		return ""
	return "RESERVE %+.0f MW" % float(resources[6])


func _selected_unit_snapshot(entity_id: int, health: Array = []) -> Dictionary:
	var display_name := String(extension.call("get_unit_display_name", entity_id)).to_upper()
	if health.is_empty():
		health = extension.call("get_unit_health", entity_id)
	return {
		"id": entity_id,
		"name": display_name if not display_name.is_empty() else "UNKNOWN UNIT",
		"health": float(health[0]) if health.size() >= 2 else -1.0,
		"max_health": float(health[1]) if health.size() >= 2 else -1.0,
	}
