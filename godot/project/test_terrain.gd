extends Node3D

func _ready():
	push_warning("Starting test...")
	var file := FileAccess.open("res://scenarios/terrain.bin", FileAccess.READ)
	if file == null:
		push_warning("Failed to open file")
		return
	else:
		push_warning("File opened successfully")
	
	file.seek_end(0)
	var size := file.get_position()
	push_warning("File size: %d" % [size])
	
	file.seek(0)
	
	var float_data := PackedFloat32Array()
	float_data.resize(320 * 320)
	for i in range(320 * 320):
		float_data[i] = file.get_float()
	
	push_warning("Float data loaded: %d elements" % [float_data.size()])
	
	var width := 320
	var height := 320
	
	var vertices := PackedVector3Array()
	for y in range(height):
		for x in range(width):
			var idx := y * width + x
			var world_x := float(x) - float(width) * 0.5
			var world_z := float(y) - float(height) * 0.5
			var world_y := float_data[idx]
			vertices.append(Vector3(world_x, world_y, world_z))
	
	push_warning("Vertices loaded: %d" % [vertices.size()])
	
	var indices := PackedInt32Array()
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
	
	push_warning("Indices loaded: %d" % [indices.size()])
	
	var normals := PackedVector3Array()
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
		normals.append(normal)
		normals.append(normal)
		normals.append(normal)
	
	push_warning("Normals loaded: %d" % [normals.size()])
	
	var colors := PackedColorArray()
	for i in range(vertices.size()):
		var height_val := float_data[i]
		var biome := 0
		if height_val < 0.0:
			biome = 0
		elif height_val < 5.0:
			biome = 1
		elif height_val < 30.0:
			biome = 2
		elif height_val < 80.0:
			biome = 3
		else:
			biome = 4
		match biome:
			0: colors.append(Color(0.0, 0.2, 0.6, 1.0))
			1: colors.append(Color(0.8, 0.75, 0.6, 1.0))
			2: colors.append(Color(0.2, 0.6, 0.2, 1.0))
			3: colors.append(Color(0.4, 0.5, 0.3, 1.0))
			4: colors.append(Color(0.5, 0.5, 0.5, 1.0))
	
	push_warning("Colors loaded: %d" % [colors.size()])
	
	var mesh := ArrayMesh.new()
	var surface_arrays := []
	surface_arrays.resize(Mesh.ARRAY_MAX)
	surface_arrays[Mesh.ARRAY_VERTEX] = vertices
	surface_arrays[Mesh.ARRAY_NORMAL] = normals
	surface_arrays[Mesh.ARRAY_INDEX] = indices
	surface_arrays[Mesh.ARRAY_COLOR] = colors
	
	push_warning("Array sizes: %d, ARRAY_MAX: %d" % [surface_arrays.size(), Mesh.ARRAY_MAX])
	
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surface_arrays)
