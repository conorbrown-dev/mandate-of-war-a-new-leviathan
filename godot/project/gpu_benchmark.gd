extends SceneTree
## Measures real rendering on the active graphics adapter. Run it through
## tools/gpu_benchmark.ps1. It says nothing about simulation scale.

const VIEWS := [
	{"name": "close", "distance": 120.0},
	{"name": "default", "distance": 420.0},
	{"name": "wide", "distance": 2200.0},
	{"name": "strategic", "distance": 9000.0},
]
const SETTLE_FRAMES := 90
const SAMPLE_FRAMES := 300
const STARTUP_LIMIT_SECONDS := 240.0


func _init() -> void:
	call_deferred("_run")


func _run() -> void:
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	Engine.max_fps = 0
	var started := Time.get_ticks_msec()
	var main: Node = load("res://main.tscn").instantiate()
	root.add_child(main)
	var viewport_rid := root.get_viewport_rid()
	RenderingServer.viewport_set_measure_render_time(viewport_rid, true)

	while not main.match_started or main.terrain_vegetation == null or main.terrain_vegetation.building:
		await process_frame
		if float(Time.get_ticks_msec() - started) / 1000.0 > STARTUP_LIMIT_SECONDS:
			_finish({"error": "the match or vegetation did not finish loading"}, 1)
			return
	var load_seconds := float(Time.get_ticks_msec() - started) / 1000.0

	var report := {
		"adapter": RenderingServer.get_video_adapter_name(),
		"vendor": RenderingServer.get_video_adapter_vendor(),
		"api": RenderingServer.get_video_adapter_api_version(),
		"method": RenderingServer.get_current_rendering_method(),
		"resolution": "%dx%d" % [DisplayServer.window_get_size().x, DisplayServer.window_get_size().y],
		"load_seconds": snappedf(load_seconds, 0.1),
		"vegetation_instances": main.terrain_vegetation.instance_count,
		"views": [],
	}
	for view in VIEWS:
		main.target_camera_distance = view.distance
		main.camera_distance = view.distance
		for _frame in range(SETTLE_FRAMES):
			await process_frame
		report.views.append(await _sample(view.name, viewport_rid))
		_save_screenshot(view.name)
	_finish(report, 0)


func _sample(view_name: String, viewport_rid: RID) -> Dictionary:
	var frame_ms := PackedFloat32Array()
	frame_ms.resize(SAMPLE_FRAMES)
	var cpu_ms := 0.0
	var gpu_ms := 0.0
	var last := Time.get_ticks_usec()
	for index in range(SAMPLE_FRAMES):
		await process_frame
		var now := Time.get_ticks_usec()
		frame_ms[index] = float(now - last) / 1000.0
		last = now
		cpu_ms += RenderingServer.viewport_get_measured_render_time_cpu(viewport_rid)
		gpu_ms += RenderingServer.viewport_get_measured_render_time_gpu(viewport_rid)
	var sorted := frame_ms.duplicate()
	sorted.sort()
	var total := 0.0
	for value in frame_ms:
		total += value
	var viewport := root.get_viewport()
	return {
		"view": view_name,
		"fps": snappedf(1000.0 * SAMPLE_FRAMES / total, 0.1),
		"avg_ms": snappedf(total / SAMPLE_FRAMES, 0.01),
		"p99_ms": snappedf(sorted[int(SAMPLE_FRAMES * 0.99)], 0.01),
		"worst_ms": snappedf(sorted[SAMPLE_FRAMES - 1], 0.01),
		"render_cpu_ms": snappedf(cpu_ms / SAMPLE_FRAMES, 0.01),
		"render_gpu_ms": snappedf(gpu_ms / SAMPLE_FRAMES, 0.01),
		"draw_calls": viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_DRAW_CALLS_IN_FRAME),
		"primitives": viewport.get_render_info(Viewport.RENDER_INFO_TYPE_VISIBLE, Viewport.RENDER_INFO_PRIMITIVES_IN_FRAME),
	}


func _save_screenshot(view_name: String) -> void:
	var folder := OS.get_environment("RTS_GPU_BENCHMARK_SCREENSHOTS")
	if not folder.is_empty():
		root.get_texture().get_image().save_png(folder.path_join("%s.png" % view_name))


func _finish(report: Dictionary, exit_code: int) -> void:
	print("GPU_BENCHMARK ", JSON.stringify(report))
	quit(exit_code)
