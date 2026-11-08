extends Node3D
## Dense presentation-only ground cover placed from the baked terrain masks.
##
## The map is split into chunks so the renderer can frustum-cull each batch.
## Every batch has a visibility range: dense trees and shrubs exist only near
## the camera, a sparser layer carries the mid distance, and past that the
## terrain shader paints forest canopy. This layer never affects simulation.
##
## Trees are impostors of real models (assets/vegetation/impostors): one quad
## each, showing the tree as rendered from the nearest of 64 directions. If the
## baked atlases are missing, simple built-in shapes stand in.
##
## Close to the camera an impostor is not enough, so the trees within
## NEAR_MODEL_RANGE are also drawn as their real models (the meshes the
## impostors were baked from, assets/vegetation/near). Every tree is recorded
## in a grid of TREE_CELL_SIZE cells as it is placed; the ring of models
## around the camera is rebuilt from that grid as the camera moves. The model
## and impostor shaders share one dithered fade (vegetation/tree_fade.gdshaderinc),
## so a tree dissolves from one form into the other without a pop.

const CHUNKS_PER_AXIS := 14
const MASK_SIZE := 640
const NEAR_RANGE := 3600.0
const FAR_RANGE := 13000.0
const NEAR_TREES_PER_KM2 := 2400.0
const FAR_TREES_PER_KM2 := 300.0
const SHRUBS_PER_CHUNK := 4200
const ROCKS_PER_CHUNK := 520
const MINIMUM_WORLD_SIZE := 10000.0
const RANDOM_SEED := 640642
const MINIMUM_LAND_HEIGHT := 6.0
const INSTANCE_FLOATS := 16
const IMPOSTOR_CATALOG := "res://assets/vegetation/impostors/impostors.json"
const ImpostorShader := preload("res://vegetation/tree_impostor.gdshader")
const NearLeafShader := preload("res://vegetation/tree_near_leaf.gdshader")
const NearBarkShader := preload("res://vegetation/tree_near_bark.gdshader")
# Trees closer to the camera than this are real models; over the last
# NEAR_FADE_BAND metres they dissolve into their impostors.
const NEAR_MODEL_RANGE := 150.0
const NEAR_FADE_BAND := 30.0
# The ring is rebuilt once the camera has moved this far, and holds the trees
# within NEAR_MODEL_RANGE plus this, so it is never short of a tree in range.
const NEAR_RING_REFRESH := 20.0
const TREE_CELL_SIZE := 100.0
# Grid record: variant, x, ground height, z, scale, width, yaw, tint r, g, b.
const TREE_RECORD_FLOATS := 10
# Grown heights in metres; the far layer is drawn larger, see size_factor.
const CONIFER_HEIGHTS := Vector2(15.0, 28.0)
const BROADLEAF_HEIGHTS := Vector2(12.0, 22.0)
const SHRUB_HEIGHTS := Vector2(1.6, 4.2)
const SAPLING_HEIGHTS := Vector2(2.5, 6.0)
# The atlases hold unlit colour from sources shot under different light. Each
# kind is drawn at a target brightness, so a pale source and a dark one match.
const KIND_LUMINANCE := {"conifer": 0.2, "broadleaf": 0.3, "sapling": 0.22, "shrub": 0.27}
# Woods grow in stands: within each patch this wide, one species of each kind
# is favoured, with the others mixed in.
const STAND_SIZE := 350.0
const STAND_PREFERENCE := 0.65
# Share of trees at the thin forest edge that are young firs.
const EDGE_SAPLING_SHARE := 0.35

signal finished

var instance_count := 0
var batch_count := 0
var building := false

var _masks := PackedByteArray()
var _world_width := 40000.0
var _world_height := 40000.0
var _height_at := Callable()
var _material: StandardMaterial3D
var _build_run := 0
# Each: {kind, key, height, material}. Empty when the baked atlases are missing.
var _impostors: Array[Dictionary] = []
var _impostor_quad: QuadMesh
# Near models, by impostor variant index; null where a variant has none.
var _near_meshes: Array[Mesh] = []
# Vector2i cell -> PackedFloat32Array of tree records (see TREE_RECORD_FLOATS).
var _tree_cells := {}
var tree_record_count := 0
var near_model_count := 0
var _near_root: Node3D
var _near_batches: Array[MultiMeshInstance3D] = []
var _near_ring_center := Vector3.INF
var _near_ring_dirty := false


static func supports_world(world_width: float) -> bool:
	return world_width >= MINIMUM_WORLD_SIZE


