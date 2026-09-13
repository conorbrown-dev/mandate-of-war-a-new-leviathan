class_name MapEditorModel
extends RefCounted

# Standalone editor-state model. It deliberately has no dependency on the
# skirmish scene, so map authoring can evolve without changing match startup.
var map_id := "untitled"
var width := 0
var height := 0
var tile_size := 16.0
var bounds := Rect2()
var terrain: PackedFloat32Array = PackedFloat32Array()
var water_cells: Dictionary = {}
var spawn_points: Array[Dictionary] = []
var resources: Array[Dictionary] = []
var entities: Array[Dictionary] = []

func create_map(id: String, world_width: int, world_height: int, cell_size: float) -> bool:
	if id.is_empty() or world_width <= 0 or world_height <= 0 or cell_size <= 0.0:
		return false
	if world_width % int(cell_size) != 0 or world_height % int(cell_size) != 0:
		return false
	map_id = id
	width = world_width
	height = world_height
	tile_size = cell_size
	bounds = Rect2(0.0, 0.0, width, height)
	terrain.resize((width / int(tile_size)) * (height / int(tile_size)))
	terrain.fill(0.0)
	water_cells.clear()
	spawn_points.clear()
	resources.clear()
	entities.clear()
	return true

func set_height(cell_x: int, cell_y: int, value: float) -> bool:
	var index := _cell_index(cell_x, cell_y)
	if index < 0 or not is_finite(value):
		return false
	terrain[index] = value
	return true

func set_water(cell_x: int, cell_y: int, enabled: bool) -> bool:
	if _cell_index(cell_x, cell_y) < 0:
		return false
	var key := Vector2i(cell_x, cell_y)
	if enabled:
		water_cells[key] = true
	else:
		water_cells.erase(key)
	return true

func add_spawn(id: String, faction: String, position: Vector2, heading := 0.0, type := "land") -> bool:
	if id.is_empty() or faction.is_empty() or not bounds.has_point(position) or _has_id(spawn_points, id):
		return false
	if type != "land" and type != "naval" and type != "airbase":
		return false
	spawn_points.append({"id": id, "faction": faction, "position": position, "heading": heading, "type": type})
	return true

func add_resource(id: String, type: String, position: Vector2, amount: float, radius: float) -> bool:
	if id.is_empty() or type.is_empty() or not bounds.has_point(position) or amount <= 0.0 or radius <= 0.0 or _has_id(resources, id):
		return false
	resources.append({"id": id, "type": type, "position": position, "amount": amount, "radius": radius})
	return true

func add_entity(id: String, content_id: String, position: Vector2) -> bool:
	if id.is_empty() or content_id.is_empty() or not bounds.has_point(position) or _has_id(entities, id):
		return false
	entities.append({"id": id, "content_id": content_id, "position": position})
	return true

func validate() -> PackedStringArray:
	var errors := PackedStringArray()
	if map_id.is_empty() or terrain.is_empty() or width <= 0 or height <= 0:
		errors.append("Map dimensions and terrain are required")
	for spawn in spawn_points:
		if not bounds.has_point(spawn.position): errors.append("Spawn outside playable bounds: %s" % spawn.id)
	for resource in resources:
		if not bounds.has_point(resource.position): errors.append("Resource outside playable bounds: %s" % resource.id)
	return errors

func save_to_file(path: String) -> bool:
	if not validate().is_empty(): return false
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null: return false
	file.store_string(JSON.stringify(to_dictionary()))
	return file.get_error() == OK

func load_from_file(path: String) -> bool:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null: return false
	var parsed = JSON.parse_string(file.get_as_text())
	if not parsed is Dictionary: return false
	var data: Dictionary = parsed
	if not create_map(String(data.get("id", "")), int(data.get("width", 0)), int(data.get("height", 0)), float(data.get("tile_size", 0.0))): return false
	var heights: Array = data.get("terrain", [])
	if heights.size() != terrain.size(): return false
	for index in heights.size(): terrain[index] = float(heights[index])
	for cell in data.get("water", []):
		if not set_water(int(cell[0]), int(cell[1]), true): return false
	for entry in data.get("spawns", []):
		if not add_spawn(String(entry.id), String(entry.faction), Vector2(entry.position[0], entry.position[1]), float(entry.get("heading", 0.0)), String(entry.get("type", "land"))): return false
	for entry in data.get("resources", []):
		if not add_resource(String(entry.id), String(entry.type), Vector2(entry.position[0], entry.position[1]), float(entry.amount), float(entry.radius)): return false
	for entry in data.get("entities", []):
		if not add_entity(String(entry.id), String(entry.content_id), Vector2(entry.position[0], entry.position[1])): return false
	return validate().is_empty()

func to_dictionary() -> Dictionary:
	var serialize := func(items: Array[Dictionary]) -> Array:
		var output: Array = []
		for item in items:
			var copy: Dictionary = item.duplicate(true)
			if copy.has("position"):
				var position: Vector2 = copy.position
				copy.position = [position.x, position.y]
			output.append(copy)
		return output
	var water: Array = []
	for key in water_cells.keys(): water.append([key.x, key.y])
	water.sort_custom(func(a, b): return a[1] < b[1] or (a[1] == b[1] and a[0] < b[0]))
	return {"id": map_id, "width": width, "height": height, "tile_size": tile_size, "terrain": Array(terrain), "water": water, "spawns": serialize.call(spawn_points), "resources": serialize.call(resources), "entities": serialize.call(entities)}

func _cell_index(cell_x: int, cell_y: int) -> int:
	var columns := width / int(tile_size)
	var rows := height / int(tile_size)
	if cell_x < 0 or cell_y < 0 or cell_x >= columns or cell_y >= rows:
		return -1
	return cell_y * columns + cell_x

func _has_id(items: Array[Dictionary], id: String) -> bool:
	for item in items:
		if item.id == id: return true
	return false
