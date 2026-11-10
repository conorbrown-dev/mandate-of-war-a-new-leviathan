extends RefCounted
## Hover descriptions for the build menus, from ui/build_descriptions.json.
## Native catalog entries carry a numeric type; units resolve to their type key
## (ELITE_MAIN_BATTLE_TANK) through data/unit_faction_stats.json.

const UnitCatalog := preload("res://unit_catalog.gd")
const PATH := "res://ui/build_descriptions.json"
const STRUCTURE_KEYS := ["FORWARD_OUTPOST", "RADAR_MAST", "AIRFIELD", "FLOODLIGHT"]

static var _data: Dictionary = {}


static func data() -> Dictionary:
	if _data.is_empty():
		var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(PATH))
		if typeof(parsed) == TYPE_DICTIONARY:
			_data = parsed
		else:
			push_error("Build descriptions could not be read from %s" % PATH)
	return _data


## The key a catalog entry is described under, or "" when it has none.
static func key_for(entry: Dictionary) -> String:
	var type := int(entry.get("type", -1))
	if bool(entry.get("is_structure", false)):
		var structure := int(entry.get("structure_type", type - 100))
		return STRUCTURE_KEYS[structure] if structure >= 0 and structure < STRUCTURE_KEYS.size() else ""
	return String(UnitCatalog.units().get(type, {}).get("type", ""))


static func for_entry(entry: Dictionary) -> String:
	var key := key_for(entry)
	if key.is_empty():
		return ""
	var group := "structures" if bool(entry.get("is_structure", false)) else "units"
	return String(data().get(group, {}).get(key, ""))
