class_name StrategicIconOverlay
extends Control

## Screen-space strategic markers. These are deliberately independent from the
## 3D unit meshes so they remain a fixed, legible size at full-map zoom.

const STRATEGIC_ZOOM_DISTANCE := 600.0
var match_view: Node3D


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _process(_delta: float) -> void:
	queue_redraw()


func _draw() -> void:
	if match_view == null or not match_view.match_started or match_view.camera_distance < STRATEGIC_ZOOM_DISTANCE:
		return
	var camera: Camera3D = match_view.camera
	var viewport_bounds := Rect2(Vector2.ZERO, size)
	for entity_id in match_view.entity_ids:
		var instance_index := int(match_view.entity_to_instance.get(entity_id, -1))
		if instance_index < 0 or instance_index * 2 + 1 >= match_view.unit_positions.size():
			continue
		var world_x := float(match_view.unit_positions[instance_index * 2])
		var world_z := float(match_view.unit_positions[instance_index * 2 + 1])
		var world := Vector3(world_x, float(match_view._terrain_height_at(world_x, world_z)) + 5.0, world_z)
		if camera.is_position_behind(world):
			continue
		var anchor := camera.unproject_position(world)
		if not viewport_bounds.grow(30.0).has_point(anchor):
			continue
		var icon_center := anchor + Vector2(0.0, -22.0)
		var faction_color: Color = match_view.entity_base_colors.get(entity_id, Color.WHITE)
		var leader_color := Color("#b8bec0")
		leader_color.a = 0.82
		draw_line(anchor, icon_center + Vector2(0.0, 7.0), leader_color, 1.5, true)
		draw_circle(anchor, 2.0, faction_color)
		_draw_icon(int(match_view.entity_unit_types.get(entity_id, 0)), icon_center, faction_color)


func _draw_icon(unit_type: int, center: Vector2, faction_color: Color) -> void:
	var fill := Color("#b8bec0")
	var outline := faction_color.lightened(0.25)
	match unit_type:
		0, 6: # MBTs
			_draw_polygon(_rect_points(center, Vector2(9, 12)), fill, outline)
		1: # Artillery
			_draw_polygon(_rect_points(center, Vector2(5, 17)), fill, outline)
		2, 5: # Anti-air
			_draw_polygon(_regular_points(center, 9, 4, PI * 0.25), fill, outline)
		3: # Swarm tank
			_draw_polygon(_regular_points(center, 9, 6), fill, outline)
		4: # Assault vehicle
			_draw_polygon(_rect_points(center, Vector2(14, 8)), fill, outline)
		7: # Missile platform
			_draw_polygon(_regular_points(center, 10, 4, PI * 0.25), fill, outline)
		8: # Engineer: octagonal utility marker with a clear center cross
			_draw_polygon(_regular_points(center, 10, 8), fill, outline)
			draw_line(center + Vector2(-4, 0), center + Vector2(4, 0), Color("#30383a"), 2.0, true)
			draw_line(center + Vector2(0, -4), center + Vector2(0, 4), Color("#30383a"), 2.0, true)
		9: # Fighter: acute triangle
			_draw_polygon(PackedVector2Array([center + Vector2(0, -12), center + Vector2(-7, 8), center + Vector2(7, 8)]), fill, outline)
		10: # VTOL
			_draw_polygon(_regular_points(center, 10, 4), fill, outline)
		11: # Patrol boat
			_draw_polygon(PackedVector2Array([center + Vector2(0, -10), center + Vector2(9, -3), center + Vector2(6, 9), center + Vector2(-6, 9), center + Vector2(-9, -3)]), fill, outline)
		12: # Command Walker
			_draw_polygon(_regular_points(center, 11, 6), fill, outline)
		13: # Reserved bomber: broad obtuse triangle
			_draw_polygon(PackedVector2Array([center + Vector2(0, -8), center + Vector2(-12, 8), center + Vector2(12, 8)]), fill, outline)
		_:
			_draw_polygon(_regular_points(center, 9, 6), fill, outline)


func _draw_polygon(points: PackedVector2Array, fill: Color, outline: Color) -> void:
	draw_colored_polygon(points, fill)
	var closed := PackedVector2Array(points)
	closed.append(points[0])
	draw_polyline(closed, outline, 1.5, true)


func _regular_points(center: Vector2, radius: float, sides: int, rotation := -PI * 0.5) -> PackedVector2Array:
	var points := PackedVector2Array()
	for index in range(sides):
		var angle := rotation + TAU * float(index) / float(sides)
		points.append(center + Vector2(cos(angle), sin(angle)) * radius)
	return points


func _rect_points(center: Vector2, extent: Vector2) -> PackedVector2Array:
	return PackedVector2Array([
		center + Vector2(-extent.x * 0.5, -extent.y * 0.5),
		center + Vector2(extent.x * 0.5, -extent.y * 0.5),
		center + Vector2(extent.x * 0.5, extent.y * 0.5),
		center + Vector2(-extent.x * 0.5, extent.y * 0.5),
	])
