extends Node3D

const UiTypographyScript = preload("res://ui_typography.gd")

const RegistryScript = preload("res://visual_definition_registry.gd")
const ValidatorScript = preload("res://visual_asset_validator.gd")
const SpawnBridgeScript = preload("res://visual_spawn_bridge.gd")

@onready var camera: Camera3D = $Camera3D
@onready var filter: OptionButton = $CanvasLayer/Panel/VBox/Filter
@onready var list: ItemList = $CanvasLayer/Panel/VBox/VisualList
@onready var details: RichTextLabel = $CanvasLayer/Panel/VBox/Details

var registry
var definitions: Array = []
var active_visual: Node3D
var orbit_yaw := 0.65
var orbit_pitch := -0.35
var orbit_distance := 13.0

func _ready() -> void:
	UiTypographyScript.apply_to(self)
	registry = RegistryScript.new()
	registry.load_definitions()
	definitions = registry._definitions.values()
	for category in ["all", "ground", "air", "naval", "logistics"]:
		filter.add_item(category)
	filter.item_selected.connect(func(_index): _rebuild_list())
	list.item_selected.connect(_select_visual)
	_rebuild_list()
	_update_camera()
	var report = ValidatorScript.new().validate()
	details.text = "[b]Asset Viewer[/b]\n%d definitions; %d validation warnings" % [report.entries.size(), report.warnings.size()]

func _rebuild_list() -> void:
	list.clear()
	var selected_category := filter.get_item_text(filter.selected)
	for definition in definitions:
		if selected_category == "all" or ValidatorScript.new()._category(definition) == selected_category:
			list.add_item(String(definition.visual_id))

func _select_visual(index: int) -> void:
	var visual_id := list.get_item_text(index)
	var definition: Dictionary = registry.resolve(visual_id)
	if active_visual != null:
		active_visual.queue_free()
	active_visual = SpawnBridgeScript.new().spawn(self, registry, visual_id, Transform3D.IDENTITY, Color("#70b9db"))
	active_visual.set_selected(true)
	details.text = "[b]%s[/b]\nCategory: %s\nModel: %s\nProvenance: prototype/donor\nFootprint: %s" % [visual_id, ValidatorScript.new()._category(definition), definition.model_path, active_visual.presentation_footprint()]

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		orbit_yaw -= event.relative.x * 0.01
		orbit_pitch = clampf(orbit_pitch - event.relative.y * 0.01, -1.15, 0.2)
		_update_camera()
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_WHEEL_UP:
		orbit_distance = maxf(3.0, orbit_distance - 1.0)
		_update_camera()
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		orbit_distance = minf(60.0, orbit_distance + 1.0)
		_update_camera()

func _update_camera() -> void:
	var direction := Vector3(cos(orbit_pitch) * sin(orbit_yaw), sin(orbit_pitch), cos(orbit_pitch) * cos(orbit_yaw))
	camera.position = direction * orbit_distance
	camera.look_at(Vector3(0, 0.6, 0), Vector3.UP)
