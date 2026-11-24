extends RefCounted
## The forward command post, built from Godot primitives only.
##
## A graded gravel pad inside a HESCO blast-wall perimeter with a gate to the
## south (+Z). On it: a two-storey prefab headquarters with a faction-coloured
## roof marking, a satellite dish and two antenna masts on the roof line, an
## open vehicle bay with a parked utility truck, a containerised generator with
## a fuel bladder, a canvas operations tent, floodlights and a flag.
##
## Cost: the HESCO wall and the sandbag ring are one MultiMesh each; every other
## part is a primitive mesh sharing one of a few cached materials. Only the
## faction accent material is per colour.
##
## Local axes: X east, Z south, Y up; the origin is the centre of the pad at its
## highest ground point. `ground_drop` extends the pad's skirt down so a slope
## under the footprint never shows a gap.

const FOOTPRINT := Vector2(56.0, 42.0)
const PAD_TOP := 0.35
const HQ_SIZE := Vector3(20.0, 4.4, 10.0)
const HQ_CENTER := Vector2(-10.0, -8.0)
const MAST_HEIGHTS := [26.0, 18.0]
const HESCO_SIZE := Vector3(2.2, 2.3, 1.1)
const GATE_WIDTH := 10.0

const CONCRETE_TEXTURE := "res://assets/airfield/concrete_floor_02_diff_2k.jpg"

static var _shared: Dictionary = {}
static var _accents: Dictionary = {}


static func _material(key: String) -> StandardMaterial3D:
	if _shared.has(key):
		return _shared[key]
	var material := StandardMaterial3D.new()
	match key:
		"pad":
			material.albedo_color = Color("#6e6b60")
			material.albedo_texture = load(CONCRETE_TEXTURE)
			material.uv1_triplanar = true
			material.uv1_scale = Vector3(0.08, 0.08, 0.08)
			material.roughness = 0.95
		"hesco":
			material.albedo_color = Color("#8c7b57")
			material.roughness = 0.95
		"hull":
			material.albedo_color = Color("#5d6656")
			material.roughness = 0.7
			material.metallic = 0.25
		"roof":
			material.albedo_color = Color("#3c433b")
			material.roughness = 0.8
			material.metallic = 0.2
		"glass":
			material.albedo_color = Color("#11202a")
			material.roughness = 0.15
			material.metallic = 0.6
			material.emission_enabled = true
			material.emission = Color("#8fd6ff")
			material.emission_energy_multiplier = 0.35
		"white":
			material.albedo_color = Color("#d9dcd6")
			material.roughness = 0.5
			material.metallic = 0.2
		"steel":
			material.albedo_color = Color("#6f7472")
			material.roughness = 0.45
			material.metallic = 0.7
		"canvas":
			material.albedo_color = Color("#6b6a4b")
			material.roughness = 1.0
		"lamp":
			material.albedo_color = Color("#fff1c9")
			material.emission_enabled = true
			material.emission = Color("#ffe3a3")
			material.emission_energy_multiplier = 3.0
		"warning":
			material.albedo_color = Color("#ff3b2f")
			material.emission_enabled = true
			material.emission = Color("#ff3b2f")
			material.emission_energy_multiplier = 2.5
	_shared[key] = material
	return material


static func accent_material(color: Color) -> StandardMaterial3D:
	var key := color.to_html()
	if _accents.has(key):
		return _accents[key]
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.5
	material.emission_enabled = true
	material.emission = color
	material.emission_energy_multiplier = 1.1
	_accents[key] = material
	return material


static func build(faction_color: Color, ground_drop: float = 1.0) -> Node3D:
	var root := Node3D.new()
	root.set_meta("footprint", FOOTPRINT)
	root.set_meta("faction_color", faction_color)
	var accent := accent_material(faction_color)
	var drop := maxf(ground_drop, 0.5)
	# Graded pad: its top sits PAD_TOP above the origin, its skirt reaches down.
	_box(root, "Pad", Vector3(FOOTPRINT.x, PAD_TOP + drop, FOOTPRINT.y), Vector3(0, (PAD_TOP - drop) * 0.5, 0), _material("pad"))
	_hesco_perimeter(root)
	_headquarters(root, accent)
	_vehicle_bay(root)
	_generator(root)
	_ops_tent(root)
	_lighting(root, accent)
	_selection_ring(root, faction_color)
	return root


