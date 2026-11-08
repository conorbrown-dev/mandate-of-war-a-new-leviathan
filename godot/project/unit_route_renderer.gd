extends Node3D

## Presentation-only drawing of selected units' remaining routes. The native
## simulation decides every route (`RtsExtension.get_unit_routes`); this node
## only lays each one on the terrain as a thin ribbon with a mark at each turn.
## A unit with Shift-queued waypoints reports one route through every leg in
## driving order; each waypoint gets a larger square marker.
const UiTokens := preload("res://ui/theme/ui_tokens.gd")
const MIN_WIDTH := 0.35
const MAX_WIDTH := 2.4
const HEIGHT_OFFSET := 0.32
const MIN_SAMPLE_SPACING := 2.0
const MAX_SAMPLES_PER_ROUTE := 240

var terrain_height_sampler: Callable
var route_mesh_instance: MeshInstance3D
## The routes last drawn, in simulation (x, y) coordinates, one per unit.
var drawn_routes: Array[PackedVector2Array] = []
## The waypoints last drawn, one array per drawn route, in driving order.
var drawn_waypoints: Array[PackedVector2Array] = []
var _material: StandardMaterial3D
var _camera_distance := 100.0
var _width := MIN_WIDTH


func _ready() -> void:
	route_mesh_instance = MeshInstance3D.new()
	route_mesh_instance.name = "UnitRouteRibbons"
	route_mesh_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_material = StandardMaterial3D.new()
	# Cyan is reserved for command intent; the route is a quieter tint of it
	# than the move-order arrow so the arrow stays the primary feedback.
	var tint := UiTokens.ACCENT
	tint.a = 0.55
	_material.albedo_color = tint
	_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	route_mesh_instance.material_override = _material
	add_child(route_mesh_instance)
	visible = false


func set_terrain_height_sampler(sampler: Callable) -> void:
	terrain_height_sampler = sampler


func set_camera_distance(distance: float) -> void:
	_camera_distance = maxf(distance, 0.0)
	var next_width := _width_for_distance()
	if visible and not is_equal_approx(next_width, _width):
		_width = next_width
		_rebuild()


## Draws routes, each a PackedVector2Array of simulation (x, y) points that
## starts at the unit and ends at its last destination. `waypoints`, when
## given, holds each unit's destinations in order
## (`RtsExtension.get_unit_waypoints`), matched to `routes` by index; each one
## is marked. Empty routes are skipped.
func show_routes(routes: Array, waypoints: Array = []) -> void:
	drawn_routes.clear()
	drawn_waypoints.clear()
	for index in range(routes.size()):
		var points := routes[index] as PackedVector2Array
		if points.size() >= 2:
			drawn_routes.append(points)
			drawn_waypoints.append(waypoints[index] as PackedVector2Array if index < waypoints.size() else PackedVector2Array())
	if drawn_routes.is_empty():
		clear_routes()
		return
	_width = _width_for_distance()
	_rebuild()
	visible = true


func clear_routes() -> void:
	drawn_routes.clear()
	drawn_waypoints.clear()
	visible = false


## Number of turns drawn across all routes: route points that are neither the
## unit, nor a marked waypoint, nor (with no waypoints given) the destination.
func corner_count() -> int:
	var total := 0
	for index in range(drawn_routes.size()):
		total += _turn_indices(drawn_routes[index], drawn_waypoints[index]).size()
	return total


## Number of legs drawn across all routes: one per marked waypoint, or one per
## route drawn without waypoints.
func leg_count() -> int:
	var total := 0
	for index in range(drawn_routes.size()):
		total += maxi(drawn_waypoints[index].size(), 1)
	return total


func waypoint_marker_size() -> float:
	return maxf(_width * 2.6, 1.4)


