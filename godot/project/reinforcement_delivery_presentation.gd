extends Node3D

# Native state drives this view; this script never creates gameplay units.
var zone: MeshInstance3D
const F15C_SCENE := preload("res://assets/source_3d/reference_models/industrial_fighter_f15c.glb")
const F15C_SOURCE_LENGTH_M := 8.0
const F15C_REFERENCE_LENGTH_M := 19.44
const F15C_PRESENTATION_SCALE := F15C_REFERENCE_LENGTH_M / F15C_SOURCE_LENGTH_M
const RUNWAY_APPROACH_DISTANCE_M := 600.0

var transport: Node3D
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
	transport = F15C_SCENE.instantiate()
	# The converted source mesh is 8 m long; an F-15C is about 19.44 m long.
	# Keep the delivery visual at that authored real-world scale.
	transport.scale = Vector3(F15C_PRESENTATION_SCALE, F15C_PRESENTATION_SCALE, F15C_PRESENTATION_SCALE)
	transport.visible = false
	add_child(transport)

func sync(snapshot: Dictionary, terrain_height: Callable) -> void:
	var x := float(snapshot.get("x", 0.0))
	var z := float(snapshot.get("y", 0.0))
	zone.visible = false # Delivery anchors are airfields, not arbitrary terrain zones.
	var state := int(snapshot.get("state", 0))
	zone_material.emission = Color("#47d9ff") if state <= 2 else Color("#ffbd52")
	# Keep the delivery aircraft parked on its selected runway after unloading.
	transport.visible = state == 3 or state == 4
	if transport.visible:
		var progress := 1.0 if state == 4 else clampf(float(snapshot.get("elapsed_ms", 0.0)) / maxf(1.0, float(snapshot.get("duration_ms", 1.0))), 0.0, 1.0)
		var delivery_x := float(snapshot.get("delivery_x", x))
		var delivery_z := float(snapshot.get("delivery_y", z))
		var approach_axis := _runway_approach_axis(Vector2(delivery_x, delivery_z))
		var approach_start := Vector2(delivery_x, delivery_z) + approach_axis * RUNWAY_APPROACH_DISTANCE_M
		var current := approach_start.lerp(Vector2(delivery_x, delivery_z), progress)
		transport.position = Vector3(current.x, terrain_height.call(delivery_x, delivery_z) + lerpf(20.0, 0.15, progress), current.y)
		# Source +X is the F-15C longitudinal axis. Align it with the selected
		# diagonal runway and its inbound travel direction.
		var travel := -approach_axis
		transport.rotation.y = atan2(-travel.y, travel.x)


func _runway_approach_axis(airfield: Vector2) -> Vector2:
	var outward := airfield.normalized()
	if outward.length_squared() <= 0.0001:
		outward = Vector2(-1.0, 0.0)
	var diagonal_a := Vector2(1.0, 1.0).normalized()
	var diagonal_b := Vector2(1.0, -1.0).normalized()
	var axis := diagonal_a if absf(outward.dot(diagonal_a)) >= absf(outward.dot(diagonal_b)) else diagonal_b
	return axis if outward.dot(axis) >= 0.0 else -axis
