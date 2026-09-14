extends Node3D

# Native state drives this view; this script never creates gameplay units.
var zone: MeshInstance3D
var transport: MeshInstance3D
var zone_material: StandardMaterial3D

func _ready() -> void:
	zone = MeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = 1.0
	mesh.bottom_radius = 1.0
	mesh.height = 0.05
	zone.mesh = mesh
	zone_material = StandardMaterial3D.new()
	zone_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	zone_material.albedo_color = Color("#47d9ff", 0.25)
	zone_material.emission_enabled = true
	zone_material.emission = Color("#47d9ff")
	zone.material_override = zone_material
	add_child(zone)
	transport = MeshInstance3D.new()
	var transport_mesh := BoxMesh.new()
	transport_mesh.size = Vector3(12.0, 3.0, 5.0)
	transport.mesh = transport_mesh
	var transport_material := StandardMaterial3D.new()
	transport_material.albedo_color = Color("#d9edf3")
	transport_material.emission_enabled = true
	transport_material.emission = Color("#47d9ff")
	transport.material_override = transport_material
	transport.visible = false
	add_child(transport)

func sync(snapshot: Dictionary, terrain_height: Callable) -> void:
	var x := float(snapshot.get("x", 0.0))
	var z := float(snapshot.get("y", 0.0))
	zone.position = Vector3(x, terrain_height.call(x, z) + 0.12, z)
	var radius := maxf(1.0, float(snapshot.get("radius", 1.0)))
	zone.scale = Vector3(radius, 1.0, radius)
	var state := int(snapshot.get("state", 0))
	zone_material.emission = Color("#47d9ff") if state <= 2 else Color("#ffbd52")
	transport.visible = state == 3
	if transport.visible:
		var progress := clampf(float(snapshot.get("elapsed_ms", 0.0)) / maxf(1.0, float(snapshot.get("duration_ms", 1.0))), 0.0, 1.0)
		transport.position = Vector3(lerpf(x - 220.0, x, progress), terrain_height.call(x, z) + 20.0 - progress * 16.0, z)
