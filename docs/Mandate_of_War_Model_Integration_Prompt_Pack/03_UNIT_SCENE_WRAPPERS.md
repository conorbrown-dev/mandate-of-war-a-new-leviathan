# Goal 3 — Lightweight Unit Visual Wrappers

Integrate GLB resources through a reusable Godot visual wrapper instead of attaching gameplay scripts directly to imported scenes.

Conceptual structure:

```text
UnitVisualRoot
  ├─ ModelRoot
  ├─ OptionalTurretRoot
  ├─ OptionalHardpointMarkers
  ├─ SelectionVisual
  └─ DebugVisuals
```

Use the existing project architecture if it already provides an equivalent.

## Performance
Prefer a generic visual controller plus data over a unique script-heavy scene for every unit.

Avoid:
- per-unit timers
- unnecessary `_process()` callbacks
- deep imported Node trees
- gameplay physics in visual scenes
- runtime mesh joining/decimation/UV work

Runtime correction may include cheap scale/rotation/offset/material overrides only.

## Prototype Coverage
Integrate at least one usable prototype for each available category:
- MBT
- recon/light vehicle
- logistics truck
- fighter
- naval combatant
- carrier if available

Do not fabricate missing assets.

## Selection
Selection/footprint logic must not depend on triangle-mesh collision. Use gameplay footprint data or lightweight presentation metadata.

## Tests
Verify correct model loading, simulation transform driving visual transform, safe missing-model behavior, and replaceability of the GLB without changing gameplay definition.