static func _box(root: Node3D, name: String, size: Vector3, position: Vector3, material: Material, rotation_degrees := Vector3.ZERO) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	return _part(root, name, mesh, position, material, rotation_degrees)


static func _cylinder(root: Node3D, name: String, top: float, bottom: float, height: float, position: Vector3, material: Material, segments := 8, rotation_degrees := Vector3.ZERO) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top
	mesh.bottom_radius = bottom
	mesh.height = height
	mesh.radial_segments = segments
	mesh.rings = 1
	return _part(root, name, mesh, position, material, rotation_degrees)


static func _part(root: Node3D, name: String, mesh: Mesh, position: Vector3, material: Material, rotation_degrees := Vector3.ZERO) -> MeshInstance3D:
	var part := MeshInstance3D.new()
	part.name = name
	part.mesh = mesh
	part.material_override = material
	part.position = position
	part.rotation_degrees = rotation_degrees
	root.add_child(part)
	return part


# Blast-wall cells around the pad edge, one MultiMesh, with the gate gap to the south.
static func _hesco_perimeter(root: Node3D) -> void:
	var transforms: Array[Transform3D] = []
	var half := FOOTPRINT * 0.5 - Vector2(HESCO_SIZE.z, HESCO_SIZE.z) * 0.5
	var y := PAD_TOP + HESCO_SIZE.y * 0.5
	for side in [-1.0, 1.0]:
		var count := int(floor(FOOTPRINT.x / HESCO_SIZE.x))
		for index in range(count):
			var x := -FOOTPRINT.x * 0.5 + HESCO_SIZE.x * (index + 0.5)
			if side > 0.0 and absf(x) < GATE_WIDTH * 0.5:
				continue
			transforms.append(Transform3D(Basis.IDENTITY, Vector3(x, y, side * half.y)))
		count = int(floor((FOOTPRINT.y - HESCO_SIZE.z * 2.0) / HESCO_SIZE.x))
		for index in range(count):
			var z := -FOOTPRINT.y * 0.5 + HESCO_SIZE.z + HESCO_SIZE.x * (index + 0.5)
			transforms.append(Transform3D(Basis(Vector3.UP, PI * 0.5), Vector3(side * half.x, y, z)))
	var mesh := BoxMesh.new()
	mesh.size = HESCO_SIZE
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = mesh
	multimesh.instance_count = transforms.size()
	for index in range(transforms.size()):
		multimesh.set_instance_transform(index, transforms[index])
	var wall := MultiMeshInstance3D.new()
	wall.name = "HescoPerimeter"
	wall.multimesh = multimesh
	wall.material_override = _material("hesco")
	root.add_child(wall)
	# Gate posts in the faction colour, and a striped barrier arm.
	for side in [-1.0, 1.0]:
		_box(root, "GatePost", Vector3(0.9, 3.4, 0.9), Vector3(side * (GATE_WIDTH * 0.5 + 0.2), PAD_TOP + 1.7, half.y), accent_material(root.get_meta("faction_color")))
	_box(root, "GateBarrier", Vector3(GATE_WIDTH - 1.0, 0.25, 0.25), Vector3(0.0, PAD_TOP + 1.1, half.y), _material("white"))


