# Mandate of War — Feature & Fix Backlog

## Purpose

This document defines the next batch of gameplay, environment, UX, and bug-fix work for **Mandate of War**.

The game is a large-scale near-future combined-arms RTS built in **Godot**, with performance as a first-class requirement. Any implementation should remain suitable for large maps and very high unit counts.

Do not solve these tasks in ways that require expensive per-frame logic on every unit if a cheaper event-driven, batched, cached, or data-oriented approach is available.

---

# General Implementation Rules

- Preserve performance at scale.
- Prefer data-driven configuration over hardcoded values.
- Avoid heavyweight rigid-body physics for ordinary RTS unit movement.
- New gameplay constants should be exposed through tunable configuration where practical.
- Systems should support faction-specific overrides later.
- Avoid tightly coupling gameplay logic to presentation/UI logic.
- Add or update automated tests where the project currently has test coverage.
- Add developer/debug visualization where it materially helps validate behavior.
- Do not silently remove existing functionality to complete these tasks.
- When a task is ambiguous, preserve existing architecture and implement the smallest extensible solution.

---

# Priority Overview

## P0 — Gameplay / Blocking Bugs

1. Vehicles must not move through structures.
2. Camera movement must follow current camera yaw.
3. Build-menu tooltips must display correctly.
4. Engineer build-range hexes must only appear when appropriate.
5. Structures must reject terrain that is unsuitable for construction.

## P1 — Core Ground Movement / Logistics

5. Realistic ground-vehicle turning and steering.
6. Constructible roads.
7. Off-road movement penalty / attrition system.

## P2 — Environment / Readability

8. Improve Fog Beacon illumination.
9. Developer map brightness/fog controls.
10. Civilian buildings on maps.
11. Better tree assets/models.

## P3 — UI Polish

12. Replace placeholder resource icons with production-quality icons.

---

# FEATURES

---

## FEATURE-001 — Improved Tree Models

### Goal

Replace or improve the current tree assets so terrain looks convincing at strategic and tactical zoom levels without harming large-map performance.

### Requirements

- Replace visibly low-quality placeholder trees.
- Trees should fit the near-future realistic military art direction.
- Assets should remain readable from:
  - close tactical zoom,
  - normal gameplay zoom,
  - high strategic zoom.
- Create or use appropriate LODs.
- Tree collision should remain lightweight.
- Avoid individual heavyweight physics bodies if unnecessary.
- Tree assets should be reusable across biome definitions.
- Support biome variation where the environment system already allows it.

### Preferred Asset Pipeline

Use the existing project asset philosophy:

```text
source asset / Blender
        ↓
optimized mesh
        ↓
LODs
        ↓
materials/textures
        ↓
collision representation
        ↓
.glb
        ↓
Godot import
```

### Performance Notes

The game may display thousands of trees.

Prefer:

- MultiMesh / instancing where compatible with current terrain architecture.
- Shared materials.
- Texture atlases where useful.
- Simplified collision.
- Aggressive LOD transitions.
- Billboard/impostor fallback at extreme distance if needed.

Do not create a unique node-heavy scene for every tree unless benchmarking demonstrates it is acceptable.

### Acceptance Criteria

- Existing maps use visibly improved tree assets.
- Trees no longer look obviously placeholder-quality.
- Distant trees use lower-cost representation.
- No obvious frame-time regression from equivalent tree density.
- Collision behavior remains correct for units/projectiles if trees currently participate in collision.
- Tree rendering works correctly through strategic zoom.

### Implementation status — current presentation scope complete (2026-09-11)

Implemented with a shared low-poly tree mesh containing a trunk and three
layered canopy surfaces for tactical view. Strategic zoom uses a reduced cone
mesh with half the instance density. Both paths remain MultiMesh-batched and
the Godot harness verifies the transition at the 5 km camera threshold.
Release build, CTest 3/3, direct integration runner 162/162, and Godot
presentation harness 85/85 pass. Production-quality imported tree assets and
biome-specific art remain future polish.

---

## FEATURE-002 — Realistic Ground Vehicle Turning and Steering

### Goal

Ground vehicles should move like wheeled/tracked vehicles rather than characters or hovering objects.

Vehicles must orient toward their intended path before moving effectively and must not rotate freely in place unless the vehicle type explicitly supports pivot steering.

### Current Problem

Ground vehicles can currently rotate unrealistically, including spinning around their center axis rather than maneuvering into alignment with their desired travel direction.

