extends RefCounted
## Authored sound choices, independent of weapon damage/targeting rules.
const Catalog := preload("res://unit_catalog.gd")
const CANNON := "res://audio/kenney_scifi/lowFrequency_explosion_000.ogg"
const ARTILLERY := "res://audio/kenney_scifi/lowFrequency_explosion_001.ogg"
const MISSILE := "res://audio/kenney_scifi/thrusterFire_000.ogg"
const GUN := "res://audio/edited/ak_single.wav"
const BURST := "res://audio/edited/smg_burst.wav"
const GROUND_ENGINE := "res://audio/nayckron_engine/engine_heavy_loop_1.ogg"
const AIR_ENGINE := "res://audio/kenney_scifi/spaceEngineLarge_000.ogg"
const SHOTS := {0: CANNON, 1: ARTILLERY, 2: MISSILE, 3: CANNON,
	4: BURST, 5: MISSILE, 6: CANNON, 7: MISSILE, 8: GUN, 9: MISSILE,
	10: BURST, 11: BURST, 12: CANNON, 13: GUN, 15: GUN}

static func shot_path(unit_type: int) -> String:
	return String(SHOTS.get(unit_type, ""))

static func engine_path(unit_type: int) -> String:
	var unit: Dictionary = Catalog.units().get(unit_type, {})
	if bool(unit.get("is_aircraft", false)):
		return AIR_ENGINE
	if float(unit.get("fuel_per_km", 0.0)) > 0.0 or bool(unit.get("is_naval", false)) or unit_type == Catalog.COMMANDER_UNIT_TYPE:
		return GROUND_ENGINE
	return ""

static func cruising_speed(unit_type: int) -> float:
	return maxf(1.0, float(Catalog.units().get(unit_type, {}).get("speed", 10.0)))
