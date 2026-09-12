class_name HeightMap
extends RefCounted

const TERRAIN_WIDTH := 320
const TERRAIN_HEIGHT := 320
const BIOME_OCEAN := 0
const BIOME_COAST := 1
const BIOME_PLAINS := 2
const BIOME_HILLS := 3
const BIOME_MOUNTAINS := 4


static func load_from_file(path: String) -> PackedFloat32Array:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		print("ERROR: Could not open heightmap file: %s" % path)
		return PackedFloat32Array()
	
	var size := file.get_length()
	var expected_size := TERRAIN_WIDTH * TERRAIN_HEIGHT * 4
	if size != expected_size:
		print("ERROR: Heightmap file size mismatch. Expected %d bytes, got %d" % [expected_size, size])
		file.close()
		return PackedFloat32Array()
	
	var buffer := file.get_buffer(size)
	file.close()
	
	var heights := PackedFloat32Array()
	heights.resize(TERRAIN_WIDTH * TERRAIN_HEIGHT)
	for i in range(TERRAIN_WIDTH * TERRAIN_HEIGHT):
		var byte_offset := i * 4
		heights[i] = buffer.decode_float(byte_offset)
	
	return heights


static func get_biome(height: float) -> int:
	if height < 0.0:
		return BIOME_OCEAN
	elif height < 5.0:
		return BIOME_COAST
	elif height < 30.0:
		return BIOME_PLAINS
	elif height < 80.0:
		return BIOME_HILLS
	else:
		return BIOME_MOUNTAINS


static func presentation_height(height: float, world_x: float, world_width := 320.0) -> float:
	# The simulation heightfield stores broad land relief.  Broken Strait's map
	# metadata defines two landmasses but the original binary contains no ocean
	# samples, so derive the visual channel here without changing simulation data.
	var land_mask := presentation_land_mask(world_x, world_width)
	# Heightfield values are authored in meters. The old 5% presentation scale
	# flattened 1.9 km mountain belts into a nearly featureless 95 m surface.
	# Preserve mountain-scale relief so ridges, valleys, and ravines read across
	# the 40 km theater instead of collapsing into a textured plane.
	var land_height := 0.35 + maxf(0.0, height) * 0.80
	return lerpf(-0.70, land_height, land_mask)


static func presentation_land_mask(world_x: float, world_width := 320.0) -> float:
	return smoothstep(world_width * 0.14, world_width * 0.22, absf(world_x))


static func generate_terrain_mesh(heights: PackedFloat32Array, world_width := 320.0, world_height := 320.0) -> ArrayMesh:
	print("generate_terrain_mesh called with height data size: %d" % [heights.size()])
	var mesh := ArrayMesh.new()
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	var colors := PackedColorArray()
	var uvs := PackedVector2Array()
	
	var width := TERRAIN_WIDTH
	var height := TERRAIN_HEIGHT
	
	for y in range(height):
		for x in range(width):
			var idx := y * width + x
			var world_x := (float(x) / float(width - 1) - 0.5) * world_width
			var world_z := (float(y) / float(height - 1) - 0.5) * world_height
			var world_y := presentation_height(heights[idx], world_x, world_width)
			vertices.append(Vector3(world_x, world_y, world_z))
			uvs.append(Vector2(float(x) / float(width - 1), float(y) / float(height - 1)))
	
	for y in range(height - 1):
		for x in range(width - 1):
			var i0 := y * width + x
			var i1 := i0 + 1
			var i2 := (y + 1) * width + x
			var i3 := i2 + 1
			indices.append(i0)
			indices.append(i2)
			indices.append(i1)
			indices.append(i1)
			indices.append(i2)
			indices.append(i3)
	
	normals.resize(vertices.size())
	for i in range(indices.size() / 3):
		var i0 := indices[i * 3]
		var i1 := indices[i * 3 + 1]
		var i2 := indices[i * 3 + 2]
		var v0 := vertices[i0]
		var v1 := vertices[i1]
		var v2 := vertices[i2]
		var edge1 := v1 - v0
		var edge2 := v2 - v0
		var normal := edge1.cross(edge2).normalized()
		normals[i0] = normals[i0] + normal
		normals[i1] = normals[i1] + normal
		normals[i2] = normals[i2] + normal
	
	for i in range(normals.size()):
		normals[i] = normals[i].normalized()
	
	for i in range(vertices.size()):
		var x := int(i % width)
		var world_x := (float(x) / float(width - 1) - 0.5) * world_width
		colors.append(Color(presentation_land_mask(world_x, world_width), 0.0, 0.0, 1.0))
	
	var surface_arrays := []
	surface_arrays.resize(Mesh.ARRAY_MAX)
	surface_arrays[Mesh.ARRAY_VERTEX] = vertices
	surface_arrays[Mesh.ARRAY_NORMAL] = normals
	surface_arrays[Mesh.ARRAY_INDEX] = indices
	surface_arrays[Mesh.ARRAY_COLOR] = colors
	surface_arrays[Mesh.ARRAY_TEX_UV] = uvs
	
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface_arrays)
	
	print("Mesh surface count after generation: %d" % [mesh.get_surface_count()])
	return mesh


static func _biome_to_color(biome: int) -> Color:
	match biome:
		BIOME_OCEAN:
			return Color(0.0, 0.2, 0.6, 1.0)
		BIOME_COAST:
			return Color(0.8, 0.75, 0.6, 1.0)
		BIOME_PLAINS:
			return Color(0.2, 0.6, 0.2, 1.0)
		BIOME_HILLS:
			return Color(0.4, 0.5, 0.3, 1.0)
		BIOME_MOUNTAINS:
			return Color(0.5, 0.5, 0.5, 1.0)
		_:
			return Color(1.0, 0.0, 1.0, 1.0)