### Desired Behavior

When ordered to move:

1. The unit receives or already has a path.
2. The vehicle determines the desired heading toward the next path segment.
3. The vehicle changes heading at a limited turn rate.
4. Forward speed is reduced when heading error is large.
5. Once sufficiently aligned, the vehicle accelerates toward normal speed.
6. The vehicle continues steering along the path rather than snapping to each segment.

### Vehicle Categories

Support movement parameters per unit definition.

Suggested parameters:

```text
max_forward_speed
max_reverse_speed
acceleration
deceleration
turn_rate
turn_rate_at_speed
minimum_turn_radius
can_pivot_turn
reverse_preference_threshold
steering_response
```

Tracked vehicles may have much tighter turning behavior than wheeled vehicles.

Examples:

- Tank:
  - can potentially pivot or near-pivot,
  - lower maximum speed,
  - high low-speed turn rate.

- Truck:
  - cannot pivot,
  - requires forward/reverse maneuvering,
  - larger turning radius.

- Light scout:
  - fast,
  - responsive steering,
  - moderate turning radius.

### Important Constraints

Do **not** implement this as full rigid-body vehicle simulation.

Use deterministic/custom movement math appropriate for RTS scale.

Movement should remain compatible with:

- pathfinding,
- formations,
- local avoidance,
- multiplayer determinism strategy,
- replay system,
- large unit counts.

### Edge Cases

Handle:

- destination directly behind vehicle,
- narrow corridors,
- obstacle avoidance,
- formation movement,
- reversing when preferable,
- vehicles beginning very close to destination,
- path recalculation while turning,
- temporary blockage by other units.

### Debugging

Add optional developer visualization for:

- current heading,
- desired heading,
- steering target,
- current path segment,
- turn radius if useful.

### Acceptance Criteria

- Wheeled vehicles no longer spin freely on their center axis.
- Vehicles visibly maneuver toward their travel heading.
- Vehicles slow appropriately during sharp turns.
- Vehicles can reach destinations behind their initial facing direction.
- Tanks/tracked vehicles may retain tighter turning than trucks.
- No obvious oscillation around path nodes.
- Behavior remains performant with large groups of vehicles.

---

## FEATURE-003 — Constructible Roads

### Goal

Allow players to construct roads that improve ground logistics and movement.

Roads should become strategically valuable infrastructure rather than purely cosmetic terrain decoration.

### Initial Scope

Players should be able to enter a road-construction mode and define a road between points.

Possible interaction:

1. Select an engineer/construction unit.
2. Select **Build Road**.
3. Click or drag a road path.
4. Show a placement preview.
5. Confirm placement.
6. Engineers construct road segments.
7. Completed road segments modify movement behavior.

### Road Representation

Prefer a lightweight road network rather than thousands of expensive independent objects.

The implementation should support:

- path segments,
- road width,
- material cost,
- construction time,
- ownership if relevant,
- traversal speed modifier,
- terrain adaptation,
- connectivity between segments.

### Road Effects

Roads should provide benefits such as:

- increased movement speed,
- reduced fuel/energy use if such systems apply,
- reduced off-road attrition,
- increased logistics throughput later,
- more predictable large-force movement.

Exact balance values should be configurable.

### Placement Constraints

Consider:

- water,
- cliffs,
- blocked terrain,
- structures,
- bridges,
- extreme slopes,
- map boundaries.

The first implementation does not need a full highway-engineering simulation.

### Pathfinding Integration

Roads should influence route selection.

Possible strategies include:

- lower traversal cost on road-covered navigation cells,
- road graph layered over terrain navigation,
- cached road-preference weighting.

Avoid rebuilding the entire navigation world for every single road segment if possible.

Batch or defer navigation updates.

### Visual Requirements

Roads should:

- conform to terrain,
- remain visible from standard gameplay zoom,
- remain distinguishable at strategic zoom where practical,
- avoid obvious z-fighting.

### Future-Proofing

Design so roads can later support:

- bridges,
- damaged roads,
- repair,
- supply throughput,
- faction-specific road construction,
- paved vs temporary roads,
- strategic-map infrastructure.

### Acceptance Criteria

- Player can place and construct roads.
- Invalid road placement is clearly indicated.
- Vehicles receive a measurable traversal benefit on roads.
- Pathfinding can prefer road routes when the total route is advantageous.
- Roads persist for the duration of the battle.
- Road construction cost/time are data-driven.
- Building roads does not cause major navigation stalls.

