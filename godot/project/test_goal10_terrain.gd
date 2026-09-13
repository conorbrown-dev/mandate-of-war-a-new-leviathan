extends SceneTree

func _initialize() -> void:
	var heights := HeightMap.load_from_file("res://scenarios/terrain.bin")
	assert(heights.size() == 320 * 320, "Goal 10 heightmap has the authored 320x320 grid")
	assert(HeightMap.get_biome(-1.0) == HeightMap.BIOME_OCEAN)
	assert(HeightMap.get_biome(2.0) == HeightMap.BIOME_COAST)
	assert(HeightMap.get_biome(10.0) == HeightMap.BIOME_PLAINS)
	assert(HeightMap.get_biome(40.0) == HeightMap.BIOME_HILLS)
	assert(HeightMap.get_biome(100.0) == HeightMap.BIOME_MOUNTAINS)
	var mesh := HeightMap.generate_terrain_mesh(heights)
	assert(mesh.get_surface_count() == 1, "Goal 10 terrain mesh has one generated surface")
	var arrays := mesh.surface_get_arrays(0)
	assert((arrays[Mesh.ARRAY_VERTEX] as PackedVector3Array).size() == 320 * 320, "Goal 10 mesh has one vertex per height sample")
	assert((arrays[Mesh.ARRAY_NORMAL] as PackedVector3Array).size() == 320 * 320, "Goal 10 mesh has generated normals")
	print("GODOT_GOAL10_TERRAIN checks=9")
	quit()
