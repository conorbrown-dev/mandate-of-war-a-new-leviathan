extends Node3D

# Native state drives this view; this script never creates gameplay units.
var zone: MeshInstance3D
const F15C_SCENE := preload("res://assets/source_3d/reference_models/industrial_fighter_f15c.glb")
const F15C_SOURCE_LENGTH_M := 8.0
const F15C_REFERENCE_LENGTH_M := 19.44
const F15C_PRESENTATION_SCALE := F15C_REFERENCE_LENGTH_M / F15C_SOURCE_LENGTH_M

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
		var runway_offset: Vector2 = snapshot.get("runway_offset", Vector2(-55.0, 0.0))
		var runway := Vector2(delivery_x, delivery_z) + runway_offset
		var pose := landing_pose(runway, _runway_approach_axis(Vector2(delivery_x, delivery_z)), progress)
		var current: Vector3 = pose.position
		transport.position = current + Vector3(0.0, float(terrain_height.call(runway.x, runway.y)), 0.0)
		# Source +X is the aircraft's nose. Orient along the path, including
		# pitch and a shallow bank through the turn, rather than snapping yaw.
		transport.rotation = Vector3(0.0, pose.yaw, 0.0)
		transport.rotate_object_local(Vector3.BACK, pose.pitch)
		transport.rotate_object_local(Vector3.RIGHT, pose.bank)


# A tangent-continuous curved entry joins a straight 550 m final/rollout.
# The final approach touches down 220 m before the runway centre; only then
# does the aircraft decelerate to its parked position. Timing is presentation
# only: native delivery completion and unit creation remain authoritative.
static func _landing_position(runway: Vector2, axis: Vector2, progress: float) -> Vector3:
	var p := clampf(progress, 0.0, 1.0)
	if p > 0.8:
		var t := (p - 0.8) / 0.2
		p = 0.8 + 0.2 * (t + t * t - t * t * t)
	var side := Vector2(axis.y, -axis.x)
	var point: Vector2
	if p < 0.5:
		var t := p * 2.0
		point = (runway + side * 700.0 + axis * 1100.0).bezier_interpolate(
			runway + side * 350.0 + axis * 1100.0,
			runway + axis * (550.0 + 550.0 / 3.0),
			runway + axis * 550.0, t)
	else:
		point = runway + axis * (1100.0 * (1.0 - p))
	var descent := clampf(progress / 0.8, 0.0, 1.0)
	var altitude := lerpf(100.0, 0.15, smoothstep(0.0, 1.0, descent))
	return Vector3(point.x, altitude, point.y)


static func landing_pose(runway: Vector2, axis: Vector2, progress: float) -> Dictionary:
	var position := _landing_position(runway, axis, progress)
	var before := _landing_position(runway, axis, progress - 0.001)
	var after := _landing_position(runway, axis, progress + 0.001)
	var tangent := after - before
	if progress >= 0.5:
		tangent.x = 0.0
	if tangent.length_squared() < 0.000001:
		tangent = Vector3(-axis.x, 0.0, -axis.y)
	var yaw := atan2(-tangent.z, tangent.x)
	var pitch := atan2(tangent.y, Vector2(tangent.x, tangent.z).length())
	var incoming := Vector2(position.x - before.x, position.z - before.z)
	var outgoing := Vector2(after.x - position.x, after.z - position.z)
	var bank := clampf(incoming.angle_to(outgoing) * 30.0, -0.35, 0.35) if progress > 0.001 and progress < 0.5 else 0.0
	return {"position": position, "yaw": yaw, "pitch": pitch, "bank": bank}


func _runway_approach_axis(airfield: Vector2) -> Vector2:
	return Vector2(0.0, 1.0) if airfield.y >= 0.0 else Vector2(0.0, -1.0)
