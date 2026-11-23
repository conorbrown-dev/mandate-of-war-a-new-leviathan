extends RefCounted
## Redraws the authored 40 km Broken Strait at another square size. Every
## coordinate scales by the same factor, so the layout is the same and only the
## distances change: spawns are 0.9 km apart at 5 km and 14 km apart at 80 km.
## Presentation and scenario data only; the simulation sees the scaled numbers.

const REFERENCE_SIZE_M := 40000.0
const SIZES_KM := [5, 10, 20, 40, 80]
const DEFAULT_SIZE_KM := 40
const REFERENCE_CAMERA_LEVELS := [24.0, 120.0, 420.0, 1500.0, 4500.0, 12000.0, 26000.0]


static func size_meters(size_km: int) -> float:
	return float(size_km) * 1000.0


static func factor(size_m: float) -> float:
	return size_m / REFERENCE_SIZE_M


static func point(value: Variant, scale: float) -> Array:
	return [float(value[0]) * scale, float(value[1]) * scale]


static func vec(value: Vector2, scale: float) -> Vector2:
	return value * scale


## Returns a scaled deep copy of a validated scenario definition.
static func scale_definition(definition: Dictionary, size_m: float) -> Dictionary:
	var scale := factor(size_m)
	var scaled: Dictionary = definition.duplicate(true)
	var theater: Dictionary = scaled.get("theater", {})
	theater["width"] = size_m
	theater["height"] = size_m
	for landmass: Dictionary in theater.get("landmasses", []):
		landmass["center"] = point(landmass.center, scale)
		landmass["size"] = point(landmass.size, scale)
	for side_name in ["player", "ai"]:
		var side: Dictionary = scaled.get(side_name, {})
		if side.has("spawn"):
			side["spawn"] = point(side.spawn, scale)
		var assault: Dictionary = side.get("assault", {})
		if assault.has("search_area"):
			var area: Array = assault.search_area
			assault["search_area"] = [float(area[0]) * scale, float(area[1]) * scale, float(area[2]) * scale, float(area[3]) * scale]
	for route: Dictionary in scaled.get("transport_routes", []):
		for mode_name in ["road", "rail"]:
			var mode: Dictionary = route.get(mode_name, {})
			for key in ["entry", "terminal"]:
				if mode.has(key):
					mode[key] = point(mode[key], scale)
	scaled["map_scale"] = scale
	return scaled


## Zoom stops for a map: the fixed close-up stops, then whatever the map is
## large enough for, ending at the whole map. Identical to the reference list at 40 km.
static func camera_levels(scale: float) -> Array:
	var full_map: float = maxf(REFERENCE_CAMERA_LEVELS.back() * scale, 1200.0)
	var levels: Array = []
	for level: float in REFERENCE_CAMERA_LEVELS:
		if level < full_map * 0.9:
			levels.append(level)
	levels.append(full_map)
	return levels
