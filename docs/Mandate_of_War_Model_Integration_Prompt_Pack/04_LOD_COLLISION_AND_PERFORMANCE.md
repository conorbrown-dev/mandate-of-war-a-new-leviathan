# Goal 4 — LOD, Collision, Culling, and Rendering Performance

Audit and implement a visual strategy appropriate for 10k–25k active units.

## Collision
Do not use render-mesh triangle collision for ordinary units.

Prefer the existing custom simulation system. Otherwise use lightweight representations such as:
- radius/circle
- rectangle/footprint
- capsule
- box
- simple height/clearance values

Avoid giving every unit a heavyweight physics body just because the asset is 3D.

## LOD
Inspect processed GLBs for names like:

```text
Hull
Hull_LOD1
Hull_LOD2
```

Ensure all LODs are not visible simultaneously.

Evaluate the least expensive suitable mechanism:
- Godot visibility ranges
- mesh LOD
- wrapper-managed tiers
- whole-unit simplified scenes
- strategic-zoom icon/impostor replacement

The current Blender prototype may have produced per-child LOD meshes. Document whether production assets should instead use:

```text
Unit_LOD0
Unit_LOD1
Unit_LOD2
```

while preserving only actually moving parts such as turret/gun/radar.

## Materials
Avoid unique material duplication per instance. Prefer shared materials/shaders and faction/team parameters.

## Strategic Zoom
Prepare hooks to reduce detail, animation, shadows, and possibly replace meshes with strategic icons at extreme zoom.

## Benchmark
Create or extend a developer benchmark for:
- 100 units
- 1,000 units
- 10,000 visual instances

Measure frame time, visible object count/draw calls where accessible, transform update cost, and memory where practical.

If the current approach fails before 10k, report the bottleneck explicitly.