# Two-storey prefab HQ: ground-floor modules, an operations room above with a
# window band, a faction roof marking, a satellite dish and the antenna masts.
static func _headquarters(root: Node3D, accent: Material) -> void:
	var base := Vector3(HQ_CENTER.x, PAD_TOP, HQ_CENTER.y)
	_box(root, "Headquarters", HQ_SIZE, base + Vector3(0, HQ_SIZE.y * 0.5, 0), _material("hull"))
	# Window band along the long south face and a door in the faction colour.
	_box(root, "HqWindows", Vector3(HQ_SIZE.x - 3.0, 0.9, HQ_SIZE.z + 0.12), base + Vector3(0, HQ_SIZE.y * 0.62, 0), _material("glass"))
	_box(root, "HqDoor", Vector3(1.8, 2.6, 0.3), base + Vector3(5.0, 1.3, HQ_SIZE.z * 0.5 + 0.1), accent)
	_box(root, "HqRoof", Vector3(HQ_SIZE.x + 0.6, 0.4, HQ_SIZE.z + 0.6), base + Vector3(0, HQ_SIZE.y + 0.2, 0), _material("roof"))
	# The upper operations room sits toward the west end.
	var upper := Vector3(9.0, 3.2, 8.0)
	var upper_center := base + Vector3(-4.5, HQ_SIZE.y + 0.4 + upper.y * 0.5, 0)
	_box(root, "OperationsRoom", upper, upper_center, _material("hull"))
	_box(root, "OperationsWindows", Vector3(upper.x + 0.12, 1.0, upper.z + 0.12), upper_center + Vector3(0, 0.3, 0), _material("glass"))
	_box(root, "OperationsRoof", Vector3(upper.x + 0.5, 0.35, upper.z + 0.5), upper_center + Vector3(0, upper.y * 0.5 + 0.17, 0), _material("roof"))
	# Faction identification: a broad roof panel and a stripe round the parapet,
	# both readable from the default strategic camera.
	_box(root, "RoofMarking", Vector3(8.0, 0.12, HQ_SIZE.z - 2.0), base + Vector3(5.0, HQ_SIZE.y + 0.46, 0), accent)
	_box(root, "AccentStripe", Vector3(HQ_SIZE.x + 0.7, 0.5, HQ_SIZE.z + 0.7), base + Vector3(0, HQ_SIZE.y - 0.3, 0), accent)
	# Satellite dish on the east roof, tilted to the sky.
	var dish_base := base + Vector3(HQ_SIZE.x * 0.5 - 2.5, HQ_SIZE.y + 0.4, -2.0)
	_cylinder(root, "DishPedestal", 0.25, 0.35, 1.6, dish_base + Vector3(0, 0.8, 0), _material("steel"), 6)
	_cylinder(root, "SatelliteDish", 2.2, 0.35, 0.9, dish_base + Vector3(0, 2.0, 0), _material("white"), 12, Vector3(-35.0, 0.0, 20.0))
	# Antenna masts: a tall HF mast and a shorter whip, each with a warning light.
	var mast_spots := [Vector3(base.x - HQ_SIZE.x * 0.5 - 2.0, PAD_TOP, base.z - 2.0), Vector3(base.x + 2.0, HQ_SIZE.y + 0.4 + PAD_TOP, base.z + 3.0)]
	for index in range(MAST_HEIGHTS.size()):
		var height: float = MAST_HEIGHTS[index]
		var foot: Vector3 = mast_spots[index]
		_cylinder(root, "AntennaMast%d" % index, 0.12, 0.32, height, foot + Vector3(0, height * 0.5, 0), _material("steel"), 6)
		_box(root, "MastLight%d" % index, Vector3(0.6, 0.6, 0.6), foot + Vector3(0, height + 0.3, 0), _material("warning"))
	# Cross-arms on the tall mast.
	var tall: Vector3 = mast_spots[0]
	for arm_height in [0.55, 0.8]:
		_box(root, "MastArm", Vector3(3.2, 0.12, 0.12), tall + Vector3(0, MAST_HEIGHTS[0] * arm_height, 0), _material("steel"))
	# Flag on a pole in front of the HQ.
	var pole := Vector3(base.x - 6.0, PAD_TOP, base.z + HQ_SIZE.z * 0.5 + 4.0)
	_cylinder(root, "FlagPole", 0.08, 0.12, 9.0, pole + Vector3(0, 4.5, 0), _material("white"), 6)
	_box(root, "Flag", Vector3(2.6, 1.6, 0.06), pole + Vector3(1.35, 8.0, 0), accent)


# Open-sided vehicle bay on the east side with a parked utility truck.
static func _vehicle_bay(root: Node3D) -> void:
	var center := Vector3(15.0, PAD_TOP, 4.0)
	var size := Vector3(14.0, 5.0, 9.0)
	_box(root, "VehicleBayRoof", Vector3(size.x + 0.6, 0.35, size.z + 0.6), center + Vector3(0, size.y, 0), _material("roof"), Vector3(0, 0, 3.0))
	for x in [-0.5, 0.5]:
		for z in [-0.5, 0.5]:
			_box(root, "VehicleBayPost", Vector3(0.3, size.y, 0.3), center + Vector3(x * size.x, size.y * 0.5, z * size.z), _material("steel"))
	_box(root, "VehicleBayBackWall", Vector3(size.x, size.y, 0.25), center + Vector3(0, size.y * 0.5, -size.z * 0.5), _material("hull"))
	var truck := center + Vector3(-2.0, 0, 0.5)
	_box(root, "TruckCab", Vector3(2.5, 2.4, 2.2), truck + Vector3(-3.0, 1.6, 0), _material("hull"))
	_box(root, "TruckBed", Vector3(5.0, 2.8, 2.4), truck + Vector3(0.9, 1.8, 0), _material("canvas"))
	_box(root, "TruckChassis", Vector3(7.6, 0.8, 2.3), truck + Vector3(-0.4, 0.6, 0), _material("roof"))


