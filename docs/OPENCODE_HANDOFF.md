## Documentation Files

| File | Status |
|---|---|
| `00_PROJECT_CHARTER.md` | ✅ Read |
| `03_ECONOMY_COMBAT_FACTIONS.md` | ✅ Read |
| `02_SIMULATION_AND_SCALE.md` | ✅ Read |
| `03_RENDERING_AND_CONTROLS.md` | ✅ Read |
| `docs/ARCHITECTURE.md` | ✅ Read |
| `docs/CURRENT_STATE.md` | ✅ Updated 2026-09-10 |
| `docs/NEXT_TASKS.md` | ✅ Updated 2026-09-10 |
| `docs/EXECUTION_LEDGER.md` | ✅ Updated 2026-09-10 |
| `EXTENSION_STATUS.md` | ✅ Read |
| `docs/OPENCODE_HANDOFF.md` | ✅ Updated 2026-09-10 |

## Current State

**Active goal:** Goal 05 modding, asset pipeline, and map-editor acceptance. Goals 06–11 are gated. Earlier Goal 11 and Goal 12 wording below is historical implementation context, not current acceptance routing.

**Reconciliation:** `main` is `1846132`; the prior `9c295bf` baseline is absent. A substantial dirty-tree implementation package is preserved, and `stash@{0}` remains unapplied user work. The model-integration branch has no commits absent from `main`, while its registered external worktree is unavailable. See `docs/WORKTREE_RECONCILIATION.md`.

**Current boundary:** Goal 04 is verified: the full behavior suite, focused review, Release build, CTest 3/3, direct integration 167/167, and logistics benchmark all passed. Goal 05 is active; begin with `05_MODDING_ASSET_PIPELINE_MAP_EDITOR.md` and do not advance until its end-to-end mod/map acceptance proof and review are complete.

**Goal 05 progress:** The generated-unit development path is now proven: `generate.py` emits linked unit/mesh metadata without nulling defaults; `ModManager` validates a manifest-declared generated unit; and the integration suite spawns it with authored data. Dependency ordering/version constraints and map save/load are the next unverified acceptance slices.

**Goal 05 map progress:** `MapLoader::save_map()` now round-trips map terrain,
resources, spawn points, and initial entities through the YAML/binary map
format. The direct integration suite has 168 passing assertions. Dependency
ordering/version constraints and the editor surface remain open.

**Last verified:** 2026-09-12

**Validation handoff:** Start with `docs/validation/VALIDATION_BASELINE.md`, `RUNNING_TESTS.md`, and `SCENARIO_CATALOG.md`. `tools/validate all` runs the four registered scenarios; `tools/test` runs the broader build/native/Godot/validation sweep. Generated evidence is under gitignored `validation/artifacts/`. Strategic-zoom baselines are committed under `validation/baselines/` at an empirically established 0.995 threshold. Do not update them implicitly.

**Key changes in this session:**
- Implemented full territorial control domain model (`src/ecs/components/territorial_control.{hpp,cpp}`)
- Integrated `TerritorialControlManager` into `Simulation` class (`src/simulation/simulation.hpp:225`)
- Updated documentation: `CURRENT_STATE.md`, `NEXT_TASKS.md`, `EXECUTION_LEDGER.md`, `OPENCODE_HANDOFF.md`
- Wired content-authored unit territorial capabilities from all current unit prototypes, with a compatibility mapping for older content
- Added deterministic FOB construction progress and inactive-before-completion bonus behavior
- Added authoritative FOB install command validation and GDExtension installation telemetry
- Added `8`-then-left-click FOB placement and telemetry-backed HUD progress card
- Exposed all six native Elite prototypes in the Command Walker build menu;
  fighter, VTOL, and patrol boat are reachable with `9`, `0`, and `P` and the
  presentation harness verifies an end-to-end fighter build.
- Corrected BUILD validation to accept the nonzero player-selected
  rally/output target carried by the production command.
- Removed the invalid `TerritoryManager` `Node` child that attached the
  `Node3D` `main.gd` script and caused scene-instantiation errors. All 15
  reference models load; the fighter completion test requires an imported
  model rather than a fallback wrapper.
- The playable scenario now starts with visible Industrial engineering units;
  hidden faction bases retain economy/production authority. The player engineer
  is selected at start and can immediately place an FOB with `8` then click.
- Reference-model units now follow the heightmap during registration and
  movement, and their visible selection rings are driven by the same selection
  state as the gameplay HUD. The presentation harness checks both engineer
  terrain placement and its selection marker.