# Builds one chunk per frame, nearest the focus point first, so the match stays
# responsive while the map fills in. A later rebuild or clear abandons this one.
func rebuild(masks: PackedByteArray, world_width: float, world_height: float, height_at: Callable, focus := Vector2.ZERO) -> void:
	clear()
	if masks.size() != MASK_SIZE * MASK_SIZE * 4 or not supports_world(world_width):
		return
	var run := _build_run
	building = true
	_masks = masks
	_world_width = world_width
	_world_height = world_height
	_height_at = height_at
	_material = StandardMaterial3D.new()
	_material.vertex_color_use_as_albedo = true
	_material.roughness = 1.0
	_material.metallic_specular = 0.0
	var conifer_near := _conifer_mesh(7, 3)
	var conifer_far := _conifer_mesh(5, 1)
	var broadleaf_near := _broadleaf_mesh(true)
	var broadleaf_far := _broadleaf_mesh(false)
	var shrub := _shrub_mesh()
	_load_impostors()
	var rock := _rock_mesh()
	var rng := RandomNumberGenerator.new()
	rng.seed = RANDOM_SEED
	var chunk_size := Vector2(world_width, world_height) / float(CHUNKS_PER_AXIS)
	var square_km := chunk_size.x * chunk_size.y / 1000000.0
	var areas: Array[Rect2] = []
	for chunk_z in range(CHUNKS_PER_AXIS):
		for chunk_x in range(CHUNKS_PER_AXIS):
			var origin := (Vector2(chunk_x, chunk_z) - Vector2.ONE * float(CHUNKS_PER_AXIS) * 0.5) * chunk_size
			areas.append(Rect2(origin, chunk_size))
	areas.sort_custom(func(first: Rect2, second: Rect2) -> bool:
		return first.get_center().distance_squared_to(focus) < second.get_center().distance_squared_to(focus))
	for area in areas:
		_place_trees(area, int(square_km * NEAR_TREES_PER_KM2), conifer_near, broadleaf_near, NEAR_RANGE, 1.0, rng)
		_place_trees(area, int(square_km * FAR_TREES_PER_KM2), conifer_far, broadleaf_far, FAR_RANGE, 1.5, rng)
		_place_shrubs(area, shrub, rng)
		_place_rocks(area, rock, rng)
		_near_ring_dirty = true
		if is_inside_tree():
			await get_tree().process_frame
		if run != _build_run:
			return
	building = false
	finished.emit()


func clear() -> void:
	_build_run += 1
	building = false
	for child in get_children():
		remove_child(child)
		child.queue_free()
	instance_count = 0
	batch_count = 0
	_tree_cells.clear()
	tree_record_count = 0
	near_model_count = 0
	_near_root = null
	_near_batches.clear()
	_near_ring_center = Vector3.INF
	_near_ring_dirty = false


# Hides everything growing inside a levelled site.
func clear_area(area: Rect2) -> void:
	for child in get_children():
		var batch := child as MultiMeshInstance3D
		if batch == null or batch.multimesh == null:
			continue
		var buffer := batch.multimesh.buffer
		var changed := false
		for offset in range(0, buffer.size(), INSTANCE_FLOATS):
			if not area.has_point(Vector2(buffer[offset + 3], buffer[offset + 11])):
				continue
			for scale_index in [0, 1, 2, 4, 5, 6, 8, 9, 10]:
				buffer[offset + scale_index] = 0.0
			changed = true
		if changed:
			batch.multimesh.buffer = buffer
	_clear_tree_records(area)


func _mask_value(world_x: float, world_z: float, channel: int) -> int:
	var x := clampi(int((world_x / _world_width + 0.5) * float(MASK_SIZE - 1)), 0, MASK_SIZE - 1)
	var z := clampi(int((world_z / _world_height + 0.5) * float(MASK_SIZE - 1)), 0, MASK_SIZE - 1)
	return _masks[(z * MASK_SIZE + x) * 4 + channel]