---

## FEATURE-004 — Off-Road Attrition / Logistics Penalty

### Goal

Ground forces traveling away from roads should incur enough operational cost that players are incentivized to build and control road networks.

This system should reinforce logistics gameplay without making all off-road maneuvering nonviable.

### Design Intent

Roads should be the efficient option for:

- long-distance movement,
- reinforcement routes,
- heavy logistics traffic,
- sustained offensives.

Off-road travel should remain valuable for:

- flanking,
- surprise attacks,
- short tactical repositioning,
- reconnaissance,
- emergency retreats.

### Recommended Model

Avoid literal random HP damage as the only penalty unless that is already part of the game's logistics design.

Prefer configurable operational penalties such as one or more of:

- increased Material consumption,
- increased Energy/fuel consumption,
- reduced movement speed,
- increased maintenance accumulation,
- temporary readiness degradation,
- mechanical attrition,
- increased chance of mobility impairment at extreme accumulated wear.

### Suggested First-Pass System

Track an `off_road_wear` or equivalent lightweight value for applicable units.

While moving off-road:

```text
wear += distance_traveled * terrain_wear_factor * unit_wear_factor
```

On-road:

```text
wear accumulation ≈ zero or strongly reduced
```

Wear may later translate into:

- resource drain,
- movement penalty,
- maintenance requirement,
- damage only at severe thresholds.

The exact implementation should align with any existing logistics/fuel systems.

### Terrain Factors

Allow terrain-specific modifiers later:

- grass/plains: low,
- sand: moderate,
- mud: high,
- snow: high,
- rocky terrain: very high.

### Unit Factors

Different unit classes should be affected differently.

Examples:

- tracked armor: moderate off-road resilience,
- wheeled logistics truck: high road dependency,
- infantry: low road dependency,
- specialized all-terrain vehicle: reduced penalty.

### UI

Players need understandable feedback.

At minimum:

- unit tooltip/status should expose meaningful attrition state if it affects gameplay,
- route planning should not hide the penalty entirely,
- roads should communicate their benefit.

### Performance

Do not calculate expensive terrain queries every frame for every moving unit.

Prefer:

- distance-based updates,
- low-frequency batched updates,
- terrain-cell metadata already available to movement/pathfinding,
- cached road/off-road state.

### Acceptance Criteria

- Long off-road travel has a noticeable operational cost.
- Road travel has clearly lower attrition/cost.
- Short tactical off-road movement remains practical.
- Different vehicle classes can use different penalty multipliers.
- Penalty values are data-driven.
- System remains performant for thousands of moving units.

### Implementation status — initial scope complete (2026-09-11)

Implemented with a lightweight `OffRoadWear` component and data-authored
values in `data/off_road.json`. Ground movement accumulates distance-based
wear, drains Material and Energy, and applies a bounded speed penalty. Road
cells use a strongly reduced wear multiplier. Unit-class factors are
configurable, and selected-unit HUD telemetry exposes surface, wear, and speed
multiplier. Release build, CTest 3/3, direct integration runner 161/161, and
Godot presentation harness 79/79 pass. Terrain-biome factors and explicit
maintenance/recovery remain future extensions.

---

## FEATURE-005 — Developer Map Brightness and Fog Controls

### Goal

Add a developer-only hotkey panel for quickly tuning map brightness and fog presentation during development.

### Requirements

Add a developer/debug UI opened through a keyboard shortcut.

Suggested default:

```text
F8 — Environment Debug Panel
```

Use another existing debug key if F8 conflicts with current controls.

### Controls

At minimum provide sliders for:

- Map / environment brightness
- Fog density / fog amount

Useful optional controls if easy to expose:

- fog color,
- fog start distance,
- fog end distance,
- ambient light,
- directional light energy,
- exposure.

### Behavior

- Changes should update live.
- Display current numerical value.
- Provide **Reset to Defaults**.
- Developer controls must not unintentionally persist into production saves.
- Panel should only be accessible in development/debug builds unless an existing developer-mode system exists.

### Architecture

The UI should adjust the authoritative environment settings rather than duplicating rendering logic.

### Acceptance Criteria

- Developer hotkey opens and closes the panel.
- Brightness can be adjusted live.
- Fog amount can be adjusted live.
- Reset restores map/default environment values.
- Controls do not appear during normal player UI flow.

