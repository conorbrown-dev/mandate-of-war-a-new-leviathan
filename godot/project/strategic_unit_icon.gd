class_name StrategicUnitIcon
extends RefCounted

## NATO symbology for strategic icons. Each unit type is represented by a
## standardized symbol following APP-115 (NATO symbology standards).

static func mesh_for(unit_type: int) -> Mesh:
	match unit_type:
		0: return _square(Vector3(1.4, 0.16, 1.4)) # Elite MBT - Fixed wing unit (rectangle)
		1: return _diamond(Vector3(1.4, 0.16, 1.4)) # Artillery - Engineering (diamond)
		2: return _circle(8, Vector3(1.4, 0.16, 1.4)) # Elite anti-air - Air defense (circle)
		3: return _square(Vector3(1.4, 0.16, 1.4)) # Swarm tank - Mobile gun system (rectangle)
		4: return _square(Vector3(1.4, 0.16, 1.4)) # Assault vehicle - Infantry fighting vehicle (rectangle)
		5: return _circle(8, Vector3(1.4, 0.16, 1.4)) # Mass anti-air - Air defense (circle)
		6: return _square(Vector3(1.4, 0.16, 1.4)) # Industrial MBT - Fixed wing unit (rectangle)
		7: return _lozenge(Vector3(1.4, 0.16, 1.4)) # Missile platform - Missile (lozenge/rhombus)
		8: return _diamond(Vector3(1.4, 0.16, 1.4)) # Engineering vehicle - Engineering (diamond)
		9: return _triangle(3, Vector3(1.4, 0.16, 1.4)) # Elite T1 fighter - Fixed wing aircraft (triangle)
		10: return _triangle(4, Vector3(1.4, 0.16, 1.4)) # Elite VTOL - Rotocraft (triangle)
		11: return _ellipse(8, Vector3(1.4, 0.16, 1.4)) # Patrol boat - Naval (ellipse)
		12: return _hexagon(Vector3(1.4, 0.16, 1.4)) # Command Walker - Command and control (hexagon)
		13: return _triangle(3, Vector3(1.4, 0.16, 1.4)) # Reserved bomber - Fixed wing aircraft (obtuse triangle)
		_: return _circle(8, Vector3(1.4, 0.16, 1.4))


static func shape_scale_for(unit_type: int) -> Vector3:
	match unit_type:
		_: return Vector3.ONE


static func rotation_for(unit_type: int) -> float:
	return 0.0


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


static func _square(size: Vector3) -> BoxMesh:
	return _box(size)


static func _circle(segments: int, size: Vector3) -> CylinderMesh:
	return _cylinder(segments, size.x * 0.5)


static func _ellipse(segments: int, size: Vector3) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = size.x * 0.5
	mesh.bottom_radius = size.x * 0.5
	mesh.height = size.y
	mesh.radial_segments = segments
	return mesh


static func _triangle(segments: int, size: Vector3) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = size.x * 0.5
	mesh.bottom_radius = size.x * 0.5
	mesh.height = size.y
	mesh.radial_segments = segments
	return mesh


static func _diamond(size: Vector3) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = size.x * 0.5
	mesh.bottom_radius = size.x * 0.5
	mesh.height = size.y
	mesh.radial_segments = 4
	return mesh


static func _lozenge(size: Vector3) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = size.x * 0.5
	mesh.bottom_radius = size.x * 0.5
	mesh.height = size.y
	mesh.radial_segments = 4
	return mesh


static func _hexagon(size: Vector3) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = size.x * 0.5
	mesh.bottom_radius = size.x * 0.5
	mesh.height = size.y
	mesh.radial_segments = 6
	return mesh