func _load_impostors() -> void:
	_impostors.clear()
	_near_meshes.clear()
	if not FileAccess.file_exists(IMPOSTOR_CATALOG):
		push_warning("Tree impostors missing; using simple tree shapes")
		return
	var catalog = JSON.parse_string(FileAccess.get_file_as_string(IMPOSTOR_CATALOG))
	if not catalog is Dictionary:
		push_warning("Tree impostor catalog is unreadable; using simple tree shapes")
		return
	for variant in catalog.get("variants", []):
		var albedo: Texture2D = load(String(variant.albedo))
		var normal: Texture2D = load(String(variant.normal))
		if albedo == null or normal == null:
			push_warning("Tree impostor %s failed to load" % variant.get("key", "?"))
			continue
		var material := ShaderMaterial.new()
		material.shader = ImpostorShader
		material.set_shader_parameter("albedo_atlas", albedo)
		material.set_shader_parameter("normal_atlas", normal)
		material.set_shader_parameter("frames", float(catalog.get("frames", 8)))
		material.set_shader_parameter("radius", float(variant.radius))
		material.set_shader_parameter("center_height", float(variant.center_height))
		var mean: Array = variant.get("mean_color", [0.25, 0.25, 0.2])
		var luminance := 0.2126 * float(mean[0]) + 0.7152 * float(mean[1]) + 0.0722 * float(mean[2])
		var brightness := clampf(float(KIND_LUMINANCE.get(String(variant.kind), 0.25)) / maxf(luminance, 0.02), 0.6, 3.5)
		material.set_shader_parameter("brightness", brightness)
		# Shrubs stay impostors; trees hand over to their models up close.
		var near_mesh: Mesh = null
		if String(variant.kind) != "shrub":
			near_mesh = _near_mesh(String(variant.get("source", "")), brightness)
		_set_near_fade(material, near_mesh != null)
		_near_meshes.append(near_mesh)
		_impostors.append({"kind": String(variant.kind), "key": String(variant.key), "height": float(variant.height), "material": material})
	if _impostor_quad == null:
		_impostor_quad = QuadMesh.new()


# A copy of the tree's near model with the near shaders, which match its
# impostor's shading and dissolve into it. Null if the model is missing.
func _near_mesh(source: String, brightness: float) -> Mesh:
	if source.is_empty() or not ResourceLoader.exists(source):
		return null
	var original := load(source) as ArrayMesh
	if original == null:
		return null
	var mesh := original.duplicate() as ArrayMesh
	var bounds := mesh.get_aabb()
	# Where the baker put the crown (tools/bake_tree_impostors.gd).
	var crown_center := Vector3(0.0, bounds.position.y + bounds.size.y * 0.62, 0.0)
	var crown_radius := maxf(bounds.size.x, bounds.size.z) * 0.5
	for surface in range(mesh.get_surface_count()):
		var source_material := mesh.surface_get_material(surface) as BaseMaterial3D
		if source_material == null:
			continue
		var material := ShaderMaterial.new()
		var leafy := source_material.transparency != BaseMaterial3D.TRANSPARENCY_DISABLED
		material.shader = NearLeafShader if leafy else NearBarkShader
		if source_material.albedo_texture != null:
			material.set_shader_parameter("albedo_texture", source_material.albedo_texture)
		material.set_shader_parameter("albedo_color", source_material.albedo_color)
		material.set_shader_parameter("brightness", brightness)
		material.set_shader_parameter("crown_center", crown_center)
		material.set_shader_parameter("crown_radius", crown_radius)
		if leafy:
			material.set_shader_parameter("alpha_cutoff", source_material.alpha_scissor_threshold)
		else:
			material.set_shader_parameter("uv_scale", Vector2(source_material.uv1_scale.x, source_material.uv1_scale.y))
			if source_material.normal_enabled and source_material.normal_texture != null:
				material.set_shader_parameter("normal_texture", source_material.normal_texture)
				material.set_shader_parameter("use_normal_texture", true)
		_set_near_fade(material, true)
		mesh.surface_set_material(surface, material)
	return mesh


# Without a near model the impostor never fades, so the tree is always drawn.
func _set_near_fade(material: ShaderMaterial, enabled: bool) -> void:
	material.set_shader_parameter("near_fade_start", NEAR_MODEL_RANGE - NEAR_FADE_BAND if enabled else 0.0)
	material.set_shader_parameter("near_fade_end", NEAR_MODEL_RANGE if enabled else 0.0)


func _tree_cell(x: float, z: float) -> Vector2i:
	return Vector2i(floori(x / TREE_CELL_SIZE), floori(z / TREE_CELL_SIZE))


func _record_tree(variant: int, position: Vector2, height: float, scale: float, width: float, yaw: float, tint: Color) -> void:
	var cell := _tree_cell(position.x, position.y)
	if not _tree_cells.has(cell):
		_tree_cells[cell] = PackedFloat32Array()
	_tree_cells[cell].append_array(PackedFloat32Array([variant, position.x, height, position.y, scale, width, yaw, tint.r, tint.g, tint.b]))
	tree_record_count += 1


