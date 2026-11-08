extends SceneTree
## Frame rate over forest, with and without the near tree models, in one run.
## Opens a window; vsync is off. Says nothing about simulation scale.
##
##   $env:RTS_AUTO_START_SKIRMISH = "1"
##   godot --path godot/project --resolution 1920x1080 --script res://tools/forest_benchmark.gd
##
## Set RTS_FOREST_SCREENSHOTS to a folder to save one picture per view.

const TerrainVegetation := preload("res://terrain_vegetation.gd")
const DISTANCES := [60.0, 180.0, 600.0, 2500.0, 7000.0]
const SETTLE_FRAMES := 60
const SAMPLE_FRAMES := 240
# The camera circles the forest centre this far out, so it pans over trees.
const PAN_RADIUS := 220.0
const PAN_SECONDS_PER_TURN := 40.0
const STARTUP_LIMIT_SECONDS := 240.0

var _main: Node
var _center := Vector3.ZERO
var _angle := 0.0


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	Engine.max_fps = 0
	var started := Time.get_ticks_msec()
	_main = load("res://main.tscn").instantiate()
	root.add_child(_main)
	while not _main.match_started or _main.terrain_vegetation == null or _main.terrain_vegetation.building:
		await process_frame
		if float(Time.get_ticks_msec() - started) / 1000.0 > STARTUP_LIMIT_SECONDS:
			_finish({"error": "the match or vegetation did not finish loading"}, 1)
			return
	var vegetation: TerrainVegetation = _main.terrain_vegetation
	_center = _densest_forest(vegetation)
	var report := {
		"adapter": RenderingServer.get_video_adapter_name(),
		"method": RenderingServer.get_current_rendering_method(),
		"resolution": "%dx%d" % [DisplayServer.window_get_size().x, DisplayServer.window_get_size().y],
		"load_seconds": snappedf(float(Time.get_ticks_msec() - started) / 1000.0, 0.1),
		"vegetation_instances": vegetation.instance_count,
		"vegetation_batches": vegetation.batch_count,
		"tree_records": vegetation.tree_record_count,
		"forest_center": [snappedf(_center.x, 1.0), snappedf(_center.z, 1.0)],
		"views": [],
	}
	for with_models in [true, false]:
		_set_near_models(vegetation, with_models)
		for distance in DISTANCES:
			_main.target_camera_distance = distance
			_main.camera_distance = distance
			for _frame in range(SETTLE_FRAMES):
				await _pan_frame()
			var sample := await _sample()
			sample["distance"] = distance
			sample["near_models"] = with_models
			sample["near_model_count"] = vegetation.near_model_count if with_models else 0
			report.views.append(sample)
			_save_screenshot("%s_%dm" % ["models" if with_models else "impostors", int(distance)])
	_finish(report, 0)


# The 100 m grid cell with the most trees.
func _densest_forest(vegetation: TerrainVegetation) -> Vector3:
	var best := Vector2i.ZERO
	var most := 0
	for cell in vegetation._tree_cells:
		var count: int = vegetation._tree_cells[cell].size()
		if count > most:
			most = count
			best = cell
	var x := (float(best.x) + 0.5) * TerrainVegetation.TREE_CELL_SIZE
	var z := (float(best.y) + 0.5) * TerrainVegetation.TREE_CELL_SIZE
	return Vector3(x, _main._terrain_height_at(x, z), z)


# The A/B switch: without models every tree stays an impostor up close.
func _set_near_models(vegetation: TerrainVegetation, enabled: bool) -> void:
	for impostor in vegetation._impostors:
		if impostor.kind != "shrub":
			vegetation._set_near_fade(impostor.material, enabled)
	vegetation.set_process(enabled)
	var ring := vegetation.get_node_or_null("NearTreeModels")
	if ring != null:
		ring.visible = enabled


func _pan_frame() -> void:
	await process_frame
	_angle += TAU * root.get_process_delta_time() / PAN_SECONDS_PER_TURN
	var x := _center.x + cos(_angle) * PAN_RADIUS
	var z := _center.z + sin(_angle) * PAN_RADIUS
	_main.camera_target = Vector3(x, _main._terrain_height_at(x, z), z)


func _sample() -> Dictionary:
	var frame_ms := PackedFloat32Array()
	frame_ms.resize(SAMPLE_FRAMES)
	var last := Time.get_ticks_usec()
	for index in range(SAMPLE_FRAMES):
		await _pan_frame()
		var now := Time.get_ticks_usec()
		frame_ms[index] = float(now - last) / 1000.0
		last = now
	var sorted := frame_ms.duplicate()
	sorted.sort()
	var total := 0.0
	for value in frame_ms:
		total += value
	var viewport := root.get_viewport()
	return {
		"fps": snappedf(1000.0 * SAMPLE_FRAMES / total, 0.1),
		"p99_ms": snappedf(sorted[int(SAMPLE_FRAMES * 0.99)], 0.01),
		"primitives": viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_PRIMITIVES_IN_FRAME),
		"draw_calls": viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME),
	}


func _save_screenshot(view_name: String) -> void:
	var folder := OS.get_environment("RTS_FOREST_SCREENSHOTS")
	if not folder.is_empty():
		root.get_texture().get_image().save_png(folder.path_join("%s.png" % view_name))


func _finish(report: Dictionary, exit_code: int) -> void:
	print("FOREST_BENCHMARK ", JSON.stringify(report))
	quit(exit_code)
