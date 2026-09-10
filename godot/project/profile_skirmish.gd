extends SceneTree
func _initialize() -> void:
	_run.call_deferred()
func _run() -> void:
	var view: Node = load("res://main.tscn").instantiate()
	root.add_child(view)
	await process_frame
	view._on_start_skirmish_pressed()
	for i in range(60):
		await process_frame
	var started := Time.get_ticks_usec()
	var samples := PackedFloat64Array()
	var last := started
	for i in range(240):
		await process_frame
		var now := Time.get_ticks_usec()
		samples.append(float(now-last)/1000)
		last = now
	samples.sort()
	print("G08_GRAPHICS_PROFILE native_entities=%d visible=%d frames=240 fps=%.2f p95_frame_ms=%.3f max_frame_ms=%.3f" % [view.extension.call("get_entity_count"),view.entity_ids.size(),240000000.0/float(last-started),samples[228],samples[239]])
	view.queue_free()
	await process_frame
	quit()
