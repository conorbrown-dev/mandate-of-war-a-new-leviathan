extends Node3D
## Dense presentation-only ground cover placed from the baked terrain masks.
##
## The map is split into chunks so the renderer can frustum-cull each batch.
## Every batch has a visibility range: detailed trees and shrubs exist only
## near the camera, simple trees carry the mid distance, and past that the
## terrain shader paints forest canopy. This layer never affects simulation.

const CHUNKS_PER_AXIS := 14
const MASK_SIZE := 640
const NEAR_RANGE := 3600.0
const FAR_RANGE := 13000.0
const NEAR_TREES_PER_KM2 := 1500.0
const FAR_TREES_PER_KM2 := 190.0
const SHRUBS_PER_CHUNK := 4200
const ROCKS_PER_CHUNK := 520
const MINIMUM_WORLD_SIZE := 10000.0
const RANDOM_SEED := 640642
const MINIMUM_LAND_HEIGHT := 6.0
const INSTANCE_FLOATS := 16

signal finished

var instance_count := 0
var batch_count := 0
var building := false

var _masks := PackedByteArray()
var _world_width := 40000.0
var _world_height := 40000.0
var _height_at := Callable()
var _material: StandardMaterial3D
var _build_run := 0


static func supports_world(world_width: float) -> bool:
	return world_width >= MINIMUM_WORLD_SIZE


# Builds one chunk per frame, nearest the focus point first, so the match stays
# responsive while the map fills in. A later rebuild or clear abandons this one.
func rebuild(masks: PackedByteArray, world_width: float, world_height: float, height_at: Callable, focus := Vector2.ZERO) -> void:
	clear()
	if masks.size() != MASK_SIZE * MASK_SIZE * 4 or not supports_world(world_width):
		return
	var run := _build_run
	building = true
	_masks = masks
	_world_width = world_width
	_world_height = world_height
	_height_at = height_at
	_material = StandardMaterial3D.new()
	_material.vertex_color_use_as_albedo = true
	_material.roughness = 1.0
	_material.metallic_specular = 0.0
	var conifer_near := _conifer_mesh(7, 3)
	var conifer_far := _conifer_mesh(5, 1)
	var broadleaf_near := _broadleaf_mesh(true)
	var broadleaf_far := _broadleaf_mesh(false)
	var shrub := _shrub_mesh()
	var rock := _rock_mesh()
	var rng := RandomNumberGenerator.new()
	rng.seed = RANDOM_SEED
	var chunk_size := Vector2(world_width, world_height) / float(CHUNKS_PER_AXIS)
	var square_km := chunk_size.x * chunk_size.y / 1000000.0
	var areas: Array[Rect2] = []
	for chunk_z in range(CHUNKS_PER_AXIS):
		for chunk_x in range(CHUNKS_PER_AXIS):
			var origin := (Vector2(chunk_x, chunk_z) - Vector2.ONE * float(CHUNKS_PER_AXIS) * 0.5) * chunk_size
			areas.append(Rect2(origin, chunk_size))
	areas.sort_custom(func(first: Rect2, second: Rect2) -> bool:
		return first.get_center().distance_squared_to(focus) < second.get_center().distance_squared_to(focus))
	for area in areas:
		_place_trees(area, int(square_km * NEAR_TREES_PER_KM2), conifer_near, broadleaf_near, NEAR_RANGE, 1.0, rng)
		_place_trees(area, int(square_km * FAR_TREES_PER_KM2), conifer_far, broadleaf_far, FAR_RANGE, 1.5, rng)
		_place_shrubs(area, shrub, rng)
		_place_rocks(area, rock, rng)
		if is_inside_tree():
			await get_tree().process_frame
		if run != _build_run:
			return
	building = false
	finished.emit()


func clear() -> void:
	_build_run += 1
	building = false
	for child in get_children():
		remove_child(child)
		child.queue_free()
	instance_count = 0
	batch_count = 0


