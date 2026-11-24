extends SceneTree

const RegistryScript = preload("res://visual_definition_registry.gd")
const VisualRootScript = preload("res://unit_visual_root.gd")

func _init() -> void:
	call_deferred("_run")

func _run() -> void:
	var registry = RegistryScript.new()
	assert(registry.load_definitions())
	var wrapper = VisualRootScript.new()
	root.add_child(wrapper)
	assert(wrapper.configure(registry, "visual.elite.mbt.prototype", Color.BLUE))
	assert(wrapper.get_node("ModelRoot").get_child_count() > 0)
	wrapper.apply_simulation_transform(Transform3D(Basis.IDENTITY, Vector3(17, 2, -8)))
	assert(wrapper.global_position == Vector3(17, 2, -8))
	wrapper.set_gameplay_ranges(115.0, 150.0, 450.0)
	wrapper.set_selected(true)
	assert(wrapper.get_node("VisibilityHex").visible and wrapper.get_node("AttackHex").visible)
	assert(wrapper.hardpoint_root().name == "OptionalHardpointMarkers")
	assert(wrapper.get_node("OptionalTurretRoot/GunRoot/muzzle") != null)
	var before_muzzle := wrapper.hardpoint_transform("muzzle").origin
	wrapper.apply_presentation_pose(PI * 0.5, 0.25, 0.1)
	assert(is_equal_approx(wrapper.get_node("OptionalTurretRoot").rotation.y, PI * 0.5))
	assert(not wrapper.hardpoint_transform("muzzle").origin.is_equal_approx(before_muzzle))
	assert(wrapper.global_position == Vector3(17, 2, -8))
	assert(String(wrapper.presentation_footprint().shape) == "circle")
	wrapper.update_lod(250.0)
	# 250 m is the reduced-detail band; the imported model remains visible until
	# the strategic overlay's 600 m cutoff.
	assert(wrapper.get_node("ModelRoot").visible)
	# Strategic markers are rendered by the external screen-space overlay; the
	# per-unit 3D strategic visual remains hidden at strategic zoom.
	assert(not wrapper.get_node("StrategicZoomVisual").visible)
	wrapper.update_lod(600.0)
	assert(not wrapper.get_node("ModelRoot").visible)
	wrapper.update_lod(10.0)
	assert(wrapper.get_node("ModelRoot").visible)
	var fallback = VisualRootScript.new()
	root.add_child(fallback)
	assert(not fallback.configure(registry, "missing.visual", Color.BLUE))
	assert(fallback.get_node("ModelRoot").get_child(0).name == "DevelopmentFallbackMesh")
	# The F-15C lit black: a mirrored transform and a quarter of its triangles
	# wound against their normals. Built through the real pipeline it must have
	# neither, so double-sided drawing never flips a sunlit face's normal.
	var eagle = VisualRootScript.new()
	root.add_child(eagle)
	assert(eagle.configure(registry, "visual.industrial.fighter.f15c.prototype", Color.BLUE))
	var eagle_meshes: Array = eagle.get_node("ModelRoot").find_children("*", "MeshInstance3D", true, false)
	assert(not eagle_meshes.is_empty())
	var mirrored := 0
	var triangles := 0
	var against := 0
	for mesh_instance in eagle_meshes:
		if (mesh_instance as MeshInstance3D).transform.basis.determinant() < 0.0:
			mirrored += 1
		for surface in range(mesh_instance.mesh.get_surface_count()):
			var arrays: Array = mesh_instance.mesh.surface_get_arrays(surface)
			var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
			var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
			for start in range(0, indices.size() - 2, 3):
				var a := indices[start]
				var b := indices[start + 1]
				var c := indices[start + 2]
				var facing := (vertices[c] - vertices[a]).cross(vertices[b] - vertices[a])
				if facing.length() == 0.0:
					continue
				triangles += 1
				# Edge-on triangles (normals in their own plane) face neither way;
				# only a clear disagreement flips lighting.
				var sum := normals[a] + normals[b] + normals[c]
				if facing.dot(sum) < -0.01 * facing.length() * sum.length():
					against += 1
	assert(mirrored == 0, "the F-15C carries no mirrored transform (%d meshes do)" % mirrored)
	assert(triangles > 3000 and against == 0, "every F-15C triangle winds with its normals (%d of %d do not)" % [against, triangles])
	print("UNIT_VISUAL_ROOT checks=22 failures=0")
	quit()
