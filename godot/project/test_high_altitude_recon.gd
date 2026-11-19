extends SceneTree

# High-altitude strategic recon through the real Godot boundary: a recon plane
# is ordered with queue_faction_unit, flies its flight plan in from the west map
# edge, and passes straight over one enemy unit. The native simulation decides
# who can engage it; this script only reads health back.

const STRATEGIC_RECON := 20
const LONGSIGHT_RECON := 14
const ELITE := 0
const MASS := 1
const MASS_SWARM_TANK := 3
const MASS_ASSAULT_VEHICLE := 4
const MASS_ANTI_AIR := 5
const TARGET := Vector2(1000.0, 1000.0)
# On the inbound leg, which runs along y = TARGET.y from the west edge.
const SHOOTER := Vector2(-1500.0, 1000.0)
const STEP_MS := 50.0
const MAX_STEPS := 1400

var checks := 0
var failures: Array[String] = []
var bridge: Object


func check(condition: bool, message: String) -> void:
	checks += 1
	if not condition:
		failures.append(message)
		print("VALIDATION FAIL: %s" % message)


# Returns the plane's lowest health fraction over its inbound pass, or -1 when
# the flight never happened.
func fly_over(recon_type: int, shooter_type: int) -> float:
	bridge.call("reset_simulation")
	bridge.call("configure_world_size", 8000.0, 8000.0)
	bridge.call("start_simulation")
	if int(bridge.call("create_faction_base", ELITE, 3000.0, 3000.0)) < 0:
		return -1.0
	var shooter := int(bridge.call("create_unit_with_type", SHOOTER.x, SHOOTER.y, shooter_type, MASS))
	if shooter < 0:
		return -1.0
	var before: PackedInt32Array = bridge.call("get_entity_ids")
	if not bool(bridge.call("queue_faction_unit", ELITE, recon_type, TARGET.x, TARGET.y)):
		return -1.0
	var plane := -1
	var lowest := 1.0
	for step in range(MAX_STEPS):
		bridge.call("update_simulation", STEP_MS)
		if plane < 0:
			for id in bridge.call("get_entity_ids"):
				if not before.has(id):
					plane = int(id)
			continue
		var health: Array = bridge.call("get_unit_health", plane)
		if health.size() < 2:
			return 0.0  # shot down
		lowest = minf(lowest, float(health[0]) / float(health[1]))
		var position: PackedFloat32Array = bridge.call("get_unit_position", plane)
		if position[0] > SHOOTER.x + 600.0:
			return lowest  # well past the shooter
	return lowest if plane >= 0 else -1.0


func _init() -> void:
	OS.set_environment("RTS_DATA_ROOT", ProjectSettings.globalize_path("res://../../data").simplify_path())
	bridge = ClassDB.instantiate("RtsExtension")
	check(bridge != null, "RtsExtension could not be instantiated")
	if bridge != null:
		check(not String(bridge.call("get_unit_visual_id", STRATEGIC_RECON)).is_empty(), "strategic recon has a visual")

		var by_aa := fly_over(STRATEGIC_RECON, MASS_ANTI_AIR)
		print("HIGH_ALTITUDE_RECON strategic recon over anti-air: lowest health %.3f" % by_aa)
		check(by_aa >= 0.0, "strategic recon flew its flight plan")
		check(by_aa < 1.0, "advanced anti-air damages strategic recon at 24,000 m")

		for shooter_type in [MASS_SWARM_TANK, MASS_ASSAULT_VEHICLE]:
			var by_gun := fly_over(STRATEGIC_RECON, shooter_type)
			print("HIGH_ALTITUDE_RECON strategic recon over unit %d: lowest health %.3f" % [shooter_type, by_gun])
			check(by_gun == 1.0, "unit %d cannot reach strategic recon" % shooter_type)

		# Control: the same anti-air on the same line also hits a plane at normal altitude.
		# Only Elite fields fighters and strategic recon, so a fighter cannot meet an
		# enemy strategic recon plane here; tests/test_air_altitude.cpp covers that.
		var control := fly_over(LONGSIGHT_RECON, MASS_ANTI_AIR)
		print("HIGH_ALTITUDE_RECON ordinary recon over anti-air: lowest health %.3f" % control)
		check(control >= 0.0 and control < 1.0, "anti-air on the inbound line hits ordinary recon")
		bridge.call("reset_simulation")
	print("HIGH_ALTITUDE_RECON checks=%d failures=%d" % [checks, failures.size()])
	quit(1 if failures.size() > 0 else 0)
