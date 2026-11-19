class_name StrategicIconOverlay
extends Control

const UnitCatalog := preload("res://unit_catalog.gd")

## Screen-space strategic markers. These are deliberately independent from the
## 3D unit meshes so they remain a fixed, legible size at full-map zoom.

const STRATEGIC_ZOOM_DISTANCE := 600.0
const LAST_KNOWN_COLOR := Color("#ff8a6b")
const LAST_KNOWN_MERGE_PIXELS := 42.0
const LAST_KNOWN_FADE_SECONDS := 180.0
const NatoMapSymbolScript := preload("res://nato_map_symbol.gd")
# Resource sites, marked at every zoom. Owner colours are the APP-6 friendly and
# hostile fills, with amber for a site last seen unclaimed.
const RESOURCE_UNCLAIMED_FILL := Color("#ffd27a")
const RESOURCE_ICON_OFFSET := Vector2(0.0, -20.0)
var match_view: Node3D
# What the last _draw put on screen, for contracts: one entry per marker.
var last_unit_symbols: Array[Dictionary] = []
var last_resource_markers: Array[Dictionary] = []
var last_power_markers: Array[Dictionary] = []


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _process(_delta: float) -> void:
	queue_redraw()


func _draw() -> void:
	if match_view == null or not match_view.match_started:
		return
	var camera: Camera3D = match_view.camera
	var viewport_bounds := Rect2(Vector2.ZERO, size)
	last_unit_symbols.clear()
	last_resource_markers.clear()
	_draw_resource_markers(camera, viewport_bounds)
	last_power_markers.clear()
	_draw_power_markers(camera, viewport_bounds)
	_draw_last_known(camera, viewport_bounds)
	_draw_radar_contacts(camera, viewport_bounds)
	if match_view.camera_distance < STRATEGIC_ZOOM_DISTANCE:
		return
	var symbol_font: Font = preload("res://ui/theme/mandate_fonts.gd").label()
	for entity_id in match_view.entity_ids:
		if not match_view._is_unit_shown(entity_id):
			continue
		var instance_index := int(match_view.entity_to_instance.get(entity_id, -1))
		if instance_index < 0 or instance_index * 2 + 1 >= match_view.unit_positions.size():
			continue
		var placement := marker_placement(entity_id, camera)
		var world: Vector3 = placement.world
		if camera.is_position_behind(world):
			continue
		var anchor: Vector2 = placement.anchor
		if not viewport_bounds.grow(30.0).has_point(anchor):
			continue
		var icon_center: Vector2 = placement.icon_center
		var faction_color: Color = match_view.entity_base_colors.get(entity_id, Color.WHITE)
		var leader_color := Color("#b8bec0")
		leader_color.a = 0.82
		draw_line(anchor, icon_center + Vector2(0.0, 7.0), leader_color, 1.5, true)
		draw_circle(anchor, 2.0, faction_color)
		var hostile: bool = match_view.ai_entity_ids.has(entity_id)
		var symbol: Dictionary = NatoMapSymbolScript.draw(self, icon_center, int(match_view.entity_unit_types.get(entity_id, 0)), hostile, symbol_font)
		symbol["entity_id"] = entity_id
		symbol["center"] = icon_center
		last_unit_symbols.append(symbol)


func marker_placement(entity_id: int, camera: Camera3D) -> Dictionary:
	var world: Vector3 = match_view._unit_overlay_anchor(entity_id)
	var anchor := camera.unproject_position(world)
	return {"world": world, "anchor": anchor, "icon_center": anchor+Vector2(0,-22)}


func _draw_radar_contacts(camera: Camera3D, viewport_bounds: Rect2) -> void:
	var contacts: PackedFloat32Array = match_view.radar_contacts
	var occupied: Dictionary = {}
	for index in range(0, contacts.size()-1, 2):
		var x := contacts[index]
		var z := contacts[index+1]
		var world := Vector3(x, float(match_view._terrain_height_at(x,z))+5.0, z)
		if camera.is_position_behind(world):
			continue
		var center := camera.unproject_position(world)
		if not viewport_bounds.grow(20.0).has_point(center):
			continue
		var bucket := Vector2i(center / LAST_KNOWN_MERGE_PIXELS)
		if occupied.has(bucket):
			continue
		occupied[bucket] = true
		var points := _regular_points(center, 10.0, 6)
		points.append(points[0])
		draw_polyline(points, Color("#a04dff"), 1.5, true)
		draw_string(ThemeDB.fallback_font, center+Vector2(-3,5), "?", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color("#a04dff"))


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
	var font: Font = preload("res://ui/theme/mandate_fonts.gd").label()
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


## Sites appear only after scouting. Their marker and last-seen owner persist
## when sight is lost; hidden ownership changes never leak through the map.
func resource_markers(camera: Camera3D, viewport_bounds: Rect2) -> Array[Dictionary]:
	var markers: Array[Dictionary] = []
	for site: Dictionary in match_view.material_site_state.values():
		if bool(site.get("destroyed", false)) or not bool(site.get("discovered", false)):
			continue
		_append_resource_marker(markers, site, "materials", camera, viewport_bounds)
	for site: Dictionary in match_view.fuel_depot_state.values():
		if not bool(site.get("discovered", false)):
			continue
		_append_resource_marker(markers, site, "fuel", camera, viewport_bounds)
	return markers