---

## FEATURE-006 — Civilian Buildings / World Dressing

### Goal

Scatter civilian structures throughout suitable areas of maps to make battlefields feel inhabited and strategically believable.

### Examples

Possible civilian structures:

- houses,
- apartment blocks,
- warehouses,
- farms,
- gas stations,
- small industrial buildings,
- offices,
- municipal buildings,
- utility structures.

### Map Placement

Civilian buildings should be placed according to believable clusters rather than uniform random scattering.

Prefer patterns such as:

- villages,
- suburbs,
- industrial zones,
- roadside businesses,
- farms,
- small towns.

### Gameplay

Initial implementation may treat these as neutral map structures.

They should:

- block vehicle movement,
- participate in collision,
- not belong to a combat faction unless intended,
- be targetable/damageable only if the current destruction system supports it.

### Performance

Maps may contain many buildings.

Prefer:

- LODs,
- shared materials,
- instancing where feasible,
- simplified collision,
- cheap destroyed-state handling.

### Future-Proofing

Architecture should allow later additions such as:

- destructible civilian infrastructure,
- occupation/capture,
- collateral damage mechanics,
- strategic resources,
- roads connecting settlements,
- civilian population simulation if ever desired.

Do not implement those future systems unless already in scope.

### Acceptance Criteria

- Maps contain believable civilian structure clusters.
- Structures properly block vehicle movement.
- Placement does not interfere with critical spawn zones unless intentionally authored.
- Buildings remain performant at strategic zoom.
- Assets visually match the near-future setting.

### Implementation status — initial scope complete (2026-09-11)

Implemented as deterministic neutral world dressing. Cluster data is authored
in `godot/project/scenarios/civilian_dressing.json`; house, warehouse, and
utility groups render through three `MultiMeshInstance3D` batches. Candidates
are restricted to land with bounded local relief, protected corridors keep
critical starts/resources clear, and each accepted footprint blocks localized
native ground navigation. Release build, CTest 3/3, direct integration runner
162/162, and Godot presentation harness 82/82 pass. Destruction, occupation,
and civilian simulation remain out of scope.

---

## FEATURE-007 — Production-Quality Resource Icons

### Goal

Replace placeholder resource icons with clear, polished, scalable UI icons suitable for a commercial game interface.

### Required Icons

#### Material

- Color identity: **blue**
- Symbol: **wrench**
- Meaning: manufacturing feedstock, ammunition, construction, physical logistics.

#### Energy

- Color identity: **orange**
- Symbol: **lightning bolt**
- Meaning: electrical power, fuel, reactor output, power systems.

### Visual Direction

Icons should feel like high-quality web/SaaS/game UI iconography:

- clean geometry,
- strong silhouette,
- readable at small sizes,
- consistent stroke/shape language,
- no amateur clip-art appearance,
- visually coherent with the rest of the HUD.

Prefer vector source assets where possible.

### Asset Requirements

Provide appropriate sizes/import settings for:

- compact HUD display,
- build menu,
- tooltip,
- resource summary,
- high-DPI display.

Use SVG where Godot integration permits, otherwise generate clean PNG variants from a vector master.

### Future Resource

The icon style should be designed so a third icon can later be added for:

- Research

without redesigning the entire resource visual language.

### Acceptance Criteria

- Material uses a blue wrench icon.
- Energy uses an orange lightning-bolt icon.
- Icons remain readable at small HUD sizes.
- Icons share a coherent visual style.
- Existing UI is updated to use the new icons.
- No blurry scaling or low-resolution artifacts are visible.

### Implementation status — current resource-HUD scope complete (2026-09-11)

Implemented as shared vector-drawn HUD geometry rather than bitmap assets.
Material uses a blue wrench, Energy uses an orange lightning bolt, and a
matching green research sigil establishes the future third-resource slot.
The economy strip and compact build cards use the same scalable geometry.
Godot presentation harness 86/86, Release build, CTest 3/3, and direct
integration runner 162/162 pass. Further icon changes are desktop
art-direction polish only.

---

# FEATURE-008 — Realistic Structure Terrain Placement

### Goal

Prevent military structures from being constructed on terrain that could not
support their footprint in reality. A construction preview must communicate
terrain suitability before the player commits the order, and the authoritative
simulation must enforce the same rule.

### Placement Validation