# Forgets the trees inside a levelled site, so no model grows there either.
func _clear_tree_records(area: Rect2) -> void:
	var first := _tree_cell(area.position.x, area.position.y)
	var last := _tree_cell(area.end.x, area.end.y)
	for cell_z in range(first.y, last.y + 1):
		for cell_x in range(first.x, last.x + 1):
			var cell := Vector2i(cell_x, cell_z)
			if not _tree_cells.has(cell):
				continue
			var records: PackedFloat32Array = _tree_cells[cell]
			var kept := PackedFloat32Array()
			for offset in range(0, records.size(), TREE_RECORD_FLOATS):
				if area.has_point(Vector2(records[offset + 1], records[offset + 3])):
					tree_record_count -= 1
				else:
					kept.append_array(records.slice(offset, offset + TREE_RECORD_FLOATS))
			_tree_cells[cell] = kept
	_near_ring_dirty = true


func _process(_delta: float) -> void:
	if _tree_cells.is_empty():
		return
	var camera := get_viewport().get_camera_3d() if is_inside_tree() else null
	if camera != null:
		update_near_ring(camera.global_position)


# Rebuilds the ring of tree models around the camera when it has moved far
# enough, or the trees have changed. Public so checks can drive it directly.
func update_near_ring(camera_position: Vector3, force := false) -> void:
	if not force and not _near_ring_dirty and camera_position.distance_to(_near_ring_center) < NEAR_RING_REFRESH:
		return
	_near_ring_dirty = false
	_near_ring_center = camera_position
	var reach := NEAR_MODEL_RANGE + NEAR_RING_REFRESH
	var buffers: Array[PackedFloat32Array] = []
	for _variant in _near_meshes:
		buffers.append(PackedFloat32Array())
	var first := _tree_cell(camera_position.x - reach, camera_position.z - reach)
	var last := _tree_cell(camera_position.x + reach, camera_position.z + reach)
	var instance := PackedFloat32Array()
	instance.resize(INSTANCE_FLOATS)
	for cell_z in range(first.y, last.y + 1):
		for cell_x in range(first.x, last.x + 1):
			var records: PackedFloat32Array = _tree_cells.get(Vector2i(cell_x, cell_z), PackedFloat32Array())
			for offset in range(0, records.size(), TREE_RECORD_FLOATS):
				var origin := Vector3(records[offset + 1], records[offset + 2], records[offset + 3])
				if origin.distance_to(camera_position) > reach:
					continue
				var variant := int(records[offset])
				# The same transform and tint as the tree's impostor.
				_write_instance(instance, 0, Vector2(origin.x, origin.z), origin.y, records[offset + 4], records[offset + 6],
					Color(records[offset + 7], records[offset + 8], records[offset + 9]), records[offset + 5])
				buffers[variant].append_array(instance)
	if _near_root == null:
		_near_root = Node3D.new()
		_near_root.name = "NearTreeModels"
		add_child(_near_root)
		for variant in range(_near_meshes.size()):
			var view := MultiMeshInstance3D.new()
			view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			if _near_meshes[variant] != null:
				var multimesh := MultiMesh.new()
				multimesh.transform_format = MultiMesh.TRANSFORM_3D
				multimesh.use_colors = true
				multimesh.mesh = _near_meshes[variant]
				view.multimesh = multimesh
			_near_root.add_child(view)
			_near_batches.append(view)
	near_model_count = 0
	for variant in range(_near_batches.size()):
		var multimesh := _near_batches[variant].multimesh
		if multimesh == null:
			continue
		var count := buffers[variant].size() / INSTANCE_FLOATS
		multimesh.instance_count = count
		if count > 0:
			multimesh.buffer = buffers[variant]
		_near_batches[variant].visible = count > 0
		near_model_count += count


func _impostor_indices(kind: String) -> Array[int]:
	var found: Array[int] = []
	for index in range(_impostors.size()):
		if _impostors[index].kind == kind:
			found.append(index)
	return found


