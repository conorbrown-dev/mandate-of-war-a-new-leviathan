extends RefCounted
## Read-only view of data/unit_faction_stats.json for presentation code, so a
## new unit in that file needs no script changes.

const FACTION_KEYS := ["ELITE_PRECISION", "MASS_WARFARE", "INDUSTRIAL_EXPERIMENTAL"]
# The Command Walker is a commander presentation, not a unit data entry.
const COMMANDER_UNIT_TYPE := 12

static var _units: Dictionary = {}


static func units() -> Dictionary:
	if _units.is_empty():
		_load()
	return _units


static func has_unit(unit_type: int) -> bool:
	return units().has(unit_type)


static func faction_index(unit_type: int) -> int:
	return FACTION_KEYS.find(String(units().get(unit_type, {}).get("faction", "")))


static func strategic_icon(unit_type: int) -> String:
	if unit_type == COMMANDER_UNIT_TYPE:
		return "command"
	return String(units().get(unit_type, {}).get("strategic_icon", ""))


static func is_construction_unit(unit_type: int) -> bool:
	return bool(units().get(unit_type, {}).get("construction_unit", false))


static func _load() -> void:
	var root := OS.get_environment("RTS_DATA_ROOT")
	if root.is_empty():
		root = ProjectSettings.globalize_path("res://").path_join("../../data")
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(root.path_join("unit_faction_stats.json")))
	if typeof(parsed) != TYPE_DICTIONARY:
		push_error("Unit data could not be read from %s" % root)
		return
	for unit: Dictionary in parsed.get("unit_types", []):
		_units[int(unit.id)] = unit
