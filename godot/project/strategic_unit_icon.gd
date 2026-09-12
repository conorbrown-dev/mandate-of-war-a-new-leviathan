class_name StrategicUnitIcon
extends RefCounted

## Generated SupCom-style strategic placeholders. These silhouettes can later
## be replaced by authored icon meshes without changing LOD ownership.

static func mesh_for(unit_type: int) -> Mesh:
	match unit_type:
		0: return _box(Vector3(1.35, 0.16, 1.85)) # Elite MBT
		1: return _box(Vector3(0.85, 0.16, 2.15)) # Artillery
		2: return _cylinder(4, 0.9) # Elite anti-air
		3: return _cylinder(6, 0.88) # Swarm tank
		4: return _box(Vector3(1.9, 0.16, 1.15)) # Assault vehicle
		5: return _cylinder(8, 0.88) # Mass anti-air
		6: return _box(Vector3(1.65, 0.16, 1.65)) # Industrial MBT
		7: return _cylinder(4, 0.82) # Missile platform
		8: return _cylinder(8, 0.68) # Engineering vehicle
		9: return _cylinder(3, 0.72) # Fighter, acute triangle
		10: return _cylinder(4, 0.72) # VTOL
		11: return _cylinder(5, 0.78) # Patrol boat
		12: return _cylinder(6, 0.95) # Command Walker
		13: return _cylinder(3, 0.78) # Reserved bomber, obtuse triangle
		_: return _cylinder(6, 0.78)


static func shape_scale_for(unit_type: int) -> Vector3:
	match unit_type:
		1: return Vector3(1.0, 1.0, 1.35)
		7: return Vector3(1.0, 1.0, 1.55)
		9: return Vector3(0.62, 1.0, 1.8)
		10: return Vector3(1.45, 1.0, 1.15)
		11: return Vector3(1.8, 1.0, 0.82)
		12: return Vector3(1.35, 1.0, 1.35)
		13: return Vector3(1.8, 1.0, 0.78)
		_: return Vector3.ONE


static func rotation_for(unit_type: int) -> float:
	return deg_to_rad(30.0) if unit_type in [7, 9, 10, 13] else 0.0


static func material_for(faction_color: Color) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("#b8bec0")
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.emission_enabled = true
	material.emission = faction_color.darkened(0.55)
	material.emission_energy_multiplier = 0.55
	return material


static func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


static func _cylinder(segments: int, radius: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = 0.16
	mesh.radial_segments = segments
	return mesh
