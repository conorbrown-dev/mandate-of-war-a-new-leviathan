extends SceneTree

const RegistryScript = preload("res://visual_definition_registry.gd")
const VisualRootScript = preload("res://unit_visual_root.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	var count := int(OS.get_environment("VISUAL_BENCHMARK_COUNT"))
	if count <= 0:
		count = 100
	var registry = RegistryScript.new()
	assert(registry.load_definitions())
	var holder := Node3D.new()
	root.add_child(holder)
	var start := Time.get_ticks_usec()
	for index in range(count):
		var visual = VisualRootScript.new()
		holder.add_child(visual)
		visual.configure(registry, "visual.elite.mbt.prototype", Color("#5c9ccc"))
		visual.apply_simulation_transform(Transform3D(Basis.IDENTITY, Vector3(index % 100, 0, index / 100)))
	var create_ms := float(Time.get_ticks_usec() - start) / 1000.0
	start = Time.get_ticks_usec()
	for visual in holder.get_children():
		visual.update_lod(250.0)
	var lod_ms := float(Time.get_ticks_usec() - start) / 1000.0
	print("VISUAL_WRAPPER_BENCHMARK count=%d create_ms=%.3f strategic_lod_ms=%.3f visible_objects=%d fallback_models=%d" % [count, create_ms, lod_ms, holder.get_child_count(), holder.get_child_count()])
	quit()
