extends SceneTree
## The forest is drawn from baked impostors of real tree models
## (tools/bake_tree_impostors.gd), and close to the camera from the models
## themselves (EZ-Tree meshes, tools/build_near_trees.gd). Checks the baked
## catalog, that the vegetation layer places those trees at believable sizes,
## and that the ring of near models holds exactly the trees around the camera.

const TerrainVegetation := preload("res://terrain_vegetation.gd")
const ImpostorShader := preload("res://vegetation/tree_impostor.gdshader")
const NearLeafShader := preload("res://vegetation/tree_near_leaf.gdshader")
const NearBarkShader := preload("res://vegetation/tree_near_bark.gdshader")
const GROUND := 600.0

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	_run.call_deferred()


func _test_catalog() -> Dictionary:
	var catalog = JSON.parse_string(FileAccess.get_file_as_string(TerrainVegetation.IMPOSTOR_CATALOG))
	check(catalog is Dictionary, "the impostor catalog is valid JSON")
	if not catalog is Dictionary:
		return {}
	var frames := int(catalog.get("frames", 0))
	check(frames >= 4, "each atlas holds a useful number of views")
	var kinds := {}
	for variant in catalog.get("variants", []):
		var atlas_size := frames * int(variant.get("frame_pixels", catalog.get("frame_pixels", 0)))
		check(atlas_size >= 2048, "%s has sharp frames (%d px atlas)" % [variant.key, atlas_size])
		kinds[String(variant.kind)] = true
		var albedo: Texture2D = load(String(variant.albedo))
		var normal: Texture2D = load(String(variant.normal))
		check(albedo != null and normal != null, "%s atlases load" % variant.key)
		if albedo != null and normal != null:
			# Normals are stored at half resolution; the frame layout is the same.
			check(albedo.get_width() == atlas_size and albedo.get_height() == atlas_size and normal.get_size() * 2 == albedo.get_size(),
				"%s atlases are %d px square, normals half that" % [variant.key, atlas_size])
			check(variant.has("mean_color"), "%s records its mean colour for exposure matching" % variant.key)
		var height := float(variant.height)
		var radius := float(variant.radius)
		var center := float(variant.center_height)
		# The quad is 2 * radius across around center_height: the whole tree must fit.
		var minimum_height := 0.5 if String(variant.kind) == "shrub" else 2.0
		check(height > minimum_height and center > 0.0 and center < height, "%s stands on its origin" % variant.key)
		check(radius >= maxf(center, height - center) and radius >= float(variant.crown_width) * 0.5,
			"%s's bounding sphere holds the whole tree" % variant.key)
		# Every tree and bush is baked from the model drawn up close.
		var source := String(variant.get("source", ""))
		var model: Mesh = load(source) if ResourceLoader.exists(source) else null
		check(model != null, "%s has its near model" % variant.key)
		if model != null:
			check(model.get_surface_count() == 2 and absf(model.get_aabb().end.y - height) < height * 0.02,
				"%s's model has bark and leaves and is the height of its impostor" % variant.key)
	check(kinds.has("conifer") and kinds.has("broadleaf"), "the forest has both conifers and broadleaves")
	for kind in ["conifer", "broadleaf", "sapling", "shrub"]:
		var species := 0
		for variant in catalog.get("variants", []):
			species += 1 if String(variant.kind) == kind else 0
		check(species >= 2, "the forest has several %s variants (%d)" % [kind, species])
	# The EZ-Tree species: pines, oaks, ashes, aspens, young pines and bushes.
	var keys := {}
	for variant in catalog.get("variants", []):
		var key := String(variant.key)
		keys[key.substr(0, key.rfind("_"))] = true
	for species in ["pine", "oak", "ash", "aspen", "young_pine", "bush"]:
		check(keys.has(species), "the forest has %s" % species)
	return catalog


