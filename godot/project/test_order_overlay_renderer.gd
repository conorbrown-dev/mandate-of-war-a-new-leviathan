extends SceneTree

const Renderer := preload("res://order_overlay_renderer.gd")
const UiTokens := preload("res://ui/theme/ui_tokens.gd")

func _init() -> void:
	var renderer := Renderer.new()
	renderer.set_terrain_height_sampler(func(_x: float, _z: float): return 3.5)
	root.add_child(renderer)
	await process_frame
	renderer.show_move_order(Vector2(0.0, 0.0), Vector2(120.0, 40.0))
	assert(renderer.visible)
	assert(renderer.destination_marker.visible)
	assert(renderer.arrow_mesh_instance.mesh != null)
	assert(renderer._arrow_material.albedo_color.is_equal_approx(Color(UiTokens.ACCENT, 0.72)))
	var arrays := renderer.arrow_mesh_instance.mesh.surface_get_arrays(0)
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	assert(vertices.size() >= 7)
	for vertex in vertices:
		assert(is_finite(vertex.x) and is_finite(vertex.y) and is_finite(vertex.z))
	assert(renderer._width > 0.0)
	renderer.set_camera_distance(26000.0)
	assert(renderer._width <= renderer.MAX_WIDTH)
	renderer.clear_order()
	assert(not renderer.visible and not renderer.destination_marker.visible)
	print("order_overlay_renderer: PASS")
	quit()
