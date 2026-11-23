extends SceneTree
## Starts Broken Strait at every map size through the startup picker's code
## path and checks the world, the units and the AI really use that size.
## Run: godot --headless --path godot/project --script res://test_map_sizes.gd

var checks := 0
var failures := 0
var buildings_by_size := {}
var roads_by_size := {}


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	for size_km in [5, 10, 20, 40, 80]:
		var view: Node = load("res://main.tscn").instantiate()
		root.add_child(view)
		await process_frame
		await process_frame
		check(view.map_size_picker != null and view.map_size_picker.item_count == 5, "%d km: the startup screen offers five sizes" % size_km)
		var index: int = view.map_size_picker.get_item_index(size_km)
		view.map_size_picker.select(index)
		view._on_map_size_selected(index)
		var size_m := float(size_km) * 1000.0
		check(is_equal_approx(view.terrain_world_width, size_m) and is_equal_approx(view.terrain_world_height, size_m), "%d km: the theater is that size" % size_km)
		check(is_equal_approx(view.camera_max_distance, float(view.camera_zoom_levels.back())), "%d km: the widest zoom is the whole map" % size_km)
		view._on_start_skirmish_pressed()
		await process_frame
		check(view.match_started, "%d km: the match starts" % size_km)
		var player_spawn: Array = view.scenario_definition.player.spawn
		var player_post := int(view.commander_ids.get(0, -1))
		check(player_post > 0, "%d km: the player's commander exists" % size_km)
		if player_post > 0:
			var position: Vector2 = view._native_unit_position(player_post)
			check(position.distance_to(Vector2(float(player_spawn[0]), float(player_spawn[1]))) < 400.0 * maxf(1.0, size_m / 40000.0), "%d km: the commander starts at the scaled spawn" % size_km)
			check(absf(position.x) < size_m * 0.5 and absf(position.y) < size_m * 0.5, "%d km: the commander is on the map" % size_km)
		# Heights match the scaled terrain in both the native world and the view.
		var native_heights: PackedFloat32Array = view.extension.call("get_terrain_heights")
		check(native_heights.size() == view.terrain_heights.size() and not native_heights.is_empty(), "%d km: terrain heights agree" % size_km)
		if native_heights.size() == view.terrain_heights.size() and not native_heights.is_empty():
			var worst := 0.0
			for height_index in range(0, native_heights.size(), 257):
				worst = maxf(worst, absf(native_heights[height_index] - view.terrain_heights[height_index]))
			check(worst < 0.01, "%d km: native and view heights match (worst %.4f)" % [size_km, worst])
		for mode in [1, 2]:
			check(bool(view.extension.call("transport_route", 0, mode).get("enabled", false)), "%d km: the player's transport route %d is usable" % [size_km, mode])
			check(bool(view.extension.call("transport_route", 1, mode).get("enabled", false)), "%d km: the enemy's transport route %d is usable" % [size_km, mode])
		roads_by_size[size_km] = int(view.public_road_count)
		check(int(view.public_road_count) >= 3, "%d km: the map has roads (%d)" % [size_km, int(view.public_road_count)])
		var drawn_roads: PackedFloat32Array = view.extension.call("get_road_segments")
		check(drawn_roads.size() / 6 >= int(view.public_road_count), "%d km: every road is reported to the view" % size_km)
		buildings_by_size[size_km] = int(view.civilian_building_instance_count)
		check(int(view.civilian_building_instance_count) >= 30, "%d km: the land has buildings (%d)" % [size_km, int(view.civilian_building_instance_count)])
		for _tick_index in range(200):
			view.extension.call("update_simulation", 50.0)
			view._update_match_state()
		check(not view.match_finished, "%d km: ten seconds in, the match is still running" % size_km)
		view.queue_free()
		await process_frame
	print("MAP_SIZES buildings by size: ", buildings_by_size, " roads by size: ", roads_by_size)
	check(buildings_by_size[80] > buildings_by_size[20] and buildings_by_size[20] > buildings_by_size[5], "bigger maps get more buildings")
	print("MAP_SIZES checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
