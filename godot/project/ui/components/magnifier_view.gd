class_name MagnifierView
extends MandatePanel
## Inset showing a close-up of the ground under the mouse cursor. Presentation
## only: a second camera renders the same World3D as the main view, so the fog
## of war, hidden enemies and terrain match what the player can already see.

const Tokens = preload("res://ui/theme/ui_tokens.gd")

## 8x is the widest the inset goes; the player can only zoom further in.
const ZOOM_LEVELS := [8.0, 16.0, 32.0]
const DEFAULT_ZOOM_INDEX := 0
const VIEW_SIZE := Vector2i(280, 280)
const MIN_DISTANCE := 24.0
const TOP_MARGIN := 100

var _viewport: SubViewport
var _camera: Camera3D
var _zoom_label: Label
var _zoom_index := DEFAULT_ZOOM_INDEX
var _enabled := true
var _has_target := false


func _ready() -> void:
	super._ready()
	theme = preload("res://ui/theme/mandate_theme.tres")
	surface = "command"
	name = "MagnifierView"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
	var width := VIEW_SIZE.x + Tokens.SPACE_SM * 2
	offset_left = -width - Tokens.SPACE_MD
	offset_right = -Tokens.SPACE_MD
	offset_top = TOP_MARGIN
	offset_bottom = TOP_MARGIN + VIEW_SIZE.y + Tokens.SPACE_SM * 2 + 24
	var margin := MarginContainer.new()
	margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
	for side in ["margin_left", "margin_right", "margin_top", "margin_bottom"]:
		margin.add_theme_constant_override(side, Tokens.SPACE_SM)
	add_child(margin)
	var column := VBoxContainer.new()
	column.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_theme_constant_override("separation", Tokens.SPACE_XS)
	margin.add_child(column)
	var header := HBoxContainer.new()
	header.mouse_filter = Control.MOUSE_FILTER_IGNORE
	column.add_child(header)
	var title := Label.new()
	title.text = "MAGNIFIER"
	title.theme_type_variation = "EyebrowLabel"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(title)
	_zoom_label = Label.new()
	_zoom_label.name = "ZoomLabel"
	_zoom_label.theme_type_variation = "MutedLabel"
	header.add_child(_zoom_label)
	var container := SubViewportContainer.new()
	container.stretch = true
	container.mouse_filter = Control.MOUSE_FILTER_IGNORE
	container.custom_minimum_size = Vector2(VIEW_SIZE)
	column.add_child(container)
	_viewport = SubViewport.new()
	_viewport.size = VIEW_SIZE
	_viewport.handle_input_locally = false
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	_viewport.world_3d = get_viewport().world_3d
	container.add_child(_viewport)
	# A small reticle marks the exact point under the cursor.
	var reticle := Control.new()
	reticle.name = "Reticle"
	reticle.mouse_filter = Control.MOUSE_FILTER_IGNORE
	reticle.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	reticle.draw.connect(func():
		var c := reticle.size * 0.5
		var tone := Color(Tokens.ACCENT, 0.85)
		for direction in [Vector2.LEFT, Vector2.RIGHT, Vector2.UP, Vector2.DOWN]:
			reticle.draw_line(c + direction * 6.0, c + direction * 16.0, tone, 1.0)
	)
	container.add_child(reticle)
	_camera = Camera3D.new()
	_camera.name = "MagnifierCamera"
	_viewport.add_child(_camera)
	_refresh_label()
	visible = _enabled


func zoom_factor() -> float:
	return ZOOM_LEVELS[_zoom_index]


func is_enabled() -> bool:
	return _enabled


func has_target() -> bool:
	return _has_target


func camera() -> Camera3D:
	return _camera


func toggle() -> void:
	set_enabled(not _enabled)


func set_enabled(value: bool) -> void:
	_enabled = value
	visible = value
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS if value else SubViewport.UPDATE_DISABLED


func cycle_zoom(step: int) -> void:
	_zoom_index = clampi(_zoom_index + step, 0, ZOOM_LEVELS.size() - 1)
	_refresh_label()


## focus is the ground point to look at, or null when the cursor is over the HUD
## or off the map; the view then keeps its last position, dimmed.
func update_view(main_camera: Camera3D, main_distance: float, focus: Variant, focus_height: float) -> void:
	if not _enabled:
		return
	if focus != null:
		_has_target = true
		var ground := focus as Vector2
		var look := Vector3(ground.x, focus_height, ground.y)
		# Same bearing and pitch as the main camera, just closer.
		var direction := main_camera.global_transform.basis.z.normalized()
		_camera.far = main_camera.far
		_camera.fov = main_camera.fov
		_camera.near = clampf(main_distance * 0.01 / zoom_factor(), 0.05, 40.0)
		var distance := distance_for(main_distance)
		_camera.global_position = look + direction * distance
		_camera.look_at(look, Vector3.UP)
	modulate = Color(1, 1, 1, 1.0 if (focus != null or not _has_target) else 0.6)


## How far the inset's camera stands from the ground for a main-view distance.
func distance_for(main_distance: float) -> float:
	return maxf(MIN_DISTANCE, main_distance / zoom_factor())


func _refresh_label() -> void:
	_zoom_label.text = "%dx  ·  [ ]" % int(zoom_factor())
