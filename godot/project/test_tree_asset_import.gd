extends SceneTree

const TREE_ASSETS := [
	"res://assets/source_3d/nature/trees/oak_tree_tactical.glb",
	"res://assets/source_3d/nature/trees/oak_tree_big_003.glb",
	"res://assets/source_3d/nature/trees/oak_tree_large_001.glb",
	"res://assets/source_3d/nature/trees/oak_tree_medium_002.glb",
	"res://assets/source_3d/nature/trees/oak_tree_small_003.glb",
	"res://assets/source_3d/nature/trees/oak_tree_sapling_001.glb",
	"res://assets/source_3d/nature/trees/oak_tree_strategic.glb",
	"res://assets/source_3d/nature/trees/oak_tree_classic.glb",
	"res://assets/source_3d/nature/trees/oak_tree_animated.glb",
	"res://assets/source_3d/nature/trees/oak_tree_english_forest_01_a.glb",
	"res://assets/source_3d/nature/trees/oak_tree_english_forest_01_b.glb",
	"res://assets/source_3d/nature/trees/oak_tree_english_forest_01_c.glb",
	"res://assets/source_3d/nature/trees/oak_tree_english_forest_01_d.glb",
	"res://assets/source_3d/nature/trees/oak_tree_english_foliage_01.glb",
	"res://assets/source_3d/nature/trees/oak_tree_user_1.glb",
	"res://assets/source_3d/nature/trees/oak_tree_user_2.glb",
	"res://assets/source_3d/nature/trees/oak_tree_user_3.glb",
]

var checks := 0
var failures := 0


func check(condition: bool, reason: String) -> void:
	checks += 1
	if not condition:
		push_error(reason)
		failures += 1


func _initialize() -> void:
	var tactical_mesh: Mesh
	for path in TREE_ASSETS:
		var resource := load(path)
		check(resource is PackedScene, "tree conversion imports as a Godot PackedScene: %s" % path)
		if resource is PackedScene:
			var instance := (resource as PackedScene).instantiate()
			var mesh_nodes := instance.find_children("*", "MeshInstance3D", true, false)
			check(not mesh_nodes.is_empty(), "tree conversion retains render geometry: %s" % path)
			if path.contains("oak_tree_user_") and not mesh_nodes.is_empty():
				var tallest_mesh_height := 0.0
				for node_value in mesh_nodes:
					var mesh_node := node_value as MeshInstance3D
					tallest_mesh_height = maxf(tallest_mesh_height, mesh_node.mesh.get_aabb().size.y * mesh_node.scale.y)
				check(tallest_mesh_height <= 30.0, "active user tree has a practical Godot-space height: %s" % path)
			if path.ends_with("oak_tree_tactical.glb") and not mesh_nodes.is_empty():
				tactical_mesh = (mesh_nodes[0] as MeshInstance3D).mesh
			instance.queue_free()
	var tactical_material := tactical_mesh.surface_get_material(0) if tactical_mesh != null else null
	check(tactical_material is BaseMaterial3D and (tactical_material as BaseMaterial3D).albedo_texture != null, "active tactical oak retains its embedded bark/leaf texture material")
	print("GODOT_TREE_ASSET_IMPORT checks=%d failures=%d" % [checks, failures])
	quit(1 if failures > 0 else 0)
