extends SceneTree
## Generates the tree and bush meshes drawn close to the camera, with the
## vendored EZ-Tree generator (vegetation/ez_tree), and saves each as a mesh
## resource in assets/vegetation/near/. tools/bake_tree_impostors.gd then bakes
## the distant impostors from these same meshes, so a tree looks the same when
## the camera closes in and it turns from a picture into a model.
##
##   godot --headless --path godot/project --script res://tools/build_near_trees.gd
##
## Presets are EZ-Tree's own (vegetation/ez_tree/presets); a different seed
## grows a different tree of the same species.

const Generator := preload("res://vegetation/ez_tree/ez_tree_generator.gd")
const PRESETS := "res://vegetation/ez_tree/presets"
const OUTPUT_DIR := "res://assets/vegetation/near"

# kind: conifer, broadleaf, sapling or shrub, as in the impostor catalog.
# leaf_tint and bark_tint (hex) replace the preset's: the pine preset's needles
# are lime and its bark nearly white, and aspens are in autumn yellow, which
# is kept for one aspen only.
const VARIANTS := [
	{"key": "pine_a", "kind": "conifer", "preset": "pine_large", "seed": 44166, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "pine_b", "kind": "conifer", "preset": "pine_large", "seed": 51203, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "pine_c", "kind": "conifer", "preset": "pine_large", "seed": 70911, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "pine_d", "kind": "conifer", "preset": "pine_medium", "seed": 13377, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "pine_e", "kind": "conifer", "preset": "pine_medium", "seed": 90021, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "oak_a", "kind": "broadleaf", "preset": "oak_large", "seed": 23399},
	{"key": "oak_b", "kind": "broadleaf", "preset": "oak_large", "seed": 48120},
	{"key": "oak_c", "kind": "broadleaf", "preset": "oak_medium", "seed": 6611},
	{"key": "ash_a", "kind": "broadleaf", "preset": "ash_large", "seed": 30302},
	{"key": "ash_b", "kind": "broadleaf", "preset": "ash_large", "seed": 77017},
	{"key": "ash_c", "kind": "broadleaf", "preset": "ash_medium", "seed": 52441},
	{"key": "aspen_a", "kind": "broadleaf", "preset": "aspen_large", "seed": 11829, "leaf_tint": "70b062"},
	{"key": "aspen_b", "kind": "broadleaf", "preset": "aspen_large", "seed": 64750, "leaf_tint": "70b062"},
	{"key": "aspen_c", "kind": "broadleaf", "preset": "aspen_medium", "seed": 29083},
	{"key": "young_pine_a", "kind": "sapling", "preset": "pine_small", "seed": 8123, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "young_pine_b", "kind": "sapling", "preset": "pine_small", "seed": 31337, "leaf_tint": "8ca07a", "bark_tint": "b39a85"},
	{"key": "bush_a", "kind": "shrub", "preset": "bush_1", "seed": -1},
	{"key": "bush_b", "kind": "shrub", "preset": "bush_2", "seed": -1},
	{"key": "bush_c", "kind": "shrub", "preset": "bush_3", "seed": -1},
]


func _initialize() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUTPUT_DIR))
	var failures := 0
	for variant in VARIANTS:
		var preset = JSON.parse_string(FileAccess.get_file_as_string(PRESETS.path_join("%s.json" % variant.preset)))
		if not preset is Dictionary:
			push_error("Preset %s is missing or unreadable" % variant.preset)
			failures += 1
			continue
		var generator: Node3D = Generator.new()
		generator.apply_preset(preset)
		if int(variant.seed) >= 0:
			generator.rng_seed = int(variant.seed)
		if variant.has("leaf_tint"):
			generator.leaf_tint = Color(String(variant.leaf_tint))
		if variant.has("bark_tint"):
			generator.bark_tint = Color(String(variant.bark_tint))
		generator.generate()
		var mesh: ArrayMesh = generator.get_tree_mesh()
		var triangles := 0
		for surface in range(mesh.get_surface_count()):
			triangles += mesh.surface_get_arrays(surface)[Mesh.ARRAY_INDEX].size() / 3
		var path := OUTPUT_DIR.path_join("%s.res" % variant.key)
		if ResourceSaver.save(mesh, path, ResourceSaver.FLAG_COMPRESS) != OK:
			push_error("Could not save %s" % path)
			failures += 1
		var bounds := mesh.get_aabb()
		print("NEAR_TREE %s kind=%s triangles=%d height=%.1f width=%.1f" % [variant.key, variant.kind, triangles, bounds.end.y, maxf(bounds.size.x, bounds.size.z)])
		generator.free()
	quit(1 if failures > 0 else 0)