# One batch per impostor variant, since each has its own atlas.
func _place_impostor_trees(area: Rect2, attempts: int, view_range: float, size_factor: float, rng: RandomNumberGenerator) -> void:
	var pools := {}
	for kind in ["conifer", "broadleaf", "sapling"]:
		pools[kind] = _impostor_indices(kind)
	var buffers: Array[PackedFloat32Array] = []
	var counts: Array[int] = []
	for _variant in _impostors:
		var buffer := PackedFloat32Array()
		buffer.resize(attempts * INSTANCE_FLOATS)
		buffers.append(buffer)
		counts.append(0)
	for _attempt in range(attempts):
		var position := _random_point(area, rng)
		var forest := _mask_value(position.x, position.y, 1)
		if forest < 24 or rng.randi_range(0, 255) > forest:
			continue
		var height: float = _height_at.call(position.x, position.y)
		if height < MINIMUM_LAND_HEIGHT:
			continue
		var kind := "conifer" if rng.randf() < clampf((height - 260.0) / 700.0, 0.1, 0.94) else "broadleaf"
		# Thin cover at the forest edge is young growth; the far layer is too
		# coarse to show it.
		if size_factor <= 1.0 and forest < 110 and rng.randf() < EDGE_SAPLING_SHARE:
			kind = "sapling"
		var pool: Array[int] = pools[kind]
		if pool.is_empty():
			pool = pools["conifer"] if not pools["conifer"].is_empty() else pools["broadleaf"]
		if pool.is_empty():
			continue
		var variant := _stand_variant(pool, position, rng)
		var heights: Vector2 = {"conifer": CONIFER_HEIGHTS, "broadleaf": BROADLEAF_HEIGHTS, "sapling": SAPLING_HEIGHTS}.get(String(_impostors[variant].kind), BROADLEAF_HEIGHTS)
		var scale := rng.randf_range(heights.x, heights.y) * size_factor / float(_impostors[variant].height)
		# Crowns differ in build as well as size: some trees grow lean, some broad.
		var width := rng.randf_range(0.82, 1.22)
		var yaw := rng.randf_range(0.0, TAU)
		var tint := _foliage_tint(String(_impostors[variant].kind), rng)
		_write_instance(buffers[variant], counts[variant], position, height - 0.3, scale, yaw, tint, width)
		counts[variant] += 1
		if _near_meshes[variant] != null:
			_record_tree(variant, position, height - 0.3, scale, width, yaw, tint)
	for variant in range(_impostors.size()):
		_add_batch(_impostor_quad, buffers[variant], counts[variant], area, view_range, _impostors[variant].material)


# The species favoured in the stand around a point, or now and then another.
func _stand_variant(pool: Array[int], position: Vector2, rng: RandomNumberGenerator) -> int:
	if pool.size() > 1 and rng.randf() < STAND_PREFERENCE:
		var cell := Vector2i(floori(position.x / STAND_SIZE), floori(position.y / STAND_SIZE))
		var stand := absi((cell.x * 73856093) ^ (cell.y * 19349663)) % pool.size()
		return pool[stand]
	return pool[rng.randi_range(0, pool.size() - 1)]


# Instance colour multiplies the baked colour. Conifers range from blue-green
# to deep green; broadleaves from deep green to yellow-olive, with a few
# already turning.
func _foliage_tint(kind: String, rng: RandomNumberGenerator) -> Color:
	var shade := rng.randf_range(0.82, 1.16)
	match kind:
		"conifer", "sapling":
			var blue := rng.randf_range(-0.08, 0.1)
			return Color(shade * (0.96 - blue * 0.5), shade, shade * (0.94 + blue))
		"broadleaf", "shrub":
			if rng.randf() < 0.03:
				var turn := rng.randf_range(0.15, 0.45)
				return Color(shade * (1.0 + turn * 0.7), shade * (1.0 - turn * 0.05), shade * (1.0 - turn * 0.55))
			var yellow := rng.randf_range(-0.06, 0.16)
			return Color(shade * (1.0 + yellow), shade, shade * (0.92 - yellow * 0.6))
		_:
			return Color(shade, shade * 0.98, shade * 0.95)


