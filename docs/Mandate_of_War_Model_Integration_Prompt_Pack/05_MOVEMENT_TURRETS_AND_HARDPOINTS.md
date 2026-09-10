# Goal 5 — Orientation, Turrets, Hardpoints, Aircraft, and Naval Hooks

Connect prototype visuals to movement and weapons without moving authoritative simulation logic into the model scene.

## Convention
Validate and standardize project orientation. Prefer:

```text
Godot forward: -Z
Godot up: +Y
```

Store per-model corrections in visual definitions rather than scattered scripts.

## Ground Vehicles
Support presentation hooks for:
- hull heading
- independent turret yaw
- gun elevation where available
- muzzle position
- optional presentation-only track/wheel animation

No wheel physics required.

## Stable Hardpoints
Support IDs such as:
- turret_root
- gun_root
- muzzle
- weapon_primary
- weapon_secondary
- missile_01
- missile_02
- sensor
- exhaust
- aircraft_weapon_left/right
- aircraft_engine
- naval_turret_01/02

Do not globally depend on donor-model object names. Normalize them through metadata/mappings.

## Missing Markers
Donor GLBs may not contain suitable marker nodes. Allow metadata-defined position/forward offsets so prototypes remain usable until Blender art cleanup.

## Aircraft
Presentation hooks may include heading/pitch/roll, banking, engine effects, weapon release points, and later landing gear states. Fuel/endurance remains simulation logic elsewhere.

## Naval
Support hull heading, turret yaw, gun elevation, missile launch points, wake/exhaust effect anchors, and future carrier deck reference points. Do not add rigid-body buoyancy for RTS vessels without a project-level need.

## Physical Projectiles
Use muzzle/hardpoint transforms to initialize projectile presentation/position while firing authorization and projectile simulation remain authoritative systems.

## Tests
Cover hull/turret separation, hardpoint resolution/fallbacks, projectile spawn location, aircraft presentation banking not altering authoritative heading, and naval turret rotation not affecting pathing.