Validate the complete structure footprint against the terrain heightfield and
map rules. At minimum, a location is invalid when:

- the footprint overlaps water, a map boundary, or another blocked structure;
- the maximum local slope/pitch exceeds the structure's construction limit;
- the height variation across the footprint exceeds its levelness tolerance;
- the footprint contains a ravine, cliff, abrupt drop, or other impassable
  terrain discontinuity;
- required clearance for the structure and its construction approach is absent.

Validation should sample the footprint rather than only the cursor's center
point. Structure definitions should own their footprint dimensions and
terrain tolerances so larger or more specialized structures can be stricter
later without hardcoded UI exceptions.

### Player Feedback

- A valid build ghost remains in the normal placement color.
- An invalid build ghost is red and cannot be committed.
- The preview should update as the cursor moves, including when only the
  footprint's edge crosses a slope, water cell, or blocked area.
- Where practical, expose a concise invalid-reason tooltip or developer
  diagnostic such as `TOO STEEP`, `UNEVEN GROUND`, `WATER`, or `BLOCKED`.

The red preview is presentation feedback only; it must not be the sole source
of truth. The authoritative build/production command must repeat the same
terrain validation to prevent invalid placement through alternate input paths,
scripts, or network commands.

### Performance Constraints

Use the existing heightfield/navigation data and bounded footprint sampling.
Do not add per-frame physics bodies or full terrain collision scans for every
unit. Cache static terrain samples where useful, and run authoritative
validation once when a build order is submitted in addition to the lightweight
preview checks.

### Acceptance Criteria

- Structures cannot be queued or completed on water, beyond the map, on
  blocked terrain, or on footprints exceeding their configured slope or
  levelness limits.
- The build ghost is visibly red and placement is disabled for every invalid
  case above.
- A valid flat/low-slope location still previews and builds normally.
- Validation covers the entire structure footprint, not only its center.
- Preview and authoritative command validation agree for the same terrain
  sample.
- Focused automated tests cover flat valid ground, excessive slope, excessive
  height variation, water, footprint overlap, and map-edge rejection.
- Validation remains bounded and suitable for large maps and many concurrent
  build previews.

---

# FIXES

---

## FIX-001 — Vehicles Must Not Move Through Structures

### Severity

**P0 / Core gameplay bug**

### Problem

Ground vehicles can currently move through buildings or other structures.

### Expected Behavior

Structures with blocking collision should be treated as impassable by applicable ground units.

Vehicles should:

- path around structures,
- stop if their route becomes blocked,
- request/reuse an alternate route where appropriate,
- never visually clip through a structure simply to reach a movement target.

### Investigation Areas

Inspect:

- structure collision layers/masks,
- ground-unit collision layers/masks,
- navigation obstacle integration,
- pathfinding occupancy data,
- local avoidance,
- movement code that may ignore collision/path constraints.

### Important Constraint

Do not fix this using expensive full physics simulation for every vehicle.

The pathfinding/navigation system should remain the primary mechanism preventing invalid movement.

Local collision/avoidance may be used as a safety layer.

### Dynamic Structures

Newly constructed structures must update the relevant blocking/navigation representation.

Navigation updates should be batched/deferred where appropriate.

### Acceptance Criteria

- Vehicles cannot pass through existing structures.
- Vehicles cannot pass through newly constructed structures.
- Units route around blocked structures when a valid route exists.
- Units stop/repath cleanly when no route exists.
- No severe pathfinding stall is introduced when structures are created.
- Large groups do not continuously push through collision boundaries.

### Implementation status — current navigation scope complete (2026-09-11)

Completed military structures now block their authored rectangular footprint
through `Pathfinding::block_world_rectangle`, so ground routes detour around
the structure rather than crossing its center-only cell. The existing
flow-field escape behavior remains available to engineers already occupying a
cell when construction completes. Release build, CTest 3/3, direct integration
runner 163/163, and Godot presentation harness 86/86 pass. Destruction and
physics collision remain outside this slice.

---

## FIX-002 — Fog Beacons Need Stronger Illumination

### Problem

Fog Beacons currently emit too little visible light to be useful.

### Goal

Fog Beacons should create a clearly readable local illuminated area in foggy/dark environments.

### Requirements

Increase the effective beacon illumination through the most appropriate combination of:

- light energy/intensity,
- range,
- attenuation,
- volumetric contribution,
- emissive material strength,
- fog interaction.

