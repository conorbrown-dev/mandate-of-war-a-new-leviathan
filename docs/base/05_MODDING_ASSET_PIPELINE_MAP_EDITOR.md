# Goal 5 — Modding, Automated Asset Pipeline, and Map Editor Foundation

Read the charter and current project docs first.

## Objective

Create the tooling foundations that make the game practical to expand without hand-authoring every asset and hard-coding every unit.

## Modding Architecture

Treat modding as a first-class requirement.

Design mods to eventually add:

- units
- factions
- weapons
- projectiles
- technologies/doctrines
- maps
- balance changes
- AI behaviors
- UI extensions where safe
- visual assets
- game modes

Implement stable content IDs and a manifest/dependency system.

Evaluate a scripting strategy such as:

- Lua
- embedded scripting
- data-driven behavior definitions
- limited safe managed scripting if appropriate to the chosen engine

Balance:

- performance
- determinism
- security/safety
- accessibility
- debugging
- cross-platform behavior

Create `docs/MODDING.md`.

## Automated / Semi-Automated Asset Pipeline

Build a pipeline centered on Blender and automation where practical.

The core pipeline must not require proprietary cloud services.

Investigate/use:

- Blender Python API
- Geometry Nodes
- procedural modeling
- kitbashing
- automated UV generation where reasonable
- material generation
- faction masks
- LOD generation
- collision mesh generation
- icon/thumbnail generation
- engine import metadata

The first goal is not production art. It is a reliable automated placeholder/content pipeline.

## Historical Lineage Metadata

Allow a unit specification to describe visual lineage such as:

```yaml
id: faction_b_t1_interceptor
faction: faction_b
tier: 1
role: interceptor

visual:
  lineage:
    primary: p51_mustang
    secondary: lightweight_modern_fighter
  preserve:
    - aggressive_nose
    - canopy_profile
    - compact_fuselage
  modernization:
    - turbofan_engine
    - composite_skin
    - missile_hardpoints
    - modern_sensor_package
  faction_style: mass_warfare
```

Use lineage as a design constraint, not as an instruction to copy exact protected assets.

The generated result must remain fictional/original.

## Unit Generation Tool

Create a developer-facing tool or CLI where a designer can specify:

- faction
- tier
- role
- dimensions
- movement type
- weapons
- armor style
- historical/modern lineage references
- faction visual style
- cost/balance metadata

and generate as many of these as practical:

1. unit definition
2. procedural placeholder mesh
3. UV/material setup
4. faction material/mask
5. collision mesh
6. LODs
7. icon/thumbnail
8. engine import metadata

The generated unit should be immediately spawnable in a development build.

Create `docs/ASSET_PIPELINE.md`.

## Built-In Map Editor Foundation

Create an initial map editor capable of at least:

- terrain creation/editing
- water/sea areas
- spawn points
- resource locations
- simple props
- airbase/naval-base markers
- playable bounds
- save/load
- validation

Architect for future support of:

- roads/runways
- foliage
- strategic markers
- AI navigation hints
- scenarios/scripts
- environmental metadata
- logistics-relevant islands/staging points

Document format in `docs/MAP_FORMAT.md`.

## Tests

Add tests for:

- mod manifest parsing
- dependency resolution
- content ID collisions
- schema validation
- generated asset metadata
- map serialization/validation

## Required Review

Invoke Codex to review:

- mod safety/determinism risks
- content-versioning strategy
- asset-pipeline coupling
- map format longevity
- generated-content reproducibility

## Completion Criteria

This goal is complete when:

- at least one test mod loads
- at least one generated placeholder unit is immediately spawnable
- the basic map editor can save/load a map
- formats and extension points are documented (docs/MODDING.md, docs/ASSET_PIPELINE.md, docs/MAP_FORMAT.md)
- Codex review has been evaluated
- docs/EXECUTION_LEDGER.md, docs/CURRENT_STATE.md, docs/NEXT_TASKS.md updated

Do not begin Goal 06 until every completion criterion above is verified and the durable state/docs are updated. When continuous sequence mode is authorized in `docs/EXECUTION_LEDGER.md`, advance exactly once to Goal 06 after this gate closes; otherwise stop at the verified boundary.
