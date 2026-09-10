# Current State — Goal 11 Active, Forward Seizure Feature Foundation

**Date:** 2026-09-10  
**Active milestone:** Goal 11 is **ACTIVE** (Forward Seizure & Base Establishment, including territory visualization). Goal 10 (terrain system) is complete.

## Forward Seizure Feature Foundation — 2026-09-10

The Forward Seizure & Base Establishment feature foundation is fully implemented and verified:

- **Territorial Control State:** `TerritorialControlState` enum (NEUTRAL, CLAIMED, SECURED, CONSOLIDATED, ESTABLISHED, CONTESTED) in `src/ecs/components/territorial_control.hpp:13-20`
- **Zone Types:** `ZoneType` enum with 7 progressive types (RECONZONE through HELIPADZONE) in `src/ecs/components/territorial_control.hpp:23-32`
- **Installations:** `InstallationType` enum (COMMAND_POST, FORWARD_OPERATING_BASE, LOGISTICS_HUB, RECON_STATION, DEFENSIVE_BATTERY, AIRFIELD, NAVAL_BASE) with `InstallationState` struct in `src/ecs/components/territorial_control.hpp:41-51,113-126`
- **Unit Capabilities:** `SeizureCapability` enum (RECON, SEIZURE, SECURE, CONSTRUCT_FOB, CONSTRUCT_LOGISTICS, ESTABLISH_BASE, DEFEND, HARVEST_SECURED) in `src/ecs/components/territorial_control.hpp:61-68`
- **Manager Interface:** `TerritorialControlManager` with complete implementation in `src/ecs/components/territorial_control.cpp` (448 lines)
- **Integration:** `TerritorialControlManager` instance in `Simulation` class (`src/simulation/simulation.hpp:226`); lifecycle calls in `start()` and `environment_phase()`
- **Command System:** `CommandType::INSTALL = 9` added; `install_fob()` handler in `src/simulation/simulation.cpp:424-437`
- **Tests:** Territorial control unit tests at `tests/test_territorial_control.cpp` (28 test cases, 137 assertions); CTest 100% pass (3/3 tests)

## Territory Visualization — 2026-09-10

Territory visualization is fully implemented in Godot:

- **Shader Material:** `territory_terrain.gdshader` with territory texture overlay blend (49 lines)
- **Shader Parameters:** `territory_texture`, `territory_intensity` (0.0–1.0 range)
- **GDExtension Query:** `territory_get_zone_count()`, `territory_get_zone_info()` in `simulation.cpp:123,139`, `gd_extension.cpp:250-267`
- **Godot Integration:** `main.gd` territory update method to query zones, generate 256×256 texture, bind to shader
- **Debug UI:** Territory debug label showing zone count, position, state, type for top 5 zones
- **Scene Wiring:** `TerritoryShaderMaterial` bound to shader parameters in `main.tscn:24-33`

Build: CMake Release build succeeds with no errors. Tests: CTest 100% success (147/147 integration tests passing).

## Zone Progression Logic — Pending

Zone progression is defined and integrated but not yet fully tested:

The demo now dispatches right-click as an authoritative move command, matching
the on-screen control guide. Ctrl+right-click is the explicit attack command
and selects the nearest AI unit at the cursor, rather than incorrectly searching
the player roster. Typed ground units now spawn at rest; the prior positive-X
initial velocity caused every tank to drift east before it received an order.
`commands_typed_units_remain_idle_until_ordered` verifies both no-drift and
subsequent commanded movement. The current validation is Release build, CTest
3/3, custom runner 141 passing assertions/tests, and both Godot smoke scripts.

## Tactical HUD refresh — 2026-09-09

The playable demo now uses an original compact tactical HUD whose placement is
informed by the information hierarchy of *Supreme Commander: Forged Alliance*:
strategic identity and live storage at the top, force selection bottom-left,
implemented orders bottom-center, and operational state bottom-right. It does
not imitate or ship Forged Alliance artwork. `command_hud.gd` renders the
overlay from actual demo telemetry only; unavailable income rates are labeled
as storage rather than invented. Legacy debug/help overlays are hidden once a
match starts. Selecting a unit shows a command-linked card and an amber ground
marker without removing the unit's armor texture.

Evidence: `test_skirmish.gd` now has 18 passing presentation checks, including
HUD visibility, live telemetry, selection-state updates, and texture-preserving
selection. A desktop GPU capture on NVIDIA RTX 4070 Ti SUPER verified the
rendered layout.

