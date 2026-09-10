# Goal 6 — Integrate Models into Normal Unit Spawning

Connect the visual layer to the existing normal unit/content spawning path.

Expected conceptual flow:

```text
Unit ID
  ↓
UnitDefinition
  ↓
Simulation entity
  ↓
visual_id
  ↓
VisualDefinition
  ↓
Cached model/wrapper instance
```

## Requirements
Identify the existing unit definition/content architecture and add `visual_id` or the closest equivalent.

Create clearly marked prototype/dev unit definitions where needed for:
- MBT
- recon/light vehicle
- logistics truck
- fighter
- naval combatant
- carrier

Use stable fictional/prototype game IDs, not donor filenames.

Verify normal factories/debug spawners/scenario systems can spawn them without special asset-path cases.

## Forward Seizure Feature
If that system is present, allow appropriate prototypes to fill temporary roles such as recon, armor, logistics, and air cover. Do not create coupling if the feature is not yet implemented.

## Destruction
Reuse existing simulation destruction events. Prototype visuals may simply hide/swap to generic wreck behavior until dedicated wreck models exist.

## Replay / Multiplayer
Replay/network identity must remain based on unit/content IDs, not GLB filesystem paths.

## Tests
Cover normal spawn by unit ID, visual resolution, missing-visual fallback, factory/debug spawn, and replay serialization staying unit-ID driven.