func _test_placement(catalog: Dictionary) -> void:
	var masks := PackedByteArray()
	masks.resize(TerrainVegetation.MASK_SIZE * TerrainVegetation.MASK_SIZE * 4)
	for index in range(0, masks.size(), 4):
		# Dense forest, with a thin edge along the west where young pines grow.
		var column := (index / 4) % TerrainVegetation.MASK_SIZE
		masks[index + 1] = 80 if column < TerrainVegetation.MASK_SIZE / 8 else 255
		masks[index + 3] = 128
	# High enough ground for a mix of firs and broadleaves.
	var height_at := func(_x: float, _z: float) -> float: return GROUND
	var vegetation := TerrainVegetation.new()
	root.add_child(vegetation)
	vegetation.rebuild(masks, 10000.0, 10000.0, height_at)
	await vegetation.finished

	var heights_by_radius := {}
	for variant in catalog.variants:
		heights_by_radius[snappedf(float(variant.radius), 0.001)] = [String(variant.kind), float(variant.height)]
	var tree_kinds := {}
	var impostor_trees := 0
	var badly_sized := 0
	var loose_bounds := 0
	var fading_trees := 0
	for child in vegetation.get_children():
		var batch := child as MultiMeshInstance3D
		if batch == null:
			continue
		var material := batch.material_override as ShaderMaterial
		if material == null:
			continue
		check(material.shader == ImpostorShader, "impostor batches use the impostor shader")
		var key := snappedf(float(material.get_shader_parameter("radius")), 0.001)
		check(heights_by_radius.has(key), "every impostor batch matches a catalog tree")
		if not heights_by_radius.has(key):
			continue
		var kind: String = heights_by_radius[key][0]
		var model_height: float = heights_by_radius[key][1]
		var multimesh := batch.multimesh
		var is_far := batch.visibility_range_end > TerrainVegetation.NEAR_RANGE
		var grown: Vector2 = batch.get_meta("scale_range", Vector2.ZERO) * model_height
		if grown.y <= TerrainVegetation.SHRUB_HEIGHTS.y + 0.01:
			if grown.x < TerrainVegetation.SHRUB_HEIGHTS.x - 0.01:
				badly_sized += 1
		else:
			tree_kinds[kind] = true
			impostor_trees += multimesh.instance_count
			if float(material.get_shader_parameter("near_fade_end")) == TerrainVegetation.NEAR_MODEL_RANGE:
				fading_trees += multimesh.instance_count
			var heights: Vector2 = {"conifer": TerrainVegetation.CONIFER_HEIGHTS, "sapling": TerrainVegetation.SAPLING_HEIGHTS}.get(kind, TerrainVegetation.BROADLEAF_HEIGHTS)
			var factor := 1.5 if is_far else 1.0
			if grown.x < heights.x * factor - 0.01 or grown.y > heights.y * factor + 0.01:
				badly_sized += 1
		if batch.custom_aabb.size.y > 200.0 or batch.custom_aabb.end.y < GROUND + grown.y:
			loose_bounds += 1
		if kind == "shrub":
			check(float(material.get_shader_parameter("near_fade_end")) <= 0.0, "bushes stay impostors up close")
	check(impostor_trees > 1000, "a full forest grows from the impostors (%d trees)" % impostor_trees)
	check(tree_kinds.has("conifer") and tree_kinds.has("broadleaf") and tree_kinds.has("sapling"),
		"conifers, broadleaves and young pines are planted")
	check(badly_sized == 0, "trees grow to their kind's height range (%d batches outside)" % badly_sized)
	check(loose_bounds == 0, "batch bounds hug the trees so culling works (%d loose)" % loose_bounds)
	check(fading_trees == impostor_trees, "every tree impostor hands over to a model (%d of %d)" % [fading_trees, impostor_trees])
	check(vegetation.tree_record_count == impostor_trees,
		"every tree is in the near-model grid (%d of %d)" % [vegetation.tree_record_count, impostor_trees])
	_test_near_ring(vegetation)

	# Levelling a site still clears the trees standing on it. The dummy
	# renderer used by headless runs keeps no instance data to read back.
	if DisplayServer.get_name() == "headless":
		print("TREE_IMPOSTORS clear_area check needs a GPU; skipped headless")
		vegetation.queue_free()
		return
	var area := Rect2(-500.0, -500.0, 1000.0, 1000.0)
	vegetation.clear_area(area)
	var standing := 0
	for child in vegetation.get_children():
		if not child is MultiMeshInstance3D:
			continue
		var multimesh := (child as MultiMeshInstance3D).multimesh
		for index in range(multimesh.instance_count):
			var transform := multimesh.get_instance_transform(index)
			if area.has_point(Vector2(transform.origin.x, transform.origin.z)) and transform.basis.get_scale().y > 0.0001:
				standing += 1
	check(standing == 0, "a levelled site has no trees left on it (%d standing)" % standing)
	vegetation.queue_free()


