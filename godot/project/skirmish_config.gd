class_name SkirmishConfig
extends RefCounted

const SUPPORTED_VERSION := 1
const MAX_STARTING_UNITS := 1000
const MAX_FACTION_ID := 2
const MAX_UNIT_TYPE_ID := 10
const UNIT_TYPE_FACTIONS := {
	0: 0,
	1: 0,
	2: 0,
	3: 1,
	4: 1,
	5: 1,
	6: 2,
	7: 2,
	8: 2,
	9: 0,
	10: 0,
}


static func load_definition(path: String) -> Dictionary:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return {"ok": false, "error": "Could not open scenario definition: %s" % path}

	var parsed: Variant = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		return {"ok": false, "error": "Scenario definition must contain one JSON object"}

	var definition: Dictionary = parsed
	var validation_error := validate_definition(definition)
	if validation_error != "":
		return {"ok": false, "error": validation_error}

	return {"ok": true, "definition": definition}


static func validate_definition(definition: Dictionary) -> String:
	for required_key in ["version", "id", "display_name", "description", "theater", "player", "ai", "victory"]:
		if not definition.has(required_key):
			return "Scenario definition is missing '%s'" % required_key

	if not _is_integer_number(definition.version) or int(definition.version) != SUPPORTED_VERSION:
		return "Scenario version must be %d" % SUPPORTED_VERSION
	if not _is_nonempty_string(definition.id) or not _is_nonempty_string(definition.display_name):
		return "Scenario id and display_name must be non-empty strings"
	if not _is_nonempty_string(definition.description):
		return "Scenario description must be a non-empty string"

	if typeof(definition.theater) != TYPE_DICTIONARY:
		return "Scenario theater must be an object"
	var theater: Dictionary = definition.theater
	for dimension in ["width", "height"]:
		if not theater.has(dimension) or not _is_positive_finite_number(theater[dimension]):
			return "Scenario theater.%s must be a positive finite number" % dimension
	if typeof(theater.get("landmasses", null)) != TYPE_ARRAY or theater.landmasses.size() < 2:
		return "Scenario theater must define at least two landmasses"
	var landmass_ids: Dictionary = {}
	for landmass_value in theater.landmasses:
		var landmass_error := _validate_landmass(landmass_value, theater)
		if landmass_error != "":
			return landmass_error
		var landmass_id: String = landmass_value.id
		if landmass_ids.has(landmass_id):
			return "Scenario landmass ids must be unique"
		landmass_ids[landmass_id] = true

	var side_landmass_indices: Dictionary = {}
	for side_name in ["player", "ai"]:
		var side_error := _validate_side(side_name, definition[side_name])
		if side_error != "":
			return side_error
		var landmass_index := _landmass_index_for_point(definition[side_name].spawn, theater.landmasses)
		if landmass_index < 0:
			return "Scenario %s spawn must be on a defined landmass" % side_name
		side_landmass_indices[side_name] = landmass_index

	if int(definition.player.faction_id) == int(definition.ai.faction_id):
		return "Player and AI factions must be different"
	if side_landmass_indices.player == side_landmass_indices.ai:
		return "Player and AI spawns must use different landmasses"

	if typeof(definition.victory) != TYPE_DICTIONARY:
		return "Scenario victory must be an object"
	var victory: Dictionary = definition.victory
	if victory.get("type", "") != "last_faction_standing":
		return "Only last_faction_standing victory is supported by this scenario slice"
	if not _is_nonempty_string(victory.get("description", "")):
		return "Scenario victory description must be a non-empty string"

	return ""