func _append_resource_marker(markers: Array[Dictionary], site: Dictionary, kind: String, camera: Camera3D, viewport_bounds: Rect2) -> void:
	var position: Vector2 = site.position
	var world := Vector3(position.x, float(match_view._terrain_height_at(position.x, position.y)) + 3.0, position.y)
	if camera.is_position_behind(world):
		return
	var anchor := camera.unproject_position(world)
	if not viewport_bounds.grow(30.0).has_point(anchor):
		return
	var known_owner := int(site.get("known_owner", -1))
	var affiliation := "unclaimed" if known_owner < 0 else ("friendly" if known_owner == match_view.HUMAN_PLAYER_ID else "hostile")
	markers.append({
		"id": int(site.id), "kind": kind, "site_kind": String(site.get("kind", "materials")),
		"known_owner": known_owner, "affiliation": affiliation,
		"anchor": anchor, "center": anchor + RESOURCE_ICON_OFFSET,
	})


func _draw_resource_markers(camera: Camera3D, viewport_bounds: Rect2) -> void:
	var font: Font = preload("res://ui/theme/mandate_fonts.gd").label()
	for marker in resource_markers(camera, viewport_bounds):
		var anchor: Vector2 = marker.anchor
		var center: Vector2 = marker.center
		var fill: Color = RESOURCE_UNCLAIMED_FILL
		if marker.affiliation == "friendly":
			fill = NatoMapSymbolScript.FRIENDLY_FILL
		elif marker.affiliation == "hostile":
			fill = NatoMapSymbolScript.HOSTILE_FILL
		var ink := NatoMapSymbolScript.LINE_COLOR
		draw_line(anchor, center + Vector2(0.0, 9.0), Color(0.0, 0.0, 0.0, 0.7), 1.5, true)
		draw_circle(anchor, 2.5, fill)
		var outline: PackedVector2Array
		if marker.kind == "fuel":
			# Round badge with a fuel drop (POL).
			outline = _regular_points(center, 10.0, 20)
			draw_colored_polygon(outline, fill)
			draw_circle(center + Vector2(0.0, 2.0), 4.2, ink)
			draw_colored_polygon(PackedVector2Array([center + Vector2(0.0, -7.0), center + Vector2(3.9, 0.6), center + Vector2(-3.9, 0.6)]), ink)
		else:
			# Square badge with stacked ingots (Materials).
			outline = _rect_points(center, Vector2(19.0, 19.0))
			draw_colored_polygon(outline, fill)
			draw_rect(Rect2(center + Vector2(-6.5, 1.0), Vector2(13.0, 4.5)), ink)
			draw_rect(Rect2(center + Vector2(-4.0, -5.0), Vector2(8.0, 4.5)), ink)
		outline.append(outline[0])
		draw_polyline(outline, Color(0.0, 0.0, 0.0, 0.55), 3.2, true)
		draw_polyline(outline, ink, 1.6, true)
		var label := "FUEL" if marker.kind == "fuel" else "MATERIALS"
		if marker.site_kind == "military":
			label = "FUEL MIL"
		var extent := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10)
		var text_at := center + Vector2(-extent.x * 0.5, -14.0)
		draw_string_outline(font, text_at, label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, 3, Color(0.0, 0.0, 0.0, 0.8))
		draw_string(font, text_at, label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, fill)
		last_resource_markers.append(marker)


## Power stations the player's side knows of, with the holder last seen.
func power_markers(camera: Camera3D, viewport_bounds: Rect2) -> Array[Dictionary]:
	var markers: Array[Dictionary] = []
	var grid: Node3D = match_view.power_grid_view
	if grid == null or not grid.active:
		return markers
	for node: Dictionary in grid.nodes.values():
		if int(node.kind) != 0 or not bool(node.discovered):
			continue
		_append_resource_marker(markers, {"id": int(node.id), "position": node.position, "kind": "power_station",
			"known_owner": int(node.known_owner)}, "power", camera, viewport_bounds)
	return markers


func _draw_power_markers(camera: Camera3D, viewport_bounds: Rect2) -> void:
	var font: Font = preload("res://ui/theme/mandate_fonts.gd").label()
	for marker in power_markers(camera, viewport_bounds):
		var anchor: Vector2 = marker.anchor
		var center: Vector2 = marker.center
		var fill: Color = RESOURCE_UNCLAIMED_FILL
		if marker.affiliation == "friendly":
			fill = NatoMapSymbolScript.FRIENDLY_FILL
		elif marker.affiliation == "hostile":
			fill = NatoMapSymbolScript.HOSTILE_FILL
		var ink := NatoMapSymbolScript.LINE_COLOR
		draw_line(anchor, center + Vector2(0.0, 11.0), Color(0.0, 0.0, 0.0, 0.7), 1.5, true)
		draw_circle(anchor, 2.5, fill)
		# Diamond badge with a lightning bolt.
		var outline := _regular_points(center, 12.0, 4)
		draw_colored_polygon(outline, fill)
		draw_colored_polygon(PackedVector2Array([
			center + Vector2(1.8, -7.0), center + Vector2(-3.6, 1.0), center + Vector2(-0.4, 1.0),
			center + Vector2(-1.8, 7.0), center + Vector2(3.6, -1.2), center + Vector2(0.4, -1.2)]), ink)
		outline.append(outline[0])
		draw_polyline(outline, Color(0.0, 0.0, 0.0, 0.55), 3.2, true)
		draw_polyline(outline, ink, 1.6, true)
		var label := "POWER"
		var extent := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10)
		var text_at := center + Vector2(-extent.x * 0.5, -16.0)
		draw_string_outline(font, text_at, label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, 3, Color(0.0, 0.0, 0.0, 0.8))
		draw_string(font, text_at, label, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, fill)
		last_power_markers.append(marker)


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