Do not simply overexpose the beacon mesh itself while leaving the environment dark.

### Visual Target

A player should be able to immediately identify:

- where the beacon is,
- the area it is illuminating,
- nearby units/terrain within its intended radius.

### Performance

Fog Beacons may exist in significant numbers.

Avoid extremely expensive shadowed lights if they create unacceptable scaling cost.

Consider:

- unshadowed lights,
- limited light count/range,
- shader-based or environment-assisted glow,
- clustered/deferred lighting behavior in the current renderer.

### Acceptance Criteria

- Beacon illumination is clearly visible under the intended fog settings.
- Nearby terrain and units become meaningfully easier to see.
- The beacon itself does not appear absurdly overexposed.
- Multiple beacons do not cause a major performance collapse.

### Implementation status — current lighting scope complete (2026-09-11)

Completed Fog Beacons now provide a 1,200 m localized light at 12 energy
intensity and publish bounded world-space light records to the custom terrain
shader. This lets nearby terrain respond instead of relying solely on model
lighting. The Godot presentation harness verifies the light and shader wiring
at 87/87; Release build, CTest 3/3, and direct integration runner 163/163
remain green. Further balance work is desktop GPU art direction.

---

## FIX-003 — Camera Movement Must Follow Current Yaw

### Severity

**P0 / Controls**

### Problem

After changing camera yaw, movement controls continue moving the camera relative to its original orientation.

Example:

1. Camera initially faces north.
2. Player rotates camera 90°.
3. Pressing "forward" still moves toward original north rather than the camera's new forward direction.

### Expected Behavior

Camera translation should be calculated relative to the camera's current yaw orientation.

For a typical RTS camera:

```text
forward_input → camera horizontal forward vector
right_input   → camera horizontal right vector
```

Vertical pitch should not cause forward input to move the camera into the ground or sky.

### Suggested Calculation

Use the camera's basis vectors projected onto the XZ/world-ground plane.

Conceptually:

```text
forward = -camera.global_transform.basis.z
forward.y = 0
forward = forward.normalized()

right = camera.global_transform.basis.x
right.y = 0
right = right.normalized()

movement =
    forward * input_forward +
    right   * input_right
```

Adapt to the project's coordinate conventions.

### Acceptance Criteria

- Forward movement always travels toward the direction currently shown at the top/forward side of the camera view.
- Backward movement is the inverse.
- Left/right movement remain perpendicular to current yaw.
- Behavior works at all supported yaw angles.
- Camera pitch does not introduce unwanted vertical translation.
- Zoom behavior remains unchanged.

### Implementation status — complete (2026-09-11)

Q/E rotate the tactical camera yaw, and camera-relative translation projects
forward/right movement onto the ground plane using the current yaw. The
presentation harness verifies yaw direction and forward-vector behavior while
preserving terrain-aware zoom.

---

## FIX-004 — Engineer Build Range Hexes Should Only Show When Selected

### Problem

The engineer vehicle displays its build-range hex visualization when it should not.

### Expected Behavior

The build-range visualization should be hidden by default.

Show it only when one of the following applies:

- the engineer is selected, and the current design intentionally shows range on selection;
- the player is actively using a build-placement mode that requires the range visualization.

Do not display the range for:

- unselected engineers,
- enemy engineers,
- unrelated selected units,
- units outside the local player's visibility rules.

### Multi-Select

If multiple engineers are selected, use the existing selection/range-display UX convention.

Avoid creating an unreadable field of overlapping range graphics if the current design supports a cleaner aggregate approach.

### Architecture

Range rendering should subscribe to selection/build-mode state rather than polling every frame if possible.

### Acceptance Criteria

- Unselected engineer: no range hexes.
- Selected engineer: range appears when intended.
- Deselected engineer: range immediately disappears.
- Build placement still receives the required range feedback.
- Enemy engineers never expose build range unintentionally.

### Implementation status — superseded by product direction (2026-09-11)

The original acceptance intent to hide all unselected engineer range markers
is superseded by the explicit product decision that the battlefield should
always show blue visibility, purple radar, and red attack-range hexes. The
current presentation harness verifies those three unselected-unit markers;
engineer-specific selection behavior should not remove the shared tactical
range language.

---

## FIX-005 — Build Menu Tooltips Do Not Display

### Severity

**P0 / UX**

### Problem

Hovering over items in the build menu does not display tooltips.

### Expected Behavior