static func _validate_landmass(value: Variant, theater: Dictionary) -> String:
	if typeof(value) != TYPE_DICTIONARY:
		return "Scenario landmass entries must be objects"
	var landmass: Dictionary = value
	if not _is_nonempty_string(landmass.get("id", "")):
		return "Scenario landmass id must be a non-empty string"
	for vector_name in ["center", "size"]:
		var vector_value: Variant = landmass.get(vector_name, null)
		if typeof(vector_value) != TYPE_ARRAY or vector_value.size() != 2:
			return "Scenario landmass %s must be a two-number array" % vector_name
		for component in vector_value:
			if not _is_finite_number(component):
				return "Scenario landmass %s values must be finite" % vector_name
	if float(landmass["size"][0]) <= 0.0 or float(landmass["size"][1]) <= 0.0:
		return "Scenario landmass size values must be positive"
	var half_theater_width := float(theater.width) * 0.5
	var half_theater_height := float(theater.height) * 0.5
	var half_landmass_width := float(landmass["size"][0]) * 0.5
	var half_landmass_height := float(landmass["size"][1]) * 0.5
	if absf(float(landmass.center[0])) + half_landmass_width > half_theater_width \
			or absf(float(landmass.center[1])) + half_landmass_height > half_theater_height:
		return "Scenario landmass '%s' must fit inside the theater" % landmass.id
	return ""


static func _landmass_index_for_point(point: Array, landmasses: Array) -> int:
	for index in range(landmasses.size()):
		var landmass_value: Variant = landmasses[index]
		var landmass: Dictionary = landmass_value
		var half_width := float(landmass["size"][0]) * 0.5
		var half_height := float(landmass["size"][1]) * 0.5
		if absf(float(point[0]) - float(landmass.center[0])) <= half_width \
				and absf(float(point[1]) - float(landmass.center[1])) <= half_height:
			return index
	return -1


static func _validate_side(side_name: String, value: Variant) -> String:
	if typeof(value) != TYPE_DICTIONARY:
		return "Scenario %s must be an object" % side_name
	var side: Dictionary = value
	for required_key in ["faction_id", "spawn", "units"]:
		if not side.has(required_key):
			return "Scenario %s is missing '%s'" % [side_name, required_key]

	if not _is_integer_number(side.faction_id) or int(side.faction_id) < 0 or int(side.faction_id) > MAX_FACTION_ID:
		return "Scenario %s faction_id is unsupported" % side_name
	if typeof(side.spawn) != TYPE_ARRAY or side.spawn.size() != 2:
		return "Scenario %s spawn must be a two-number array" % side_name
	if not _is_finite_number(side.spawn[0]) or not _is_finite_number(side.spawn[1]):
		return "Scenario %s spawn coordinates must be finite" % side_name
	if typeof(side.units) != TYPE_ARRAY or side.units.is_empty():
		return "Scenario %s units must be a non-empty array" % side_name

	var total_units := 0
	for unit_value in side.units:
		if typeof(unit_value) != TYPE_DICTIONARY:
			return "Scenario %s unit entries must be objects" % side_name
		var unit: Dictionary = unit_value
		if not _is_integer_number(unit.get("unit_type", null)):
			return "Scenario %s unit_type must be an integer" % side_name
		if int(unit.unit_type) < 0 or int(unit.unit_type) > MAX_UNIT_TYPE_ID:
			return "Scenario %s unit_type %d is unsupported" % [side_name, int(unit.unit_type)]
		if UNIT_TYPE_FACTIONS.get(int(unit.unit_type), -1) != int(side.faction_id):
			return "Scenario %s unit_type %d does not belong to faction %d" % [
				side_name,
				int(unit.unit_type),
				int(side.faction_id),
			]
		if not _is_integer_number(unit.get("count", null)) or int(unit.count) <= 0:
			return "Scenario %s unit count must be a positive integer" % side_name
		total_units += int(unit.count)

	if total_units > MAX_STARTING_UNITS:
		return "Scenario %s exceeds the %d-unit starting limit" % [side_name, MAX_STARTING_UNITS]
	return ""


static func _is_nonempty_string(value: Variant) -> bool:
	return typeof(value) == TYPE_STRING and not String(value).strip_edges().is_empty()


static func _is_finite_number(value: Variant) -> bool:
	return (typeof(value) == TYPE_INT or typeof(value) == TYPE_FLOAT) and is_finite(float(value))


static func _is_integer_number(value: Variant) -> bool:
	return _is_finite_number(value) and float(value) == floor(float(value))


static func _is_positive_finite_number(value: Variant) -> bool:
	return _is_finite_number(value) and float(value) > 0.0