# Hides everything growing inside a levelled site.
func clear_area(area: Rect2) -> void:
	for child in get_children():
		var batch := child as MultiMeshInstance3D
		if batch == null or batch.multimesh == null:
			continue
		var buffer := batch.multimesh.buffer
		var changed := false
		for offset in range(0, buffer.size(), INSTANCE_FLOATS):
			if not area.has_point(Vector2(buffer[offset + 3], buffer[offset + 11])):
				continue
			for scale_index in [0, 1, 2, 4, 5, 6, 8, 9, 10]:
				buffer[offset + scale_index] = 0.0
			changed = true
		if changed:
			batch.multimesh.buffer = buffer


func _mask_value(world_x: float, world_z: float, channel: int) -> int:
	var x := clampi(int((world_x / _world_width + 0.5) * float(MASK_SIZE - 1)), 0, MASK_SIZE - 1)
	var z := clampi(int((world_z / _world_height + 0.5) * float(MASK_SIZE - 1)), 0, MASK_SIZE - 1)
	return _masks[(z * MASK_SIZE + x) * 4 + channel]


func _place_trees(area: Rect2, attempts: int, conifer: Mesh, broadleaf: Mesh, view_range: float, size_factor: float, rng: RandomNumberGenerator) -> void:
	var conifers := PackedFloat32Array()
	var broadleaves := PackedFloat32Array()
	conifers.resize(attempts * INSTANCE_FLOATS)
	broadleaves.resize(attempts * INSTANCE_FLOATS)
	var conifer_count := 0
	var broadleaf_count := 0
	for _attempt in range(attempts):
		var position := _random_point(area, rng)
		var forest := _mask_value(position.x, position.y, 1)
		if forest < 24 or rng.randi_range(0, 255) > forest:
			continue
		var height: float = _height_at.call(position.x, position.y)
		if height < MINIMUM_LAND_HEIGHT:
			continue
		var conifer_chance := clampf((height - 260.0) / 700.0, 0.1, 0.94)
		var tint := rng.randf_range(0.8, 1.2)
		var scale := rng.randf_range(1.05, 1.9) * size_factor
		var color := Color(tint * rng.randf_range(0.88, 1.22), tint, tint * rng.randf_range(0.82, 1.02))
		if rng.randf() < conifer_chance:
			_write_instance(conifers, conifer_count, position, height - 0.3, scale, rng.randf_range(0.0, TAU), color)
			conifer_count += 1
		else:
			_write_instance(broadleaves, broadleaf_count, position, height - 0.3, scale, rng.randf_range(0.0, TAU), color)
			broadleaf_count += 1
	_add_batch(conifer, conifers, conifer_count, area, view_range)
	_add_batch(broadleaf, broadleaves, broadleaf_count, area, view_range)


func _place_shrubs(area: Rect2, mesh: Mesh, rng: RandomNumberGenerator) -> void:
	var shrubs := PackedFloat32Array()
	shrubs.resize(SHRUBS_PER_CHUNK * INSTANCE_FLOATS)
	var count := 0
	for _attempt in range(SHRUBS_PER_CHUNK):
		var position := _random_point(area, rng)
		var river := _mask_value(position.x, position.y, 0)
		var variation := _mask_value(position.x, position.y, 3)
		if river > 60 or rng.randi_range(0, 255) < 150 - variation / 2:
			continue
		var height: float = _height_at.call(position.x, position.y)
		if height < MINIMUM_LAND_HEIGHT or height > 1500.0:
			continue
		var tint := rng.randf_range(0.76, 1.16)
		_write_instance(shrubs, count, position, height - 0.2, rng.randf_range(0.7, 2.6), rng.randf_range(0.0, TAU),
			Color(tint * rng.randf_range(0.92, 1.18), tint, tint * 0.9))
		count += 1
	_add_batch(mesh, shrubs, count, area, NEAR_RANGE)


func _place_rocks(area: Rect2, mesh: Mesh, rng: RandomNumberGenerator) -> void:
	var rocks := PackedFloat32Array()
	rocks.resize(ROCKS_PER_CHUNK * INSTANCE_FLOATS)
	var count := 0
	for _attempt in range(ROCKS_PER_CHUNK):
		var position := _random_point(area, rng)
		var height: float = _height_at.call(position.x, position.y)
		var uphill: float = _height_at.call(position.x + 60.0, position.y + 60.0)
		var slope := absf(uphill - height) / 85.0
		if height < 30.0 or (slope < 0.2 and height < 1100.0):
			continue
		var tint := rng.randf_range(0.72, 1.12)
		_write_instance(rocks, count, position, height - 1.0, rng.randf_range(1.0, 6.5), rng.randf_range(0.0, TAU),
			Color(tint, tint * 0.97, tint * 0.92))
		count += 1
	_add_batch(mesh, rocks, count, area, FAR_RANGE)