Hovering over a build-menu item should display a tooltip after the normal UI hover delay.

### Tooltip Content

Use existing data where available.

At minimum, a build-item tooltip should be capable of showing:

- unit/building name,
- short description,
- Material cost,
- Energy cost,
- Research requirement/cost if applicable,
- build time,
- key role/category.

Do not invent missing gameplay values solely for the tooltip.

### Investigation Areas

Check:

- mouse filtering,
- Control node hierarchy,
- tooltip text assignment,
- custom tooltip callbacks,
- z-order,
- clipping,
- parent containers intercepting mouse events,
- disabled controls,
- theme overrides,
- custom build-menu item scenes.

### Godot-Specific Checks

Verify whether the current UI expects either:

```gdscript
tooltip_text = "..."
```

or a custom tooltip implementation such as:

```gdscript
_make_custom_tooltip(...)
```

Ensure the hover target is actually receiving mouse events.

### Acceptance Criteria

- Hovering any enabled build-menu item displays a tooltip.
- Tooltip content corresponds to the hovered item.
- Tooltip is not clipped behind the build panel.
- Tooltip disappears normally when hover ends.
- Disabled/locked items can still explain why they are unavailable if the existing UI supports it.
- Tooltip behavior works with all current build categories.

### Implementation status — current build-deck scope complete (2026-09-11)

The compact custom tooltip path now preserves authored item name, cost,
readiness, and build-time details, renders above the deck, and waits 240 ms
before appearing to avoid flicker during cursor travel. Godot presentation
harness 87/87, Release build, CTest 3/3, and direct integration runner 163/163
pass.

---

# Suggested Implementation Order

Codex/MIRIAM should preferably implement this backlog in the following sequence:

```text
1. FIX-001 — Structure collision / navigation
2. FEATURE-002 — Ground vehicle steering
3. FIX-003 — Camera-relative movement
4. FIX-004 — Engineer range visualization
5. FIX-005 — Build-menu tooltips
6. FIX-002 — Fog Beacon lighting
7. FEATURE-005 — Environment debug controls
8. FEATURE-003 — Constructible roads
9. FEATURE-004 — Off-road attrition
10. FEATURE-006 — Civilian buildings
11. FEATURE-001 — Improved trees
12. FEATURE-007 — Resource icons
13. FEATURE-008 — Realistic structure terrain placement
```

### Why This Order?

Structure collision and realistic vehicle movement affect the same broad ground-navigation domain and should stabilize before roads alter navigation costs.

Roads should exist before off-road attrition is finalized so both systems can be balanced together.

Environment tooling should be available before large-scale lighting/environment polish.

Asset replacement and icon polish can proceed independently once core gameplay bugs are stable.

---

# Recommended Work Method for MIRIAM / Codex

For each item:

1. Inspect the existing implementation before changing code.
2. Identify the authoritative system responsible for the behavior.
3. Summarize the proposed implementation briefly.
4. Implement the smallest extensible solution.
5. Preserve existing public APIs unless there is a strong architectural reason not to.
6. Add configuration rather than hardcoding gameplay balance values.
7. Add debug visualization when useful.
8. Run existing automated tests.
9. Add focused tests where practical.
10. Launch or run the relevant scene/test environment where tooling allows it.
11. Report:
    - files changed,
    - architecture decisions,
    - configurable values added,
    - tests performed,
    - unresolved risks,
    - any follow-up work discovered.

---

# Definition of Done

A backlog item is not complete merely because the code compiles.

Each task should satisfy all of the following where applicable:

- Behavior matches the acceptance criteria.
- No obvious regression to existing controls/gameplay.
- Works with multiple units, not only one test unit.
- Handles creation/destruction of runtime objects where relevant.
- Uses scalable logic suitable for RTS unit counts.
- New constants are configurable where appropriate.
- Debug-only UI does not leak into normal gameplay.
- UI changes work at supported resolutions.
- Relevant tests pass.
- Implementation is documented sufficiently for the next agent/developer to understand.

---

# Out of Scope for This Backlog

Unless required by one of the tasks above, do not use this work batch to redesign:

- the entire pathfinding architecture,
- the strategic campaign layer,
- the full logistics economy,
- the multiplayer model,
- the replay format,
- faction balance,
- the complete terrain pipeline,
- civilian simulation,
- full destruction physics.

If a task reveals that one of these systems must change, document the dependency and implement only the minimum necessary supporting change.
