class_name HeightMap
extends RefCounted

const MASK_SIZE := 640
const TERRAIN_WIDTH := 320
const TERRAIN_HEIGHT := 320
const BIOME_OCEAN := 0
const BIOME_COAST := 1
const BIOME_PLAINS := 2
const BIOME_HILLS := 3
const BIOME_MOUNTAINS := 4


# Reads the JSON heightfield format described in src/simulation/terrain.hpp.
static func load_from_file(path: String) -> PackedFloat32Array:
	var text := FileAccess.get_file_as_string(path)
	var parsed: Variant = JSON.parse_string(text) if not text.is_empty() else null
	if typeof(parsed) != TYPE_DICTIONARY or parsed.get("format", "") != "heightfield":
		print("ERROR: Could not read heightfield: %s" % path)
		return PackedFloat32Array()
	var samples: Array = parsed.get("heights", [])
	if int(parsed.get("width", 0)) != TERRAIN_WIDTH or int(parsed.get("height", 0)) != TERRAIN_HEIGHT or samples.size() != TERRAIN_WIDTH * TERRAIN_HEIGHT:
		print("ERROR: Heightfield size mismatch: %s" % path)
		return PackedFloat32Array()
	return PackedFloat32Array(samples)


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


static func presentation_height(height: float, world_x: float, world_width := 320.0, world_z := 0.0) -> float:
	# The simulation heightfield stores broad land relief.  Broken Strait's map
	# metadata defines two landmasses but the original binary contains no ocean
	# samples, so derive the visual channel here without changing simulation data.
	var land_mask := presentation_land_mask(world_x, world_width, world_z)
	# Heightfield values are authored in meters. The old 5% presentation scale
	# flattened 1.9 km mountain belts into a nearly featureless 95 m surface.
	# Preserve mountain-scale relief so ridges, valleys, and ravines read across
	# the 40 km theater instead of collapsing into a textured plane.
	var land_height := 0.35 + maxf(0.0, height) * 0.80
	return lerpf(-0.70, land_height, land_mask)


static func presentation_land_mask(world_x: float, world_width := 320.0, world_z := 0.0) -> float:
	var shore := absf(world_x) + coast_wobble(world_z, world_width)
	var outer := maxf(
		absf(world_x) + coast_wobble(world_z + world_width * 0.0225, world_width) * 0.5,
		absf(world_z) + coast_wobble(world_x + world_width * 0.0525, world_width) * 0.5)
	return smoothstep(world_width * 0.14, world_width * 0.22, shore) * (1.0 - smoothstep(world_width * 0.452, world_width * 0.482, outer))


# Keep in step with coast_wobble() in tools/generate_terrain.py. The offset stays
# inside the existing shoreline band, so visible land never enters simulated water.
static func coast_wobble(world_z: float, world_width := 320.0) -> float:
	var scale := world_width / 40000.0
	var z := world_z / scale
	return scale * (850.0 * sin(z / 2600.0 + 1.1) + 430.0 * sin(z / 1130.0 + 0.4) + 220.0 * sin(z / 510.0 + 2.3))


# Baked RGBA masks from tools/generate_terrain.py: river, forest, light, variation.
static func load_masks(path: String) -> PackedByteArray:
	var image := Image.new()
	if image.load_png_from_buffer(FileAccess.get_file_as_bytes(path)) != OK or image.get_width() != MASK_SIZE or image.get_height() != MASK_SIZE:
		push_warning("Terrain masks are missing or have the wrong size: %s" % path)
		return PackedByteArray()
	image.convert(Image.FORMAT_RGBA8)
	return image.get_data()


static func masks_texture(masks: PackedByteArray) -> ImageTexture:
	var image := Image.create_from_data(MASK_SIZE, MASK_SIZE, false, Image.FORMAT_RGBA8, masks)
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)


static func generate_terrain_mesh(heights: PackedFloat32Array, world_width := 320.0, world_height := 320.0) -> ArrayMesh:
	print("generate_terrain_mesh called with height data size: %d" % [heights.size()])
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, build_terrain_arrays(heights, world_width, world_height))
	print("Mesh surface count after generation: %d" % [mesh.get_surface_count()])
	return mesh


# Rewrites only the vertices in region, so levelling a site never rebuilds the map.
static func update_terrain_arrays(arrays: Array, heights: PackedFloat32Array, region: Rect2i, world_width: float, world_height: float) -> void:
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var width := TERRAIN_WIDTH
	var height := TERRAIN_HEIGHT
	var first_x := maxi(region.position.x, 0)
	var first_z := maxi(region.position.y, 0)
	var last_x := mini(region.end.x, width - 1)
	var last_z := mini(region.end.y, height - 1)
	for z in range(first_z, last_z + 1):
		for x in range(first_x, last_x + 1):
			var index := z * width + x
			var vertex := vertices[index]
			vertex.y = presentation_height(heights[index], vertex.x, world_width, vertex.z)
			vertices[index] = vertex
	for z in range(maxi(first_z - 1, 0), mini(last_z + 2, height)):
		for x in range(maxi(first_x - 1, 0), mini(last_x + 2, width)):
			var left := vertices[z * width + maxi(x - 1, 0)]
			var right := vertices[z * width + mini(x + 1, width - 1)]
			var up := vertices[maxi(z - 1, 0) * width + x]
			var down := vertices[mini(z + 1, height - 1) * width + x]
			normals[z * width + x] = (down - up).cross(right - left).normalized()
	var colors: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
	for z in range(first_z, last_z + 1):
		for x in range(first_x, last_x + 1):
			colors[z * width + x].g = 1.0
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors


static func build_terrain_arrays(heights: PackedFloat32Array, world_width: float, world_height: float) -> Array:
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
			var world_y := presentation_height(heights[idx], world_x, world_width, world_z)
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
		colors.append(Color(presentation_land_mask(vertices[i].x, world_width, vertices[i].z), 0.0, 0.0, 1.0))
	
	var surface_arrays := []
	surface_arrays.resize(Mesh.ARRAY_MAX)
	surface_arrays[Mesh.ARRAY_VERTEX] = vertices
	surface_arrays[Mesh.ARRAY_NORMAL] = normals
	surface_arrays[Mesh.ARRAY_INDEX] = indices
	surface_arrays[Mesh.ARRAY_COLOR] = colors
	surface_arrays[Mesh.ARRAY_TEX_UV] = uvs
	return surface_arrays


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