func _random_point(area: Rect2, rng: RandomNumberGenerator) -> Vector2:
	return Vector2(rng.randf_range(area.position.x, area.end.x), rng.randf_range(area.position.y, area.end.y))


# MultiMesh buffer order: a 3x4 transform by rows, then the instance color.
func _write_instance(buffer: PackedFloat32Array, index: int, position: Vector2, height: float, scale: float, yaw: float, tint: Color) -> void:
	var cos_yaw := cos(yaw) * scale
	var sin_yaw := sin(yaw) * scale
	var offset := index * INSTANCE_FLOATS
	buffer[offset] = cos_yaw
	buffer[offset + 2] = sin_yaw
	buffer[offset + 3] = position.x
	buffer[offset + 5] = scale
	buffer[offset + 7] = height
	buffer[offset + 8] = -sin_yaw
	buffer[offset + 10] = cos_yaw
	buffer[offset + 11] = position.y
	buffer[offset + 12] = tint.r
	buffer[offset + 13] = tint.g
	buffer[offset + 14] = tint.b
	buffer[offset + 15] = 1.0


func _add_batch(mesh: Mesh, buffer: PackedFloat32Array, count: int, area: Rect2, view_range: float) -> void:
	if count == 0:
		return
	buffer.resize(count * INSTANCE_FLOATS)
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.mesh = mesh
	multimesh.instance_count = count
	multimesh.buffer = buffer
	var view := MultiMeshInstance3D.new()
	view.multimesh = multimesh
	view.material_override = _material
	view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	view.visibility_range_end = view_range
	view.visibility_range_end_margin = view_range * 0.08
	view.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_DISABLED
	view.custom_aabb = AABB(Vector3(area.position.x, -40.0, area.position.y), Vector3(area.size.x, 2200.0, area.size.y))
	add_child(view)
	instance_count += count
	batch_count += 1


func _conifer_mesh(sides: int, tiers: int) -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	_append_cone(faces, colors, Vector3.ZERO, 0.8, 6.0, sides, Color(0.2, 0.15, 0.1), Color(0.22, 0.16, 0.1))
	var crown_height := 21.0 / float(tiers)
	for tier in range(tiers):
		var base := 4.0 + float(tier) * crown_height * 0.82
		var radius := lerpf(6.2, 3.0, float(tier) / maxf(1.0, float(tiers - 1))) if tiers > 1 else 5.6
		var height := crown_height * 1.3 if tiers > 1 else 24.0
		_append_cone(faces, colors, Vector3(0.0, base, 0.0), radius, height, sides,
			Color(0.045, 0.1, 0.07), Color(0.1, 0.18, 0.12))
	return _mesh_from_triangles(faces, colors)


func _broadleaf_mesh(detailed: bool) -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	_append_cone(faces, colors, Vector3.ZERO, 0.8, 8.0, 5, Color(0.2, 0.15, 0.1), Color(0.23, 0.17, 0.11))
	var low := Color(0.075, 0.135, 0.065)
	var high := Color(0.165, 0.235, 0.105)
	_append_blob(faces, colors, Vector3(0.0, 13.0, 0.0), Vector3(7.5, 7.0, 7.5), low, high)
	if detailed:
		_append_blob(faces, colors, Vector3(3.6, 11.0, 2.2), Vector3(4.6, 4.2, 4.6), low, high)
		_append_blob(faces, colors, Vector3(-3.4, 11.6, -2.6), Vector3(4.2, 4.4, 4.2), low, high)
		_append_blob(faces, colors, Vector3(-1.0, 17.5, 1.6), Vector3(4.2, 3.6, 4.2), low, high)
	return _mesh_from_triangles(faces, colors)


