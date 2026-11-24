class_name StrategicIconOverlay
extends Control

const UnitCatalog := preload("res://unit_catalog.gd")

## Screen-space strategic markers. These are deliberately independent from the
## 3D unit meshes so they remain a fixed, legible size at full-map zoom.

const STRATEGIC_ZOOM_DISTANCE := 600.0
const LAST_KNOWN_COLOR := Color("#ff8a6b")
const LAST_KNOWN_MERGE_PIXELS := 42.0
const LAST_KNOWN_FADE_SECONDS := 180.0
var match_view: Node3D


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _process(_delta: float) -> void:
	queue_redraw()


func _draw() -> void:
	if match_view == null or not match_view.match_started:
		return
	var camera: Camera3D = match_view.camera
	var viewport_bounds := Rect2(Vector2.ZERO, size)
	_draw_last_known(camera, viewport_bounds)
	if match_view.camera_distance < STRATEGIC_ZOOM_DISTANCE:
		return
	for entity_id in match_view.entity_ids:
		if not match_view._is_unit_shown(entity_id):
			continue
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


# Enemies that dropped out of sight, drawn at every zoom. Marks that would overlap
# on screen merge into one with a count, keeping the newest age.
func _draw_last_known(camera: Camera3D, viewport_bounds: Rect2) -> void:
	var contacts: PackedFloat32Array = match_view.last_known_contacts
	var merged: Dictionary = {}
	for index in range(0, contacts.size() - 3, 4):
		var world_x := contacts[index]
		var world_z := contacts[index + 1]
		var world := Vector3(world_x, float(match_view._terrain_height_at(world_x, world_z)) + 5.0, world_z)
		if camera.is_position_behind(world):
			continue
		var anchor := camera.unproject_position(world)
		if not viewport_bounds.grow(30.0).has_point(anchor):
			continue
		var key := Vector2i(anchor / LAST_KNOWN_MERGE_PIXELS)
		var entry: Dictionary = merged.get(key, {"anchor": anchor, "age": contacts[index + 2], "count": 0})
		entry.age = minf(float(entry.age), contacts[index + 2])
		entry.count = int(entry.count) + 1
		merged[key] = entry
	var font := ThemeDB.fallback_font
	for entry in merged.values():
		var center: Vector2 = entry.anchor
		var age := float(entry.age)
		var color := LAST_KNOWN_COLOR
		color.a = lerpf(0.95, 0.4, clampf(age / LAST_KNOWN_FADE_SECONDS, 0.0, 1.0))
		var ring := _regular_points(center, 12.0, 4, 0.0)
		ring.append(ring[0])
		draw_polyline(ring, color, 1.5, true)
		draw_string(font, center + Vector2(-3.0, 5.0), "?", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, color)
		var label := "%ds" % int(age) if age < 60.0 else "%dm" % int(age / 60.0)
		if int(entry.count) > 1:
			label = "%d · %s" % [int(entry.count), label]
		draw_string(font, center + Vector2(15.0, 4.0), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, color)


func _draw_icon(unit_type: int, center: Vector2, faction_color: Color) -> void:
	var fill := Color("#b8bec0")
	var outline := faction_color.lightened(0.25)
	match UnitCatalog.strategic_icon(unit_type):
		"mbt":
			_draw_polygon(_rect_points(center, Vector2(9, 12)), fill, outline)
		"artillery":
			_draw_polygon(_rect_points(center, Vector2(5, 17)), fill, outline)
		"air_defense":
			_draw_polygon(_regular_points(center, 9, 4, PI * 0.25), fill, outline)
		"swarm":
			_draw_polygon(_regular_points(center, 9, 6), fill, outline)
		"assault":
			_draw_polygon(_rect_points(center, Vector2(14, 8)), fill, outline)
		"missile":
			_draw_polygon(_regular_points(center, 10, 4, PI * 0.25), fill, outline)
		"engineer": # Octagonal utility marker with a clear center cross
			_draw_polygon(_regular_points(center, 10, 8), fill, outline)
			draw_line(center + Vector2(-4, 0), center + Vector2(4, 0), Color("#30383a"), 2.0, true)
			draw_line(center + Vector2(0, -4), center + Vector2(0, 4), Color("#30383a"), 2.0, true)
		"fighter": # Acute triangle
			_draw_polygon(PackedVector2Array([center + Vector2(0, -12), center + Vector2(-7, 8), center + Vector2(7, 8)]), fill, outline)
		"vtol":
			_draw_polygon(_regular_points(center, 10, 4), fill, outline)
		"boat":
			_draw_polygon(PackedVector2Array([center + Vector2(0, -10), center + Vector2(9, -3), center + Vector2(6, 9), center + Vector2(-6, 9), center + Vector2(-9, -3)]), fill, outline)
		"command":
			_draw_polygon(_regular_points(center, 11, 6), fill, outline)
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
