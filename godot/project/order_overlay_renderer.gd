extends Node3D

## Centralized, presentation-only order visualization.  The simulation remains
## authoritative; this node only renders the latest selected move intent.
const UiTokens := preload("res://ui/theme/ui_tokens.gd")
const MAX_ROUTE_SAMPLES := 12
const MIN_WIDTH := 0.65
const MAX_WIDTH := 4.0
const HEIGHT_OFFSET := 0.28
const PULSE_SECONDS := 0.45

var terrain_height_sampler: Callable
var arrow_mesh_instance: MeshInstance3D
var destination_marker: Node3D
var _marker_ring: MeshInstance3D
var _arrow_material: StandardMaterial3D
var _marker_material: StandardMaterial3D
var _start := Vector2.ZERO
var _target := Vector2.ZERO
var _visible_order := false
var _camera_distance := 100.0
var _width := -1.0
var _pulse := 0.0

func _ready() -> void:
	arrow_mesh_instance = MeshInstance3D.new()
	arrow_mesh_instance.name = "MoveOrderRibbon"
	_arrow_material = StandardMaterial3D.new()
	var accent := UiTokens.ACCENT
	accent.a = 0.72
	_arrow_material.albedo_color = accent
	_arrow_material.emission_enabled = true
	_arrow_material.emission = UiTokens.ACCENT.darkened(0.45)
	_arrow_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_arrow_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	arrow_mesh_instance.material_override = _arrow_material
	add_child(arrow_mesh_instance)
	destination_marker = Node3D.new()
	destination_marker.name = "CommandFeedbackMarker"
	_marker_ring = MeshInstance3D.new()
	var torus := TorusMesh.new()
	torus.inner_radius = 1.5
	torus.outer_radius = 1.85
	torus.rings = 20
	torus.ring_segments = 8
	_marker_ring.mesh = torus
	_marker_material = _arrow_material.duplicate()
	var marker_accent := UiTokens.ACCENT.lightened(0.16)
	marker_accent.a = 0.9
	_marker_material.albedo_color = marker_accent
	_marker_material.emission = UiTokens.ACCENT.darkened(0.25)
	_marker_ring.material_override = _marker_material
	destination_marker.add_child(_marker_ring)
	add_child(destination_marker)
	visible = false

func set_terrain_height_sampler(sampler: Callable) -> void:
	terrain_height_sampler = sampler

func set_camera_distance(distance: float) -> void:
	_camera_distance = maxf(distance, 0.0)
	var next_width := clampf(0.65 + _camera_distance * 0.0022, MIN_WIDTH, MAX_WIDTH)
	if _visible_order and (not is_equal_approx(next_width, _width)):
		_width = next_width
		_rebuild_arrow()

func show_move_order(origin: Vector2, target: Vector2) -> void:
	_start = origin
	_target = target
	_visible_order = true
	_width = clampf(0.65 + _camera_distance * 0.0022, MIN_WIDTH, MAX_WIDTH)
	_pulse = PULSE_SECONDS
	_rebuild_arrow()
	_update_marker()
	visible = true
	destination_marker.visible = true

func clear_order() -> void:
	_visible_order = false
	visible = false
	destination_marker.visible = false

func update_order(active: bool) -> void:
	if _visible_order and not active:
		clear_order()

func _process(delta: float) -> void:
	if not _visible_order:
		return
	if _pulse > 0.0:
		_pulse = maxf(_pulse - delta, 0.0)
		var phase := 1.0 - _pulse / PULSE_SECONDS
		var scale_factor := 1.0 + sin(phase * PI) * 0.28
		destination_marker.scale = Vector3.ONE * scale_factor

func _height(point: Vector2) -> float:
	if terrain_height_sampler.is_valid():
		var value = terrain_height_sampler.call(point.x, point.y)
		if typeof(value) == TYPE_FLOAT or typeof(value) == TYPE_INT:
			return float(value)
	return 0.0

func _rebuild_arrow() -> void:
	var delta := _target - _start
	var length := delta.length()
	var direction := delta / length if length > 0.001 else Vector2(0.0, -1.0)
	var normal := Vector2(-direction.y, direction.x)
	var sample_count := clampi(int(length / 12.0) + 2, 2, MAX_ROUTE_SAMPLES)
	var vertices := PackedVector3Array()
	var indices := PackedInt32Array()
	for i in range(sample_count):
		var t := float(i) / float(sample_count - 1)
		var point := _start.lerp(_target, t)
		var center := Vector3(point.x, _height(point) + HEIGHT_OFFSET, point.y)
		var half := _width * 0.5
		vertices.append(center + Vector3(normal.x * half, 0.0, normal.y * half))
		vertices.append(center - Vector3(normal.x * half, 0.0, normal.y * half))
	for i in range(sample_count - 1):
		var base := i * 2
		indices.append_array(PackedInt32Array([base, base + 1, base + 2, base + 1, base + 3, base + 2]))
	# A filled arrowhead makes travel direction unambiguous at strategic zoom.
	var tip := Vector3(_target.x, _height(_target) + HEIGHT_OFFSET + 0.01, _target.y)
	var back := tip - Vector3(direction.x, 0.0, direction.y) * maxf(_width * 2.8, 2.0)
	var wing := Vector3(normal.x, 0.0, normal.y) * maxf(_width * 1.8, 1.4)
	var tip_index := vertices.size()
	vertices.append(tip)
	vertices.append(back + wing)
	vertices.append(back - wing)
	indices.append_array(PackedInt32Array([tip_index, tip_index + 1, tip_index + 2]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	arrow_mesh_instance.mesh = mesh

func _update_marker() -> void:
	destination_marker.position = Vector3(_target.x, _height(_target) + HEIGHT_OFFSET, _target.y)
	destination_marker.scale = Vector3.ONE