- Build and tests pass: Release build, CTest 3/3, direct integration runner 150/150
- The current presentation slice also adds a resource-backed Fog Beacon
  (380 Material, 620 Energy, 80 Research; 24 seconds) that creates a 900 m
  localized tactical light without disabling world fog. Release build, CTest
  3/3, and the Godot skirmish harness pass 66/66. Active water now uses
  layered shader current, water-only swells, and animated shore foam. Build ghosts now raycast to
  the true heightfield; Q/E rotate tactical camera yaw; selected completed
  structures show a name and committed Material/Energy billboard. The P0 slice
  also covers camera-relative movement after yaw, build-card hover tooltips,
  completed-structure navigation blockers, and the F8 environment debug panel.
  The compact icon build deck
  uses strategic-style unit/structure glyphs, blue wrench and orange bolt cost
  icons, and a condensed tactical font; its fourth structure card selects the
  Beacon without overflow. Desktop GPU composition and
  fog/light-balance review remains unverified.

  The terrain shader now consumes an explicit map-brightness parameter and up
  to eight bounded Fog Beacon light records. The F8 brightness slider updates
  the terrain as well as the scene lighting, and completed Beacons brighten the
  ground locally through the same material path. The presentation harness now
  passes 68/68; CTest remains 3/3. Desktop GPU visual verification of the
  resulting terrain/fog balance is still required.

  Completed structure blockers now close only their center strategic cell and
  allow an engineer occupying that cell to exit through a walkable neighbor.
  This fixes the post-construction immobilization regression. Release build,
  CTest 3/3, and the presentation harness 68/68 pass.

  The F8 environment panel now exposes `CLEAR`, `OVERCAST`, and `STORM` weather
  presets plus `DAYTIME`/`NIGHTTIME` controls. Presets update sky/fog state;
  nighttime enables the shadow-casting moon light and disables the sun. The
  presentation harness passes 72/72. Continuous time, transition blending,
  precipitation, and gameplay effects are not yet implemented.

  The daytime directional light was corrected to a higher overhead pitch with
  stronger authored energy; clear weather receives a brighter sun multiplier
  and storm weather a dimmer one. The presentation harness passes 73/73.
  Desktop GPU review remains the final authority for visual sun-angle tuning.

  The active terrain shader now receives explicit weather/time daylight and no
  longer darkens distant terrain toward green by 62%. This addresses the
  clear-daytime terrain appearing shadowy even with fog disabled; harness is
  still 73/73.

  Desktop evidence then identified grazing sun shadows as a separate issue.
  The sun now uses the higher `(-72,-28)` daytime angle, and the terrain shader
  has controlled weather-scaled ambient fill so shadowed daytime ground stays
  readable while nighttime remains dark. Harness remains 73/73.

  Backlog FEATURE-002 is complete. All ground prototypes now author their own
  acceleration, braking, low/high-speed turn rate, minimum turn radius, reverse
  behavior, response, and pivot capability. Movement consumes that content
  deterministically for direct and flow-field orders: wheeled vehicles arc or
  reverse instead of center-axis spinning, while tracked vehicles can pivot
  only where enabled. The active GDExtension provides authoritative heading,
  target heading, and speed; F8 displays the selected ground vehicle's state.
  Regression coverage includes bounded detour steering, rearward wheeled
  reversal, and profile ownership. Release build, CTest 3/3, and Godot
  presentation 73/73 pass.

  FEATURE-008 is complete. Structure placement samples each configured
  footprint against the native 40 km heightfield and rejects water, map edges,
  blocked cells, excessive slope, and excessive height variation. The Godot
  ghost uses the same native validator and turns red for invalid terrain; the
  queue path repeats validation. Native terrain bounds now follow the theater,
  and the scenario loads its heightmap before starting. Regression coverage
  includes valid land, water, boundaries, excessive relief, and blocked cells.
  Release build, CTest 3/3, and Godot presentation 76/76 pass. Continue with
  FEATURE-003 constructible roads.

  FEATURE-003 is complete for its initial backlog scope. The current slice adds a selected Field Engineer
  `R` road mode with a two-point preview, native terrain/navigation validation,
  a paid timed `RoadNetwork`, terrain-conforming completed road strips, and
  weighted A*/flow-field navigation plus ground-speed benefits on road-covered
  cells. Verification is a Release build, CTest 3/3, direct integration runner
  160/160, and Godot presentation 78/78. Road balance is loaded from
  `data/road_network.json`, and segments record shared-endpoint connectivity.
  FEATURE-004 off-road attrition is now complete for its initial backlog
  scope. `OffRoadWear` accumulates distance-based wear with data-authored unit
  factors, drains Material/Energy, applies a bounded speed penalty, and uses a
  strongly reduced multiplier on road cells. Selected ground units expose
  surface, wear, and speed telemetry in the HUD. Release build, CTest 3/3,
  direct integration runner 161/161, and Godot presentation 79/79 pass.
  Terrain-biome modifiers and wear recovery/maintenance are deferred follow-up.

  FEATURE-006 is complete for its initial neutral-dressing scope. The
  presentation loads deterministic civilian clusters from
  `godot/project/scenarios/civilian_dressing.json`, renders three instanced
  building groups, keeps them on authored land, and registers localized native
  navigation blockers while protecting spawn/resource/infrastructure areas.
  Release build, CTest 3/3, direct integration runner 162/162, and Godot
  presentation 82/82 pass. Destruction, occupation, and civilian simulation
  remain deferred.

  FEATURE-001 is complete for the current terrain presentation scope. The
  forest now uses a shared low-poly trunk plus layered canopy mesh for
  tactical view and a reduced cone mesh after the 5 km strategic threshold.
  Both remain MultiMesh-batched and the presentation harness verifies the LOD
  switch. Release build, CTest 3/3, direct integration runner 162/162, and
  Godot presentation 85/85 pass. Production tree imports and biome-specific
  art remain deferred polish; FEATURE-007 resource icon polish is now verified.

  FEATURE-007 is complete for the current resource-HUD scope. A shared
  vector-drawn icon layer now supplies the blue Material wrench, orange Energy
  bolt, and green future-ready Research sigil in the economy strip and compact
  build cards. Godot presentation passes 86/86; Release build, CTest 3/3, and
  the direct integration runner remain green at 162/162. Further changes are
  desktop art-direction polish only.

  FIX-001 is now complete for the current navigation scope. Completed
  military structures block their authored rectangular footprint in the native
  ground grid, routes detour around it, and flow-field escape preserves the
  builder's ability to leave a cell blocked by its newly completed structure.
  Release build, CTest 3/3, direct integration runner 163/163, and Godot
  presentation 86/86 pass.

  FIX-005 now uses a persistent bottom inspection strip rather than transient
  tooltips. The slim black bar presents a quiet empty state, then friendly/
  hostile unit identity and integrity or completed-structure Material/Energy
  detail when hovered. Build cards take priority and report blueprint name,
  Material, Energy, build time, and ready/locked state. Godot presentation
  passes 89/89; Release build and CTest 3/3 pass.

  FIX-002 now ships as the `FLOODLIGHT`: a focused 360 m, 6-energy local light
  whose terrain contribution remains visible at night, independent of the
  global daylight multiplier. Terrain texture tiling is 384 repetitions across
  the 40 km map to preserve close-range detail. Camera panning cancels an
  active zoom anchor and clears a stale middle-button latch; the strategic zoom
  regression uses an off-center cursor. Release build, CTest 3/3, and Godot
  presentation 91/91 pass.

  FIX-003 is complete: Q/E yaw and camera-relative ground-plane translation
  are covered by the presentation harness. FIX-004 is superseded by the
  explicit product direction to keep blue visibility, purple radar, and red
  attack-range hexes visible on unselected units; preserve that behavior. The
  redundant yellow selection hex is removed, and the remaining envelopes are
  flat terrain-projected `ImmediateMesh` lines rather than elevated box tubes.

Engineer placement is now authoritative as well: `create_unit_with_type` rejects
Field Engineer spawns that cannot support the shared construction footprint and
short road probes, including water, steep relief, map edges, and blocked cells.
Godot skirmish startup searches nearby deterministic candidates so both visible
opening engineers begin on build-capable ground. The native runner is 160/160
and the Godot presentation harness is 78/78. Camera clearance now samples the
camera-to-focus line, engineer models receive a terrain lift, and cursor-wheel
zoom preserves the world point beneath the cursor throughout the eased zoom
motion, not only its initial frame.

**Goal 11 status:** VERIFIED. Completion notification coverage passes in the Godot presentation harness. No canonical Goal 12 document exists yet, so future work should pause until one is authored and selected.

**Build commands:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```