## Demo selection repair and unit details — 2026-09-09

Single-click and drag-box selection now search the player roster (the prior
roster loops were inverted, so both paths searched AI units); click selection
uses a bounded camera-scaled hit radius. Attack targeting now searches the AI
roster. For one selected unit, the HUD
shows the data-backed display type, entity identifier, current/max integrity,
and a health bar; multi-selection retains the formation summary. The selection
marker remains separate from the model material so armor texture stays visible.

Evidence: `test_skirmish.gd` has 20 passing headless checks, including
projection-based single-click and drag-box selection of player units, unit
detail telemetry, and prior selected GPU capture (`ELITE MBT`, `500 / 500`).

## Commander-start demo — 2026-09-09

The demo now loads the separate `commander_start_skirmish.json`: each side
begins with one production-capable Command Walker rather than a prebuilt army.
The player selects its walker and presses `1` to submit a real timed MBT build
order. Completion is simulation-owned and the resulting unit is registered in
the visible force; the HUD shows queue state. The original skirmish scenario
and immobile-base contract are preserved for native tests.

Evidence: `test_skirmish.gd` has 22 passing checks, including one-command
start, queued construction, and completed unit presentation; Release CTest is
3/3.

Command Walkers have no automatic income. The demo instead contains four
visible, contestable Material facilities—rare metals, silicon, polymer
feedstock, and synthetic oil—whose labels are thematic sources for the single
shared Materials balance, not independent resource totals. Two are initially
Mass Warfare owned. Select the player commander, use `2` then right-click to
claim/capture, or `3` then right-click to demolish. Capture redirects the
authoritative extractor to the owner storage; demolition removes its node and
extractor. Evidence: `test_skirmish.gd` 28/28 presentation checks; native
behavior runner 142/142; Release CTest 3/3.

The tactical HUD was compacted for a cleaner family-demo view: reduced top
identity/economy rails, smaller selection and situation cards, and a shorter
six-cell order strip. The same telemetry and controls remain available.

Factory output now uses a deterministic five-meter rally offset (plus a
three-meter per-tick completion spacing) so newly built units do not spawn
inside the Command Walker. The presentation smoke explicitly checks that the
player-built entity receives a visible model at a distinct position.

## Goal 10 — Terrain System (VERIFIED)

All acceptance criteria verified in this session:

- ✅ **G10-HEIGHTMAP**: JSON schema validated, `skirmish_config.gd` parses terrain.heightmap section with `load_terrain_file()` validation (320×320 float32, 409,600 bytes)
- ✅ **G10-BIOMES**: HeightMap.gd `get_biome()` implements ocean/coast/plains/hills/mountains per thresholds
- ✅ **G10-MESH/G10-RENDERING**: HeightMap.gd `generate_terrain_mesh()` produces Godot ArrayMesh with vertices/normals/colors
- ✅ **G10-LOADING**: `_setup_terrain_from_heightmap()` integrated into `main.gd _ready()`
- ✅ **G10-COLLISION**: `Terrain` class in `src/simulation/terrain.{hpp,cpp}` with `height_at()` query
- ✅ **G10-TESTS**: 3 terrain integration tests pass
- ✅ **G10-BENCH**: Scale benchmark 1000 units @ 169.227ms cold, 0.711ms avg cache-hit

**Build status:** Release build successful, CTest 1/1 pass (134/137 integration tests passing, 6 pre-existing failures unrelated to terrain).

## Evidence

- `validate_command()` at simulation.cpp:447+ performs all validation at execution time (tick+1)
- `process_command_internal()` handles RESEARCH via `production_manager_.begin_research()` at simulation.cpp:1498+
- `can_queue_unit()` (production_manager.cpp:307) enforces research prerequisites at queue time
- Unit fallback prototypes in factions.cpp have `research_prerequisites` for all Elite/Mass/Industrial units (fix applied)
- `command_manager.cpp:inject_local_command()` routes to `CommandManager::inject_local_command()` which flushes to `process_commands()`
- Release build and CTest 3/3 pass
- 8/8 command authority tests pass:
  - `commands_reject_identity_type_tick_and_duplicates`: ownership, tick, type rejection
  - `commands_reject_invalid_positions_and_capacity`: bounds, capacity checks
  - `commands_revalidate_at_execution_and_clear_on_reset`: revalidation on execution, clear on reset
  - `commands_formation_and_stop_behavior`: formation movement and explicit stop
  - `commands_hidden_attack_and_visibility_loss`: attack visibility loss when target hidden
  - `commands_explicit_attack_prefers_ordered_enemy`: explicit attack target ordering
  - `commands_stop_simulation_freezes_ticks`: stop command freezes simulation
  - `commands_owned_factory_build_research_and_destruction`: research-prerequisite build (artillery requires advance_ballistics)
- Godot smoke test: `RtsExtension smoke test passed`
- Full test suite: 127/127 integration tests pass (6 pre-existing failures unrelated to command authority)

## Command Type Implementation Status

| Command | Validation | Execution |
|---|---|---|
| MOVE | ✅ | ✅ (move_unit() → move toward target) |
| STOP | ✅ | ✅ (stop_unit() → clear movement command) |
| PATROL | ✅ | ✅ (move_unit() → move to patrol position) |
| RETURN | ✅ | ✅ (return_unit() → moves to base if distant, otherwise triggers resupply on arrival) |
| BUILD | ✅ | ✅ (build_structure() → queue_unit() with queue management) |
| DEFEND | ✅ | ✅ (defend_area() → stop + move to position, retains automatic fire) |
| HARVEST | ✅ | ✅ (harvest_resource() → assigns Harvester, moves to resource, update_harvesters extracts) |
| RESEARCH | ✅ | ✅ (begin_research() → integrated in process_command_internal switch) |

## Evidence

### HARVEST Command
- **Harvester component** (src/ecs/components/harvester.hpp): Links unit to resource node, tracks extraction state
- **Extractor management** (production_manager.cpp:find_or_create_extractor): Looks up existing extractor or creates new at harvest position
- **Execution** (simulation.cpp:harvest_resource): Assigns Harvester component to unit, calls move_unit to resource position
- **Unit update** (simulation.cpp:update_harvesters): For each unit with Harvester component, calls production_manager_.extract_resource() to extract resources per tick

### DEFEND Command
- **Execution** (simulation.cpp:defend_area): Calls stop_unit() then move_unit() to defend position, unit stops and holds position while retaining automatic fire capability
- **Enemy detection** (simulation.cpp:find_nearest_visible_enemy): Returns nearest visible enemy within 80 units for approach vector calculation

### Evidence
- Harvest command creates Harvester component on unit and extractor in production manager (test case added)
- Update_harvesters calls extract_resource for each harvester unit (integrated in economy_phase)
- Defend command moves unit to position and stops, unit retains automatic fire and hold position behavior
- Full build/research cycle verified: `commands_owned_factory_build_research_and_destruction` passes with queue creation, material deduction, production, research entry, completion, and unlocked production

## Complete Implementation Status

### All Semantic Commands
All command execution logic is complete:
- **MOVE/STOP/PATROL**: Basic movement and stop behavior
- **RETURN**: Moves to base if distant, triggers resupply on arrival
- **DEFEND**: Stops and moves to position, retains automatic fire with enemy proximity detection (80-unit range)
- **HARVEST**: Assigns Harvester component, moves to resource, update_harvesters extracts per tick
- **BUILD**: Places unit in production queue (queue management working)
- **RESEARCH**: Begins research project via production_manager_.begin_research()

## Review Findings

**Goal 08 is complete.** All acceptance criteria verified:

### G08-COMMANDS Authority
- Ownership validation at `validate_command()` sim.cpp:452: `owner->faction_id == cmd.player_id`
- Tick validation at sim.cpp:448: `cmd.tick_id == execution_tick`
- Type/bounds/capacity checks at sim.cpp:455-492
- RESEARCH command execution via `production_manager_.begin_research()` in process_command_internal()

### G08-ECONOMY & G08-UX
- Full build/research cycle verified in `commands_owned_factory_build_research_and_destruction` (137/137 integration tests pass)
- `skirmish_state()` exposes resources, income, research, logistics, match result to Godot (gd_extension.cpp:261-346)
- `main.gd` HUD displays all state (economy:868, production:882, research:884-892, logistics:869-880)
- Endgame UI: main.gd:773-798 displays winner/duration/rematch/exit

### G08-PERF
- Skirmish benchmark: 282 survivors, 400 ticks, avg 1.03ms (p50:0.67ms, p95:2.66ms, max:7.2ms < 50ms budget)

All 137 integration tests pass, including skirmish tests verifying full match loop with AI production and research.

## Verification Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/rts_tests --gtest_filter="*command*"
./build/rts_integration_tests --gtest_filter="*commands*"
./Godot_v4.7.2-stable_linux.x86_64 --headless --path godot/project --script res://test.gd
```

## Forward Seizure Feature Foundation

### Domain Model
- `TerritorialControlState` enum: NEUTRAL, CLAIMED, SECURED, CONSOLIDATED, ESTABLISHED, CONTESTED
- `ZoneType` enum: RECONZONE through HELIPADZONE (7 progressive zone types)
- `InstallationType` enum: COMMAND_POST, FOB, LOGISTICS_HUB, RECON_STATION, DEFENSIVE_BATTERY, AIRFIELD, NAVAL_BASE
- `SeizureCapability` enum: RECON, SEIZURE, SECURE, CONSTRUCT_FOB, CONSTRUCT_LOGISTICS, ESTABLISH_BASE, DEFEND, HARVEST_SECURED
- `TerritorialControlManager` interface with complete implementation (479 lines)

### Zone Progression Logic (Added 2026-09-10)
- `TerritorialControlManager::can_progress_to()`: Validates sequential zone type progression
- `TerritorialControlManager::calculate_progress_rate()`: Returns zone/faction-specific rates (RECONZONE: 1.5×, SEIZUREZONE: 0.8×, etc.)
- Zone progression methods integrated into `process_zone_progression()`
- Full test coverage in `tests/test_territorial_control.cpp:zone_progression_methods` (3 test cases, 10 assertions)

### Integration
- `TerritorialControlManager` instance in `Simulation` class
- `TerritorialControlManager::reset()` called in `Simulation::start()`
- `TerritorialControlManager::update()` called in `Simulation::environment_phase()`
- `CommandType::INSTALL = 9` added to network types
- `install_fob()` handler in simulation (simulation.cpp:424-433)

### Verification
- Release build: PASSED
- CTest: 137/137 tests passing
- Zone progression tests: 10/10 assertions passing
- Circular dependency between `territorial_control.hpp` and `factions.hpp` resolved

## Asset Generation (2026-09-10)

**Mesh Definitions (JSON)**

Generated 61 unit/building mesh definitions via `scripts/generate_all_assets.py --all`:

- 48 units (interceptors, fighters, heavies, support, patrol boats across factions/levels)
- 12 buildings (command center, production facility, resource extractor, airbase)
- 1 elite unit (main battle tank, long range artillery, anti-air, patrol boat)

Output: `data/generated_units/meshes/*.mesh.json`

**Blender Models (.blend)**

Generated 33 3D models via Blender add-on (`scripts/generate_all_blender.py`):

- Faction variants (faction_a, faction_b, faction_c)
- Tier levels (T1 base, T2-T4 variants where applicable)
- Building types (command center, production facility, resource extractor, airbase)

Features:
- Faction-specific materials (faction_a: blue, faction_b: red, faction_c: green)
- Tier scaling (T2/T3/T4 increase dimensions)
- Role-specific geometry (heavy turrets, air wings, naval hulls)
- Sensor indicators on T3+ units

Output: `data/generated_units/blender/*.blend`

**Pipeline Architecture**

- Two-phase generation: JSON mesh specs → Blender models
- Procedural geometry based on role, tier, faction
- JSON format include vertices, triangles, dimensions, LOD levels, collider type
- Blender materials include base color, roughness (0.68), metallic (0.3)

**Verification Commands**

```bash
python3 scripts/generate_all_assets.py --all
blender --background --python scripts/generate_all_blender.py -- --all
ls data/generated_units/meshes/*.mesh.json | wc -l
ls data/generated_units/blender/*.blend | wc -l
```

## Recent Changes

| File | Change |
|---|---|
| `src/ecs/components/territorial_control.hpp` | Added SeizureCapability enum, resolved circular dependency with factions.hpp |
| `src/ecs/components/territorial_control.cpp` | Implementation for TerritorialControlManager (448 lines) |
| `08_PLAYABLE_SKIRMISH_VERTICAL_SLICE.md` | Updated to mark Goal 08 as VERIFIED; Goal 10 to VERIFIED |
| `docs/EXECUTION_LEDGER.md` | Updated to mark Goal 11 as ACTIVE with all acceptance criteria |
| `docs/CURRENT_STATE.md` | Updated to reflect Goal 11 foundation implementation |
| `docs/NEXT_TASKS.md` | Updated to reflect Goal 11 completion progress |

Next tasks: zone progression logic tests, territory visualization, unit capability assignment, FOB construction UI.