func _place_trees(area: Rect2, attempts: int, conifer: Mesh, broadleaf: Mesh, view_range: float, size_factor: float, rng: RandomNumberGenerator) -> void:
	if not _impostors.is_empty():
		_place_impostor_trees(area, attempts, view_range, size_factor, rng)
		return
	var conifers := PackedFloat32Array()
	var broadleaves := PackedFloat32Array()
	conifers.resize(attempts * INSTANCE_FLOATS)
	broadleaves.resize(attempts * INSTANCE_FLOATS)
	var conifer_count := 0
	var broadleaf_count := 0
	for _attempt in range(attempts):
		var position := _random_point(area, rng)
		var forest := _mask_value(position.x, position.y, 1)
		if forest < 24 or rng.randi_range(0, 255) > forest:
			continue
		var height: float = _height_at.call(position.x, position.y)
		if height < MINIMUM_LAND_HEIGHT:
			continue
		var conifer_chance := clampf((height - 260.0) / 700.0, 0.1, 0.94)
		var tint := rng.randf_range(0.8, 1.2)
		var scale := rng.randf_range(1.05, 1.9) * size_factor
		var color := Color(tint * rng.randf_range(0.88, 1.22), tint, tint * rng.randf_range(0.82, 1.02))
		if rng.randf() < conifer_chance:
			_write_instance(conifers, conifer_count, position, height - 0.3, scale, rng.randf_range(0.0, TAU), color)
			conifer_count += 1
		else:
			_write_instance(broadleaves, broadleaf_count, position, height - 0.3, scale, rng.randf_range(0.0, TAU), color)
			broadleaf_count += 1
	_add_batch(conifer, conifers, conifer_count, area, view_range)
	_add_batch(broadleaf, broadleaves, broadleaf_count, area, view_range)


func _place_shrubs(area: Rect2, mesh: Mesh, rng: RandomNumberGenerator) -> void:
	# Real shrub species when the impostors are available, else a blob shape.
	var bushes := _impostor_indices("shrub")
	if bushes.is_empty():
		bushes = _impostor_indices("broadleaf").slice(0, 1)
	var buffers: Array[PackedFloat32Array] = []
	var counts: Array[int] = []
	for _bush in range(maxi(bushes.size(), 1)):
		var buffer := PackedFloat32Array()
		buffer.resize(SHRUBS_PER_CHUNK * INSTANCE_FLOATS)
		buffers.append(buffer)
		counts.append(0)
	for _attempt in range(SHRUBS_PER_CHUNK):
		var position := _random_point(area, rng)
		var river := _mask_value(position.x, position.y, 0)
		var variation := _mask_value(position.x, position.y, 3)
		if river > 60 or rng.randi_range(0, 255) < 150 - variation / 2:
			continue
		var height: float = _height_at.call(position.x, position.y)
		if height < MINIMUM_LAND_HEIGHT or height > 1500.0:
			continue
		var slot := 0
		var scale := rng.randf_range(0.7, 2.6)
		var tint := Color.WHITE
		if not bushes.is_empty():
			slot = bushes.find(_stand_variant(bushes, position, rng))
			scale = rng.randf_range(SHRUB_HEIGHTS.x, SHRUB_HEIGHTS.y) / float(_impostors[bushes[slot]].height)
			tint = _foliage_tint("shrub", rng)
		else:
			var shade := rng.randf_range(0.76, 1.16)
			tint = Color(shade * rng.randf_range(0.92, 1.18), shade, shade * 0.9)
		_write_instance(buffers[slot], counts[slot], position, height - 0.2, scale, rng.randf_range(0.0, TAU), tint,
			rng.randf_range(0.85, 1.3))
		counts[slot] += 1
	if bushes.is_empty():
		_add_batch(mesh, buffers[0], counts[0], area, NEAR_RANGE)
		return
	for slot in range(bushes.size()):
		_add_batch(_impostor_quad, buffers[slot], counts[slot], area, NEAR_RANGE, _impostors[bushes[slot]].material)


func _place_rocks(area: Rect2, mesh: Mesh, rng: RandomNumberGenerator) -> void:
	var rocks := PackedFloat32Array()
	rocks.resize(ROCKS_PER_CHUNK * INSTANCE_FLOATS)
	var count := 0
	for _attempt in range(ROCKS_PER_CHUNK):
		var position := _random_point(area, rng)
		var height: float = _height_at.call(position.x, position.y)
		var uphill: float = _height_at.call(position.x + 60.0, position.y + 60.0)
		var slope := absf(uphill - height) / 85.0
		if height < 30.0 or (slope < 0.2 and height < 1100.0):
			continue
		var tint := rng.randf_range(0.72, 1.12)
		_write_instance(rocks, count, position, height - 1.0, rng.randf_range(1.0, 6.5), rng.randf_range(0.0, TAU),
			Color(tint, tint * 0.97, tint * 0.92))
		count += 1
	_add_batch(mesh, rocks, count, area, FAR_RANGE)


func _random_point(area: Rect2, rng: RandomNumberGenerator) -> Vector2:
	return Vector2(rng.randf_range(area.position.x, area.end.x), rng.randf_range(area.position.y, area.end.y))


