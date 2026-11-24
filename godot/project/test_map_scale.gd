extends SceneTree
## Checks the map-size scaling of the Broken Strait scenario at every size.
## Run: godot --headless --path godot/project --script res://test_map_scale.gd

const MapScale := preload("res://map_scale.gd")
const SkirmishConfigLoader := preload("res://skirmish_config.gd")

var failures := 0


func check(condition: bool, message: String) -> void:
	if not condition:
		failures += 1
		print("FAIL: ", message)


func _init() -> void:
	var loaded: Dictionary = SkirmishConfigLoader.load_definition("res://scenarios/commander_start_skirmish.json")
	check(bool(loaded.get("ok", false)), "the authored scenario loads")
	if not loaded.get("ok", false):
		quit(1)
		return
	var base: Dictionary = loaded.definition
	for size_km in MapScale.SIZES_KM:
		var size_m := MapScale.size_meters(size_km)
		var scale := MapScale.factor(size_m)
		var scaled: Dictionary = MapScale.scale_definition(base, size_m)
		check(is_equal_approx(float(scaled.theater.width), size_m) and is_equal_approx(float(scaled.theater.height), size_m), "%d km theater is square at that size" % size_km)
		var player_spawn: Array = scaled.player.spawn
		check(is_equal_approx(float(player_spawn[0]), float(base.player.spawn[0]) * scale), "%d km player spawn scales" % size_km)
		var gap := Vector2(float(scaled.ai.spawn[0]) - float(player_spawn[0]), float(scaled.ai.spawn[1]) - float(player_spawn[1])).length()
		var base_gap := Vector2(float(base.ai.spawn[0]) - float(base.player.spawn[0]), float(base.ai.spawn[1]) - float(base.player.spawn[1])).length()
		check(is_equal_approx(gap, base_gap * scale), "%d km spawn separation scales" % size_km)
		for landmass: Dictionary in scaled.theater.landmasses:
			check(absf(float(landmass.center[0])) + float(landmass.size[0]) * 0.5 <= size_m * 0.5 + 0.01, "%d km landmass %s fits the theater" % [size_km, landmass.id])
			check(absf(float(landmass.center[1])) + float(landmass.size[1]) * 0.5 <= size_m * 0.5 + 0.01, "%d km landmass %s fits vertically" % [size_km, landmass.id])
		var area: Array = scaled.ai.assault.search_area
		check(float(area[0]) >= -size_m * 0.5 and float(area[2]) <= size_m * 0.5, "%d km search area stays on the map" % size_km)
		check(not scaled.transport_routes.is_empty(), "%d km keeps its transport routes" % size_km)
		var levels: Array = MapScale.camera_levels(scale)
		for index in range(1, levels.size()):
			check(float(levels[index]) > float(levels[index - 1]), "%d km zoom stops rise" % size_km)
		check(is_equal_approx(float(levels.back()), maxf(26000.0 * scale, 1200.0)), "%d km top zoom shows the whole map" % size_km)
	check(MapScale.camera_levels(1.0) == MapScale.REFERENCE_CAMERA_LEVELS, "40 km zoom stops are unchanged")
	# The base definition must not be changed by scaling.
	check(is_equal_approx(float(base.theater.width), 40000.0), "scaling leaves the authored scenario untouched")
	print("map scale test: %s" % ("PASS" if failures == 0 else "FAIL (%d)" % failures))
	quit(1 if failures > 0 else 0)