# Containerised generator and fuel bladder in the north-east corner, behind sandbags.
static func _generator(root: Node3D) -> void:
	var center := Vector3(17.0, PAD_TOP, -12.0)
	_box(root, "Generator", Vector3(6.0, 2.6, 2.4), center + Vector3(0, 1.3, 0), _material("hull"))
	_box(root, "GeneratorVent", Vector3(2.0, 0.2, 1.6), center + Vector3(1.5, 2.7, 0), _material("steel"))
	_cylinder(root, "GeneratorExhaust", 0.18, 0.18, 2.2, center + Vector3(-2.4, 3.4, 0.6), _material("steel"), 6)
	_cylinder(root, "FuelBladder", 2.2, 2.6, 1.0, center + Vector3(-7.0, 0.5, 1.0), _material("roof"), 12)
	# A low sandbag ring round the bladder: one MultiMesh.
	var mesh := BoxMesh.new()
	mesh.size = Vector3(1.2, 0.6, 0.6)
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.mesh = mesh
	var count := 14
	multimesh.instance_count = count * 2
	for layer in range(2):
		for index in range(count):
			var angle := TAU * (index + 0.5 * layer) / count
			var at := center + Vector3(-7.0 + cos(angle) * 3.6, 0.3 + layer * 0.6, 1.0 + sin(angle) * 3.6)
			multimesh.set_instance_transform(layer * count + index, Transform3D(Basis(Vector3.UP, -angle + PI * 0.5), at))
	var bags := MultiMeshInstance3D.new()
	bags.name = "SandbagRing"
	bags.multimesh = multimesh
	bags.material_override = _material("hesco")
	root.add_child(bags)


# A canvas operations tent on the west side.
static func _ops_tent(root: Node3D) -> void:
	var mesh := PrismMesh.new()
	mesh.size = Vector3(6.0, 3.2, 10.0)
	_part(root, "OperationsTent", mesh, Vector3(-19.0, PAD_TOP + 1.6, 9.0), _material("canvas"))


# Floodlight masts at two corners and faction beacons on the gate, emissive only:
# no real light sources, so the post costs nothing extra to light.
static func _lighting(root: Node3D, accent: Material) -> void:
	var half := FOOTPRINT * 0.5 - Vector2(2.5, 2.5)
	for corner in [Vector2(-half.x, half.y), Vector2(half.x, -half.y), Vector2(half.x, half.y)]:
		_cylinder(root, "FloodlightPole", 0.1, 0.16, 8.0, Vector3(corner.x, PAD_TOP + 4.0, corner.y), _material("steel"), 6)
		_box(root, "FloodlightHead", Vector3(1.2, 0.5, 0.6), Vector3(corner.x, PAD_TOP + 8.1, corner.y), _material("lamp"))
	_box(root, "GateBeacon", Vector3(0.5, 0.5, 0.5), Vector3(-(GATE_WIDTH * 0.5 + 0.2), PAD_TOP + 3.65, FOOTPRINT.y * 0.5 - HESCO_SIZE.z * 0.5), accent)
	_box(root, "GateBeacon", Vector3(0.5, 0.5, 0.5), Vector3(GATE_WIDTH * 0.5 + 0.2, PAD_TOP + 3.65, FOOTPRINT.y * 0.5 - HESCO_SIZE.z * 0.5), accent)


# Shown while the post is selected: a flat frame just outside the perimeter.
static func _selection_ring(root: Node3D, color: Color) -> void:
	var ring := Node3D.new()
	ring.name = "SelectionRing"
	ring.visible = false
	root.add_child(ring)
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color(color, 0.9)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	var outer := FOOTPRINT + Vector2(6.0, 6.0)
	for side in [-1.0, 1.0]:
		_box(ring, "RingEdge", Vector3(outer.x, 0.3, 0.8), Vector3(0, PAD_TOP + 0.2, side * outer.y * 0.5), material)
		_box(ring, "RingEdge", Vector3(0.8, 0.3, outer.y), Vector3(side * outer.x * 0.5, PAD_TOP + 0.2, 0), material)
	for part in ring.get_children():
		(part as MeshInstance3D).cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