# MultiMesh buffer order: a 3x4 transform by rows, then the instance color.
# width stretches the crown across (x and z) relative to its height.
func _write_instance(buffer: PackedFloat32Array, index: int, position: Vector2, height: float, scale: float, yaw: float, tint: Color, width := 1.0) -> void:
	var cos_yaw := cos(yaw) * scale * width
	var sin_yaw := sin(yaw) * scale * width
	var offset := index * INSTANCE_FLOATS
	buffer[offset] = cos_yaw
	buffer[offset + 2] = sin_yaw
	buffer[offset + 3] = position.x
	buffer[offset + 5] = scale
	buffer[offset + 7] = height
	buffer[offset + 8] = -sin_yaw
	buffer[offset + 10] = cos_yaw
	buffer[offset + 11] = position.y
	buffer[offset + 12] = tint.r
	buffer[offset + 13] = tint.g
	buffer[offset + 14] = tint.b
	buffer[offset + 15] = 1.0


func _add_batch(mesh: Mesh, buffer: PackedFloat32Array, count: int, area: Rect2, view_range: float, material: Material = null) -> void:
	if count == 0:
		return
	buffer.resize(count * INSTANCE_FLOATS)
	# Bounds that hug where this batch stands, so culling and visibility range
	# work from the trees rather than from the full height of the map.
	var lowest := INF
	var highest := -INF
	var scales := Vector2(INF, -INF)
	for offset in range(0, buffer.size(), INSTANCE_FLOATS):
		lowest = minf(lowest, buffer[offset + 7])
		highest = maxf(highest, buffer[offset + 7])
		scales = Vector2(minf(scales.x, buffer[offset + 5]), maxf(scales.y, buffer[offset + 5]))
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = true
	multimesh.mesh = mesh
	multimesh.instance_count = count
	multimesh.buffer = buffer
	var view := MultiMeshInstance3D.new()
	view.multimesh = multimesh
	view.material_override = material if material != null else _material
	# No shadows: the sun's shadow map spans the whole 28 km theatre, so a
	# tree's shadow is a few texels; measured at 7-14% of frame rate for
	# almost nothing visible.
	view.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	view.visibility_range_end = view_range
	view.visibility_range_end_margin = view_range * 0.08
	view.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_DISABLED
	# Trees reach about 70 m above their footing at the far layer's scale.
	view.custom_aabb = AABB(Vector3(area.position.x - 40.0, lowest - 10.0, area.position.y - 40.0),
		Vector3(area.size.x + 80.0, highest - lowest + 90.0, area.size.y + 80.0))
	# Smallest and largest instance scale, kept for checks that cannot read the
	# GPU-side buffer (headless runs).
	view.set_meta("scale_range", scales)
	add_child(view)
	instance_count += count
	batch_count += 1


func _conifer_mesh(sides: int, tiers: int) -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	_append_cone(faces, colors, Vector3.ZERO, 0.8, 6.0, sides, Color(0.2, 0.15, 0.1), Color(0.22, 0.16, 0.1))
	var crown_height := 21.0 / float(tiers)
	for tier in range(tiers):
		var base := 4.0 + float(tier) * crown_height * 0.82
		var radius := lerpf(6.2, 3.0, float(tier) / maxf(1.0, float(tiers - 1))) if tiers > 1 else 5.6
		var height := crown_height * 1.3 if tiers > 1 else 24.0
		_append_cone(faces, colors, Vector3(0.0, base, 0.0), radius, height, sides,
			Color(0.045, 0.1, 0.07), Color(0.1, 0.18, 0.12))
	return _mesh_from_triangles(faces, colors)


func _broadleaf_mesh(detailed: bool) -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	_append_cone(faces, colors, Vector3.ZERO, 0.8, 8.0, 5, Color(0.2, 0.15, 0.1), Color(0.23, 0.17, 0.11))
	var low := Color(0.075, 0.135, 0.065)
	var high := Color(0.165, 0.235, 0.105)
	_append_blob(faces, colors, Vector3(0.0, 13.0, 0.0), Vector3(7.5, 7.0, 7.5), low, high)
	if detailed:
		_append_blob(faces, colors, Vector3(3.6, 11.0, 2.2), Vector3(4.6, 4.2, 4.6), low, high)
		_append_blob(faces, colors, Vector3(-3.4, 11.6, -2.6), Vector3(4.2, 4.4, 4.2), low, high)
		_append_blob(faces, colors, Vector3(-1.0, 17.5, 1.6), Vector3(4.2, 3.6, 4.2), low, high)
	return _mesh_from_triangles(faces, colors)