# Grid trees within reach of a camera (x), and inside an area (y).
func _records_near(vegetation: Node, camera: Vector3, reach: float, area := Rect2()) -> Vector2i:
	var counts := Vector2i.ZERO
	for records in vegetation._tree_cells.values():
		for offset in range(0, records.size(), TerrainVegetation.TREE_RECORD_FLOATS):
			var origin := Vector3(records[offset + 1], records[offset + 2], records[offset + 3])
			counts.x += 1 if origin.distance_to(camera) <= reach else 0
			counts.y += 1 if area.has_point(Vector2(origin.x, origin.z)) else 0
	return counts


func _test_near_ring(vegetation: Node) -> void:
	var reach := TerrainVegetation.NEAR_MODEL_RANGE + TerrainVegetation.NEAR_RING_REFRESH
	# A camera standing in the forest gets the trees around it as models.
	var camera := Vector3(120.0, GROUND + 30.0, -80.0)
	vegetation.update_near_ring(camera, true)
	var expected := _records_near(vegetation, camera, reach).x
	check(expected > 20, "a forest camera has trees within reach (%d)" % expected)
	check(vegetation.near_model_count == expected,
		"the near ring holds every tree within reach and no other (%d of %d)" % [vegetation.near_model_count, expected])
	var ring: Node = vegetation.get_node_or_null("NearTreeModels")
	check(ring != null, "the near models have their own node")
	if ring != null:
		var shaded := true
		for batch in ring.get_children():
			var multimesh: MultiMesh = (batch as MultiMeshInstance3D).multimesh
			if multimesh == null or multimesh.instance_count == 0:
				continue
			for surface in range(multimesh.mesh.get_surface_count()):
				var material := multimesh.mesh.surface_get_material(surface) as ShaderMaterial
				shaded = shaded and material != null and material.shader in [NearLeafShader, NearBarkShader] \
					and float(material.get_shader_parameter("near_fade_end")) == TerrainVegetation.NEAR_MODEL_RANGE \
					and float(material.get_shader_parameter("near_fade_start")) == TerrainVegetation.NEAR_MODEL_RANGE - TerrainVegetation.NEAR_FADE_BAND
		check(shaded, "near models use the leaf and bark shaders with the impostors' fade band")
	# Small moves keep the ring; a move past the refresh distance rebuilds it.
	var held: int = vegetation.near_model_count
	vegetation.update_near_ring(camera + Vector3(TerrainVegetation.NEAR_RING_REFRESH * 0.5, 0.0, 0.0))
	check(vegetation.near_model_count == held, "the ring is kept while the camera barely moves")
	var moved := camera + Vector3(400.0, 0.0, 0.0)
	vegetation.update_near_ring(moved)
	check(vegetation.near_model_count == _records_near(vegetation, moved, reach).x, "the ring follows the camera")
	# Zoomed out, there are no models at all.
	vegetation.update_near_ring(Vector3(0.0, GROUND + 1500.0, 0.0))
	check(vegetation.near_model_count == 0, "a high camera draws no tree models (%d)" % vegetation.near_model_count)
	# A levelled site leaves no model behind either.
	var site := Rect2(camera.x - 60.0, camera.z - 60.0, 120.0, 120.0)
	check(_records_near(vegetation, camera, reach, site).y > 0, "the test site has trees on it")
	vegetation.clear_area(site)
	check(_records_near(vegetation, camera, reach, site).y == 0, "levelling a site removes its trees from the grid")
	vegetation.update_near_ring(camera)
	check(vegetation.near_model_count == _records_near(vegetation, camera, reach).x and vegetation.near_model_count < expected,
		"the ring drops the levelled trees (%d, was %d)" % [vegetation.near_model_count, expected])


func _run() -> void:
	var catalog := _test_catalog()
	if not catalog.is_empty():
		await _test_placement(catalog)
	print("TREE_IMPOSTORS checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