func _turn_indices(route: PackedVector2Array, waypoints: PackedVector2Array) -> PackedInt32Array:
	var turns := PackedInt32Array()
	var last := route.size() - 1 if waypoints.is_empty() else route.size()
	for index in range(1, last):
		var is_waypoint := false
		for waypoint in waypoints:
			if route[index].distance_to(waypoint) < 0.01:
				is_waypoint = true
				break
		if not is_waypoint:
			turns.append(index)
	return turns


func _width_for_distance() -> float:
	return clampf(0.35 + _camera_distance * 0.0012, MIN_WIDTH, MAX_WIDTH)


func _height(point: Vector2) -> float:
	if terrain_height_sampler.is_valid():
		var value = terrain_height_sampler.call(point.x, point.y)
		if typeof(value) == TYPE_FLOAT or typeof(value) == TYPE_INT:
			return float(value)
	return 0.0


func _ground(point: Vector2) -> Vector3:
	return Vector3(point.x, _height(point) + HEIGHT_OFFSET, point.y)


func _rebuild() -> void:
	var vertices := PackedVector3Array()
	var indices := PackedInt32Array()
	for index in range(drawn_routes.size()):
		_append_route(drawn_routes[index], drawn_waypoints[index], vertices, indices)
	if vertices.is_empty():
		route_mesh_instance.mesh = null
		return
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	route_mesh_instance.mesh = mesh


func _append_route(route: PackedVector2Array, waypoints: PackedVector2Array, vertices: PackedVector3Array, indices: PackedInt32Array) -> void:
	var length := 0.0
	for index in range(route.size() - 1):
		length += route[index].distance_to(route[index + 1])
	# Sample each leg so the ribbon follows the ground between corners. Long
	# routes at strategic zoom use coarser spacing to bound the vertex count.
	var spacing := maxf(MIN_SAMPLE_SPACING, length / float(MAX_SAMPLES_PER_ROUTE))
	var half := _width * 0.5
	for index in range(route.size() - 1):
		var start := route[index]
		var finish := route[index + 1]
		var leg := finish - start
		var leg_length := leg.length()
		if leg_length < 0.001:
			continue
		var direction := leg / leg_length
		var normal := Vector2(-direction.y, direction.x) * half
		var steps := maxi(1, ceili(leg_length / spacing))
		var base := vertices.size()
		for step in range(steps + 1):
			var point := start.lerp(finish, float(step) / float(steps))
			var center := _ground(point)
			vertices.append(center + Vector3(normal.x, 0.0, normal.y))
			vertices.append(center - Vector3(normal.x, 0.0, normal.y))
		for step in range(steps):
			var quad := base + step * 2
			indices.append_array(PackedInt32Array([quad, quad + 1, quad + 2, quad + 1, quad + 3, quad + 2]))
	# A small diamond marks each turn so the bend reads at a glance.
	var mark := maxf(_width * 1.6, 0.8)
	for index in _turn_indices(route, waypoints):
		var center := _ground(route[index]) + Vector3(0.0, 0.01, 0.0)
		var first := vertices.size()
		vertices.append(center + Vector3(mark, 0.0, 0.0))
		vertices.append(center + Vector3(0.0, 0.0, mark))
		vertices.append(center + Vector3(-mark, 0.0, 0.0))
		vertices.append(center + Vector3(0.0, 0.0, -mark))
		indices.append_array(PackedInt32Array([first, first + 1, first + 2, first, first + 2, first + 3]))
	# A larger axis-aligned square marks each waypoint, so the stops read apart
	# from the diamond turn marks.
	var half_square := waypoint_marker_size()
	for waypoint in waypoints:
		var center := _ground(waypoint) + Vector3(0.0, 0.02, 0.0)
		var first := vertices.size()
		vertices.append(center + Vector3(-half_square, 0.0, -half_square))
		vertices.append(center + Vector3(half_square, 0.0, -half_square))
		vertices.append(center + Vector3(half_square, 0.0, half_square))
		vertices.append(center + Vector3(-half_square, 0.0, half_square))
		indices.append_array(PackedInt32Array([first, first + 1, first + 2, first, first + 2, first + 3]))