func _shrub_mesh() -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	var low := Color(0.11, 0.15, 0.07)
	var high := Color(0.23, 0.27, 0.13)
	_append_blob(faces, colors, Vector3(0.0, 1.2, 0.0), Vector3(2.4, 1.6, 2.4), low, high)
	_append_blob(faces, colors, Vector3(1.7, 0.9, 0.9), Vector3(1.5, 1.1, 1.5), low, high)
	return _mesh_from_triangles(faces, colors)


func _rock_mesh() -> ArrayMesh:
	var faces := PackedVector3Array()
	var colors := PackedColorArray()
	_append_blob(faces, colors, Vector3(0.0, 1.2, 0.0), Vector3(2.8, 2.0, 2.1), Color(0.17, 0.165, 0.155), Color(0.33, 0.31, 0.285))
	return _mesh_from_triangles(faces, colors)


func _append_cone(faces: PackedVector3Array, colors: PackedColorArray, base: Vector3, radius: float, height: float, sides: int, low: Color, high: Color) -> void:
	var apex := base + Vector3(0.0, height, 0.0)
	for side in range(sides):
		var a := TAU * float(side) / float(sides)
		var b := TAU * float(side + 1) / float(sides)
		var first := base + Vector3(cos(a), 0.0, sin(a)) * radius
		var second := base + Vector3(cos(b), 0.0, sin(b)) * radius
		_append_triangle(faces, colors, [first, apex, second], [low, high, low], base + Vector3(0.0, height * 0.3, 0.0))


func _append_blob(faces: PackedVector3Array, colors: PackedColorArray, center: Vector3, radii: Vector3, low: Color, high: Color) -> void:
	var points: Array[Vector3] = [
		Vector3(0.0, 1.0, 0.0), Vector3(0.95, 0.2, 0.3), Vector3(0.3, 0.1, 0.95), Vector3(-0.8, 0.25, 0.55),
		Vector3(-0.75, 0.15, -0.65), Vector3(0.35, 0.3, -0.92), Vector3(0.7, -0.6, 0.35), Vector3(-0.4, -0.65, 0.6),
		Vector3(-0.6, -0.55, -0.5), Vector3(0.45, -0.6, -0.6),
	]
	var triangles: Array[Vector3i] = [
		Vector3i(0, 2, 1), Vector3i(0, 3, 2), Vector3i(0, 4, 3), Vector3i(0, 5, 4), Vector3i(0, 1, 5),
		Vector3i(1, 2, 6), Vector3i(2, 7, 6), Vector3i(2, 3, 7), Vector3i(3, 8, 7), Vector3i(3, 4, 8),
		Vector3i(4, 9, 8), Vector3i(4, 5, 9), Vector3i(5, 6, 9), Vector3i(5, 1, 6),
	]
	for triangle in triangles:
		var corners: Array[Vector3] = []
		var shades: Array[Color] = []
		for corner in [triangle.x, triangle.y, triangle.z]:
			var point: Vector3 = points[corner]
			corners.append(center + point * radii)
			shades.append(low.lerp(high, clampf(point.y * 0.5 + 0.5, 0.0, 1.0)))
		_append_triangle(faces, colors, corners, shades, center)


# Godot treats clockwise triangles as front-facing, so wind each one away from
# the center of its shape.
func _append_triangle(faces: PackedVector3Array, colors: PackedColorArray, corners: Array, shades: Array, center: Vector3) -> void:
	var a: Vector3 = corners[0]
	var b: Vector3 = corners[1]
	var c: Vector3 = corners[2]
	var flip := (c - a).cross(b - a).dot((a + b + c) / 3.0 - center) < 0.0
	faces.append_array(PackedVector3Array([a, c if flip else b, b if flip else c]))
	colors.append_array(PackedColorArray([shades[0], shades[2] if flip else shades[1], shades[1] if flip else shades[2]]))


func _mesh_from_triangles(faces: PackedVector3Array, colors: PackedColorArray) -> ArrayMesh:
	var normals := PackedVector3Array()
	normals.resize(faces.size())
	for index in range(0, faces.size(), 3):
		var normal := (faces[index + 2] - faces[index]).cross(faces[index + 1] - faces[index]).normalized()
		normals[index] = normal
		normals[index + 1] = normal
		normals[index + 2] = normal
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = faces
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_COLOR] = colors
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh
