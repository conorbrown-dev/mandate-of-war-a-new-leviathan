# Next Tasks — Reconciled Sequential Recovery

**Updated:** 2026-09-13. Goals 05–11 are verified; no canonical Goal 12 specification exists. See `docs/WORKTREE_RECONCILIATION.md`.

## Completed: Goal 09 — logistics improvements

All acceptance rows in `09_LOGISTICS_IMPROVEMENTS.md` are verified, including path-aware return planning, finite-stock resupply, telemetry, benchmark, failure-probe, and Codex review evidence.

## Completed: Goal 10 — terrain system

The canonical `10_TERRAIN_SYSTEM.md` criteria are verified, including heightmap
loading, biome/material rendering, road traversal costs, terrain-following
ground collision, tests, benchmark, and Codex review.

## Completed: Goal 11 — Forward Seizure and Base Establishment

The canonical `11_FORWARD_SEIZURE.md` criteria are verified, including
capability-gated INSTALL, deterministic zone progression, fixed-tick FOB
construction, authoritative telemetry, tests, and Codex review.

## Gated work

- **No active numbered goal:** Goal 12 has no canonical specification yet.
- Do not apply `stash@{0}` or repair/remove the unavailable model-integration worktree without explicit user authorization.

## Validation workflow

Use `tools/validate <scenario-id>` during gameplay iteration and `tools/validate all` before broad sign-off. Use `tools/test` for the Release/native/Godot/validation regression sweep. Keep accepted visual baselines explicit; do not change the current strategic-zoom references or 0.995 threshold without an assigned visual change and inspection.

The next numbered gameplay goal still requires a canonical goal document. When one is supplied, add or extend the narrow scenario that proves its real player workflow instead of creating a test-only substitute.

## Latest Gameplay Integration

Terrain grounding and structure placement were corrected on 2026-09-13. Keep
Godot and native terrain sampling aligned to the rendered 320 x 320 triangle
mesh, preserve the Field Engineer's mesh-bottom grounding assertion, and route
interactive structure placement through the bounded nearby-site resolver.
Airfield validation now uses a 250 x 750 m operational footprint and accepts
manageable broad slopes while continuing to reject water, map edges, blocked
cells, and cliffs.

The tactical presentation now keeps unit range envelopes visible when units
are not selected: blue visibility, purple radar, and red attack hexes. A
completed tactical airfield is required before a conventional fighter can be
queued; its ferry consumes the fighter prototype's operational Material/Energy
and the fighter enters from beyond the map edge.

The starting Broken Strait theater is now 40 km square with deterministic
mountains, hills, valleys, ravines, and lowlands. Its active scene has an
overcast procedural sky, distance fog, ambient sky fill, directional shadows,
and lit terrain/forest materials so play reads as a battlefield rather than an
editor viewport. A land-only Fog Beacon now converts 380 Material, 620 Energy,
and 80 Research into a completed 900 m localized tactical light, retaining fog
as a battlefield constraint while providing an economy-backed way to improve
local clarity. The next terrain slice should connect the authored relief to
gameplay traversal rules and native territory-zone population, then validate
the visual composition and beacon light/fog balance in a desktop GPU session.

The build deck uses compact, strategic-style type glyphs and blue wrench/orange
bolt cost marks instead of full production names and `M`/`E` labels. Keep full
build details in hover affordances as future catalog entries are added; the
fourth Fog Beacon structure card is currently covered by the 59-check Godot
presentation harness.

Build ghosts and placement targets now resolve against the actual terrain
heightfield rather than a flat plane. Q/E provide held tactical camera yaw;
selected completed structures expose an above-model name and committed
Material/Energy label. Validate cursor feel, yaw direction, and label scale in
a desktop GPU session alongside the existing fog/light composition review.

Water now uses layered animated current, restrained water-only swells, and
shoreline foam through the active terrain shader. Tune its visible amplitude
and foam brightness in the same desktop GPU review; its speed and height are
authored material parameters and covered by the 63-check Godot harness.

The current P0 slice also fixes camera movement after yaw, makes compact
build-card hover tooltips render above the deck, blocks completed structures in
ground navigation, and adds the F8 developer environment panel. The Godot
harness is 68 checks with CTest 3/3. Terrain brightness now updates the custom
terrain material directly, and completed Fog Beacons publish bounded local
illumination to that material so the ground responds instead of only nearby
models. The next implementation slice should take
up vehicle steering and then road/path-cost infrastructure; civilian dressing,
replacement trees, and production resource artwork remain open.

The completed-structure blocker regression is covered: the builder can escape
its newly occupied strategic cell after construction completes, while later
routes still avoid the structure cell.

Developer environment controls now expose clear/overcast/storm weather and
daytime/nighttime lighting for desktop composition review. A continuous clock,
dawn/dusk transitions, precipitation, and weather gameplay effects remain open.

The daytime directional light now uses a higher overhead pitch and stronger
clear-weather energy after the terrain appeared incorrectly backlit/dark. A
desktop GPU review remains useful for final sun angle and shadow tuning.

Terrain lighting now follows the developer weather/time controls through an
explicit shader parameter, rather than relying only on directional-light
response in the Compatibility renderer. The former dark-green strategic fade
has been removed.

Daytime terrain now receives shadow fill through the custom shader and the sun
uses a high overhead angle, addressing the desktop-observed long black terrain
shadows. Fine-tune values through the F8 controls in a GPU session as art
direction is established.

FEATURE-002 is complete: data-authored ground chassis profiles drive bounded
turning, braking, reverse maneuvers, minimum turn radii, and pivot capability.
The selected-vehicle F8 diagnostic shows authoritative heading, steering
target, and speed. FEATURE-008 is now complete: terrain-aware structure
placement validates the full footprint against water, boundaries, blocked
cells, slope, and height variation; invalid ghosts are red and the native
queue path repeats the check. Continue with FEATURE-003 constructible roads,
followed by FEATURE-004 off-road movement penalty/attrition; both build on this
movement foundation.

FEATURE-003 is complete for its initial backlog scope: `R` road mode for Field
Engineers, terrain-aware red/valid previews, resource reservation, timed
completion, persistent terrain-conforming road visuals, weighted
navigation/speed benefits, content-authored balance values, and shared-
endpoint segment connectivity are verified. Continue with FEATURE-004
off-road attrition.

FEATURE-004 is now complete for its initial backlog scope. `OffRoadWear` is
updated from traveled distance with data-authored unit factors; off-road
movement consumes Material/Energy and receives a bounded speed penalty, while
road cells use a near-zero wear multiplier. Selected ground units expose
surface, wear, and speed telemetry in the HUD. Release build, CTest 3/3,
direct integration runner 161/161, and Godot presentation 79/79 pass. Future
work may add terrain-biome modifiers and explicit wear recovery/maintenance.

FEATURE-006 is complete for its initial neutral-dressing scope. Deterministic
civilian clusters use instanced house, warehouse, and utility meshes; accepted
land footprints block local ground navigation and protected spawn/resource/
infrastructure corridors remain clear. Release build, CTest 3/3, direct
integration runner 162/162, and Godot presentation 82/82 pass. Destruction,
occupation, and civilian simulation remain deferred.

FEATURE-001 is complete for the current terrain presentation scope. Forest
instances now use the three user-supplied GLB forms in tactical view and an
imported oak LOD2 mesh in strategic view, all batched through MultiMesh. The
default map contains 960 heightmap-grounded trees in nine deterministic large
groves; the classic and animated donor conversions remain optional Godot assets.
Biome-specific placement and art direction remain future polish.
The focused rendered oak-grove scenario now covers tree count, configured size
variation, terrain grounding, grove density, LOD population, screenshots, and
video; future foliage work should retain this scenario.
The optional English-oak USD conversions are available for future asset work,
but need simplification and authored materials before tactical activation.
FEATURE-007 resource icon polish is now verified.

FEATURE-007 is complete for the current resource-HUD scope. The economy strip
and build cards share scalable vector-drawn Material/wrench and Energy/bolt
icons, with a matching Research sigil reserved for future resource expansion.
Godot presentation is 86/86; Release build, CTest 3/3, and the integration
runner remain green at 162/162. Further work is limited to art-direction
polish after desktop review.

FIX-001 is now complete for the current navigation scope. Completed military
structures block their authored rectangular footprint, ground routes detour
around it, and engineers already inside a newly blocked cell retain an escape
direction. Release build, CTest 3/3, direct integration runner 163/163, and
Godot presentation 86/86 pass. Remaining work is limited to future dynamic
destruction/collision systems outside this backlog slice.

FIX-005 now uses a persistent bottom inspection strip rather than transient
tooltips. The slim black bar presents a quiet empty state, then friendly/hostile
unit identity and integrity or completed-structure Material/Energy detail when
hovered. Build cards take priority and report blueprint name, Material, Energy,
build time, and ready/locked state. Godot presentation is 89/89; Release build
and CTest 3/3 pass.

FIX-002 now ships as the `FLOODLIGHT`: a focused 360 m, 6-energy local light
whose terrain contribution remains visible at night. Terrain texture tiling is
384 repetitions across the 40 km map to preserve close-range detail. Camera
panning cancels an active zoom anchor and clears a stale middle-button latch;
the strategic-zoom regression uses an off-center cursor. Release build, CTest
3/3, and Godot presentation 91/91 pass. Final visual balance remains desktop
GPU art direction.

FIX-003 is complete and verified through the Q/E yaw and camera-relative
translation harness checks. FIX-004 is superseded by the product direction to
keep blue visibility, purple radar, and red attack-range hexes visible on
unselected units; no hide-range regression should be introduced. The redundant
yellow selection hex is removed, and the remaining envelopes are flat
terrain-projected `ImmediateMesh` lines rather than elevated box tubes.

Engineer spawn placement now uses the same native construction suitability
rules as structures and roads. Invalid water, edge, steep, and blocked
engineer starts are rejected, while skirmish startup selects a nearby valid
site for each visible Field Engineer. This is verified by 160/160 native
integration assertions and the 78/78 Godot presentation harness.

Camera terrain clearance, engineer model grounding, and cursor-anchored
strategic zoom are now covered by the presentation harness. The zoom check
uses the full eased camera movement so the terrain under the wheel cursor stays
anchored beyond the first correction frame. Keep validating these against a
desktop GPU session when further camera or terrain changes are made.

The reachable Command Walker catalog includes all six Elite unit prototypes;
fighter, VTOL, and patrol-boat entries are no longer hidden after the first
three cards. Validation is CTest 3/3 and `test_skirmish.gd` with 66 passing
checks, including fighter queue, completion, and presentation registration.
BUILD commands now preserve a nonzero player-selected rally/output target;
`commands_owned_factory_build_research_and_destruction` covers that contract.
The invalid `Node`/`Node3D` territory child was removed from `main.tscn`;
reference-model loading passes 15/15 and the fighter presentation assertion
requires an imported model.

## Goal 11 Verification Record

Zone progression logic is verified:
Further economy work should add range/contestation rules and construction
feedback before introducing separate material inventories; the demo deliberately
does not track oil, silicon, plastic, or metal as distinct balances.

## Goal 11 Verified

| Criterion | Status |
|---|---|
| G11-TERRITORIAL (domain types) | ✅ Implemented |
| G11-ZONES (zone type definitions) | ✅ Implemented |
| G11-INSTALLATIONS (site type definitions) | ✅ Implemented |
| G11-CAPABILITIES (unit capability tags) | ✅ Implemented |
| G11-REQUIREMENTS (establishment structure) | ✅ Implemented |
| G11-INTEGRATION (manager interface) | ✅ Implemented |
| G11-UNITASSIGN (unit-to-zone mapping) | ✅ Implemented |
| G11-ZONEPROG (zone progression tracking) | ✅ Implemented |
| G11-INSTALLCMD (FOB construction command) | ✅ Implemented |
| G11-CONSTRACK (FOB construction tracking) | ✅ Implemented |
| G11-ZONETESTS (zone progression tests) | ✅ Implemented (3 test cases, 10 assertions) |
| G11-VISUALIZATION (Godot territory rendering) | ✅ Implemented (zone boundaries, state colors) |
| G11-GDWIRING (GDExtension territory query) | ✅ Implemented (`territory_get_zone_info`, `territory_get_zone_count`) |

### Completed in This Session
- G11-TERRITORIAL: `TerritorialControlState` enum (NEUTRAL, CLAIMED, SECURED, CONSOLIDATED, ESTABLISHED, CONTESTED)
- G11-ZONES: `ZoneType` enum with progression (RECONZONE through HELIPADZONE)
- G11-INSTALLATIONS: `InstallationType` enum (COMMAND_POST, FOB, LOGISTICS_HUB, RECON_STATION, DEFENSIVE_BATTERY, AIRFIELD, NAVAL_BASE) with `InstallationState`
- G11-CAPABILITIES: `SeizureCapability` enum (RECON, SEIZURE, SECURE, CONSTRUCT_FOB, CONSTRUCT_LOGISTICS, ESTABLISH_BASE, DEFEND, HARVEST_SECURED)
- G11-REQUIREMENTS: `EstablishmentRequirements` and `ZoneProgression` structures
- G11-INTEGRATION: `TerritorialControlManager` interface with full implementation in `src/ecs/components/territorial_control.{hpp,cpp}`
- G11-UNITASSIGN: Unit-to-zone assignment methods (`assign_unit_to_zone`, `remove_unit_from_zone`, `update_unit_zone_assignment`)
- G11-ZONEPROG: Zone progression tracking (`update_zone_progression`, `process_zone_progression`)
- G11-INSTALLCMD: `CommandType::INSTALL` command type and `install_fob()` handler in `simulation.cpp`
- G11-CONSTRACK: `InstallationState` extended with `constructing`, `construction_progress`, `construction_cost` fields
- G11-VISUALIZATION: Territory visualization in Godot (`main.gd`, `territory_terrain.gdshader`, `main.tscn`)
- G11-GDWIRING: GDExtension territory query functions (`territory_get_zone_info`, `territory_get_zone_count`) with 256×256 texture generation

## Goal 09 — Deferred

Goal 09 was deferred in favor of Goal 10 to address terrain system integration first.
Goal 10 (terrain system) is now deferred in favor of Goal 11 (Forward Seizure feature foundation).

## Future Work — No Active Goal 12

### Next tasks:
- Author and select the next canonical numbered goal before starting new milestone work
- Keep the 40 km skirmish theater placement rules aligned as new civilian structures and naval rosters are added
- Extend camera presentation tests to cover held-Space free-float input when interactive input automation is available

## Verification

| Criterion | Status |
|---|---|
| All configured tests | ✅ PASS: CTest 3/3; direct integration runner 150/150 |
| Skirmish match loop (AI production/research) | ✅ PASS: `skirmish_validated_setup_and_repeatable_legal_terminal` passes with 934 checks |
| Mesh JSON generation (61 assets) | ✅ PASS |
| Blender model generation (33 assets) | ✅ PASS |
| Faction and tier coverage | ✅ PASS |
| Territory shader visualization | ✅ PASS |
| G11-VISUALIZATION (Godot territory rendering) | ✅ PASS |
| G11-GDWIRING (GDExtension territory query) | ✅ PASS |

### Verification Commands

```bash
cd /home/conor/repos/mandate-of-war
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
godot4 --headless --path godot/project --script res://test.gd
```

## Goal 10 — Deferred

Goal 10 (terrain system) is no longer active.

## Future Work — Goal 11 Continue

### Implementation complete (this session):
- `src/ecs/components/territorial_control.hpp` — domain types and manager interface
- `src/ecs/components/territorial_control.cpp` — full implementation (448 lines)
- `TerritorialControlManager` instance in `Simulation` class
- `TerritorialControlManager::reset()` called in `Simulation::start()`
- `TerritorialControlManager::update()` called in `Simulation::environment_phase()`
- Unit-to-zone assignment methods: `assign_unit_to_zone`, `remove_unit_from_zone`, `update_unit_zone_assignment`
- Zone progression tracking: `update_zone_progression`, `process_zone_progression`
- `CommandType::INSTALL` added to `network/types.hpp`
- `install_fob()` method in `simulation.cpp` with FOB construction integration
- `InstallationState` extended with `constructing`, `construction_progress`, `construction_cost` fields

### Next tasks:
- Verify zone progression logic with test cases (security thresholds, state transitions)
- Add territory visualization in Godot (zone boundaries, control state colors)
- Add unit capability assignment based on unit type/faction
- Completion notification coverage for the rendered FOB construction flow is verified
- Add economy integration for materials deduction during construction
- Add construction progress animation/tick updates

## Asset Generation Reference

| Script | Purpose |
|---|---|
| `scripts/generate_all_assets.py` | Generate 61 mesh JSON definitions |
| `scripts/generate_all_blender.py` | Generate 33 Blender models |
| `scripts/generate_unit_mesh.py` | Generate single unit mesh JSON (CLI) |
| `scripts/generate_unit_blender.py` | Generate single Blender model (CLI) |

## Verification Commands

```bash
cd /home/conor/repos/mandate-of-war
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/rts_integration_tests --gtest_filter="*skirmish_validated_setup_and_repeatable_legal_terminal*"
./build/rts_integration_tests --gtest_filter="*commands_owned_factory_build_research_and_destruction*"
```

## References

- Command authority validation: `simulation.cpp:447-492`
- DEFEND enemy detection: `simulation.cpp:450-483`
- RESEARCH execution: `simulation.cpp:1498+` (process_command_internal switch)
- Research prerequisites: `production_manager.cpp:307` (can_queue_unit)
- Unit fallback data: `src/ecs/components/factions.cpp` (research_prerequisites)
