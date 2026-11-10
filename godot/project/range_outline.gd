class_name RangeOutline
extends RefCounted
## Shared drawing of sight, radar and weapon range outlines: curved circles that
## follow the terrain, drawn as screen-space ribbons as thick as the orange fog
## of war edge (fog_of_war.gdshader edge_width_px). Presentation only.

const SHADER := preload("res://range_outline.gdshader")
const WIDTH_PX := 3.0
const LINE_ALPHA := 0.88
# Longest stretch of circle drawn as one straight piece, in metres.
const MAX_PIECE_METRES := 24.0

static var _materials := {}


static func material_for(color: Color) -> ShaderMaterial:
	if not _materials.has(color):
		var material := ShaderMaterial.new()
		material.shader = SHADER
		material.set_shader_parameter("line_color", Color(color.r, color.g, color.b, LINE_ALPHA))
		material.set_shader_parameter("width_px", WIDTH_PX)
		_materials[color] = material
	return _materials[color]


## Even number of equal pieces around a circle, so alternate pieces can be
## dashed to tell coincident sight and weapon ranges apart.
static func piece_count(radius: float, dashed := false) -> int:
	var count := clampi(ceili(TAU * radius / MAX_PIECE_METRES), 32, 192)
	if dashed:
		count = maxi(count, 48)
	return count + (count % 2)


## Circle outline as line-segment endpoint pairs. With a height callable the
## points are lifted onto the ground at (x, z); otherwise they sit at center.y.
static func circle_vertices(center: Vector3, radius: float, height_at := Callable(), lift := 0.12, dash_phase := -1) -> PackedVector3Array:
	var points := PackedVector3Array()
	var count := piece_count(radius, dash_phase >= 0)
	for piece in range(count):
		if dash_phase >= 0 and piece % 2 != dash_phase:
			continue
		for angle in [TAU * float(piece) / count, TAU * float(piece + 1) / count]:
			var x := center.x + cos(angle) * radius
			var z := center.z + sin(angle) * radius
			var y := center.y + lift
			if height_at.is_valid():
				y = float(height_at.call(x, z)) + lift
			points.append(Vector3(x, y, z))
	return points


## Ribbon mesh for line-segment endpoint pairs (see range_outline.gdshader).
static func ribbon_mesh(points: PackedVector3Array) -> ArrayMesh:
	var mesh := ArrayMesh.new()
	var pairs := points.size() / 2
	if pairs == 0:
		return mesh
	var vertices := PackedVector3Array()
	var far_ends := PackedFloat32Array()
	var sides := PackedVector2Array()
	var indices := PackedInt32Array()
	vertices.resize(pairs * 4)
	far_ends.resize(pairs * 12)
	sides.resize(pairs * 4)
	indices.resize(pairs * 6)
	for pair in range(pairs):
		var start := points[pair * 2]
		var finish := points[pair * 2 + 1]
		var base := pair * 4
		for corner in range(4):
			var near := start if corner < 2 else finish
			var far := finish if corner < 2 else start
			vertices[base + corner] = near
			far_ends[(base + corner) * 3] = far.x
			far_ends[(base + corner) * 3 + 1] = far.y
			far_ends[(base + corner) * 3 + 2] = far.z
			# A corner's side is relative to the direction it is drawn along,
			# so the far end's corners flip to keep the quad a rectangle.
			var side := -1.0 if corner % 2 == 0 else 1.0
			sides[base + corner] = Vector2(side if corner < 2 else -side, 0.0)
		var at := pair * 6
		indices[at] = base
		indices[at + 1] = base + 1
		indices[at + 2] = base + 2
		indices[at + 3] = base + 1
		indices[at + 4] = base + 3
		indices[at + 5] = base + 2
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_TEX_UV] = sides
	arrays[Mesh.ARRAY_CUSTOM0] = far_ends
	arrays[Mesh.ARRAY_INDEX] = indices
	var format := Mesh.ARRAY_CUSTOM_RGB_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, [], {}, format)
	# The shader moves corners in screen space, so the CPU bounds must not cull.
	mesh.custom_aabb = AABB(Vector3(-100000.0, -1000.0, -100000.0), Vector3(200000.0, 2000.0, 200000.0))
	return mesh
