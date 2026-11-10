class_name StrategicUnitIcon
extends RefCounted

const UnitCatalog := preload("res://unit_catalog.gd")

## Hidden 3D fallback marker for strategic zoom. The visible map-scale APP-6
## symbols are drawn in screen space by nato_map_symbol.gd.

static func mesh_for(unit_type: int) -> Mesh:
	var size := Vector3(1.4, 0.16, 1.4)
	match UnitCatalog.strategic_icon(unit_type):
		"armour", "recon": return _square(size)
		"sp_artillery", "engineer": return _diamond(size)
		"air_defense": return _circle(8, size)
		"missile": return _lozenge(size)
		"fighter", "recon_aircraft", "cargo_aircraft": return _triangle(3, size)
		"attack_helicopter": return _triangle(4, size)
		"naval_surface": return _ellipse(8, size)
		"command": return _hexagon(size)
		_: return _circle(8, size)


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
