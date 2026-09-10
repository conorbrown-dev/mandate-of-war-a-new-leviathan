# Mandate of War — Processed 3D Model Integration Prompt Pack

## Goal
Integrate the processed CC0-derived `.glb` model library into the existing Godot project for **Mandate of War**.

The models are donor/prototype assets. Integrate them as replaceable visuals, not as authoritative gameplay definitions.

## Architecture Rule

```text
Stable Unit ID
    ↓
Unit Definition
    ↓
Simulation Entity
    ↓
Visual Definition / visual_id
    ↓
Godot model resource
```

Changing a GLB later must not change gameplay identity, stats, weapons, cost, faction, orders, replay identity, or simulation behavior.

## Project Constraints
- Godot primary engine
- 10,000 active units practical target
- 25,000 active units major target
- 50,000 unit stretch goal
- deterministic simulation where practical
- physical projectile combat
- stable content IDs
- modding-first content architecture
- simulation separate from rendering/presentation
- do not use heavyweight rigid-body physics for ordinary RTS units

## Recommended Order
1. `01_ASSET_AUDIT_AND_IMPORT_STRUCTURE.md`
2. `02_VISUAL_DEFINITION_LAYER.md`
3. `03_UNIT_SCENE_WRAPPERS.md`
4. `04_LOD_COLLISION_AND_PERFORMANCE.md`
5. `05_MOVEMENT_TURRETS_AND_HARDPOINTS.md`
6. `06_MAP_AND_SPAWN_INTEGRATION.md`
7. `07_FACTION_PLACEHOLDER_MAPPING.md`
8. `08_VALIDATION_AND_ASSET_VIEWER.md`
9. `09_CODEX_REVIEW.md`

## Agent Expectations
For every stage:
- inspect the repository first
- reuse existing architecture where sensible
- do not create duplicate systems if equivalents exist
- prefer data-driven definitions and lightweight structures
- do not add per-unit `_process()` loops without a strong reason
- avoid per-unit physics bodies if custom movement/collision already exists
- do not scatter raw model paths throughout gameplay code
- add tests for mappings/load behavior
- document prototype-only mappings clearly

## Expected Final State
The game can load ground, air, naval, and logistics prototype visuals through stable IDs; spawn them through normal unit definitions; use cheap collision/footprints; support turret/hardpoint hooks; validate missing assets; and replace donor models later without gameplay rewrites.