func _shrub_mesh() -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	var low := Color(0.11, 0.15, 0.07)
	var high := Color(0.23, 0.27, 0.13)
	_append_blob(faces, colors, Vector3(0.0, 1.2, 0.0), Vector3(2.4, 1.6, 2.4), low, high)
	_append_blob(faces, colors, Vector3(1.7, 0.9, 0.9), Vector3(1.5, 1.1, 1.5), low, high)
	return _mesh_from_triangles(faces, colors)


func _rock_mesh() -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	_append_blob(faces, colors, Vector3(0.0, 1.2, 0.0), Vector3(2.8, 2.0, 2.1), Color(0.17, 0.165, 0.155), Color(0.33, 0.31, 0.285))
	return _mesh_from_triangles(faces, colors)


func _append_cone(faces: PackedVector3Array, colors: PackedColorArray, base: Vector3, radius: float, height: float, sides: int, low: Color, high: Color) -> void:
	var apex := base + Vector3(0.0, height, 0.0)
	for side in range(sides):
		var a := TAU * float(side) / float(sides)
		var b := TAU * float(side + 1) / float(sides)
		var first := base + Vector3(cos(a), 0.0, sin(a)) * radius
		var second := base + Vector3(cos(b), 0.0, sin(b)) * radius
		_append_triangle(faces, colors, [first, apex, second], [low, high, low], base + Vector3(0.0, height * 0.3, 0.0))


func _append_blob(faces: PackedVector3Array, colors: PackedColorArray, center: Vector3, radii: Vector3, low: Color, high: Color) -> void:
	var points: Array[Vector3] = [
		Vector3(0.0, 1.0, 0.0), Vector3(0.95, 0.2, 0.3), Vector3(0.3, 0.1, 0.95), Vector3(-0.8, 0.25, 0.55),
		Vector3(-0.75, 0.15, -0.65), Vector3(0.35, 0.3, -0.92), Vector3(0.7, -0.6, 0.35), Vector3(-0.4, -0.65, 0.6),
		Vector3(-0.6, -0.55, -0.5), Vector3(0.45, -0.6, -0.6),
	]
	var triangles: Array[Vector3i] = [
		Vector3i(0, 2, 1), Vector3i(0, 3, 2), Vector3i(0, 4, 3), Vector3i(0, 5, 4), Vector3i(0, 1, 5),
		Vector3i(1, 2, 6), Vector3i(2, 7, 6), Vector3i(2, 3, 7), Vector3i(3, 8, 7), Vector3i(3, 4, 8),
		Vector3i(4, 9, 8), Vector3i(4, 5, 9), Vector3i(5, 6, 9), Vector3i(5, 1, 6),
	]
	for triangle in triangles:
		var corners: Array[Vector3] = []
		var shades: Array[Color] = []
		for corner in [triangle.x, triangle.y, triangle.z]:
			var point: Vector3 = points[corner]
			corners.append(center + point * radii)
			shades.append(low.lerp(high, clampf(point.y * 0.5 + 0.5, 0.0, 1.0)))
		_append_triangle(faces, colors, corners, shades, center)


# Godot treats clockwise triangles as front-facing, so wind each one away from
# the center of its shape.
func _append_triangle(faces: PackedVector3Array, colors: PackedColorArray, corners: Array, shades: Array, center: Vector3) -> void:
	var a: Vector3 = corners[0]
	var b: Vector3 = corners[1]
	var c: Vector3 = corners[2]
	var flip := (c - a).cross(b - a).dot((a + b + c) / 3.0 - center) < 0.0
	faces.append_array(PackedVector3Array([a, c if flip else b, b if flip else c]))
	colors.append_array(PackedColorArray([shades[0], shades[2] if flip else shades[1], shades[1] if flip else shades[2]]))


func _mesh_from_triangles(faces: PackedVector3Array, colors: PackedColorArray) -> ArrayMesh:
	var normals := PackedVector3Array()
	normals.resize(faces.size())
	for index in range(0, faces.size(), 3):
		var normal := (faces[index + 2] - faces[index]).cross(faces[index + 1] - faces[index]).normalized()
		normals[index] = normal
		normals[index + 1] = normal
		normals[index + 2] = normal
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = faces
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh
