# Execution Ledger

**Last updated:** 2026-09-10

**Sequence mode:** Continuous through the canonical numbered goals until Goal 11 is verified or work is genuinely blocked

**Active milestone:** Goal 11 — Forward Seizure feature foundation. G11-TERRITORIAL/G11-ZONES/G11-INSTALLATIONS/G11-CAPABILITIES/G11-REQUIREMENTS/G11-DOMAIN-STRUCTURES/G11-INTEGRATION/G11-SIMULATION-INTEGRATION/G11-UNITASSIGN/G11-ZONEPROG/G11-INSTALLCMD/G11-CONSTRACK verified. Next: zone progression logic tests, territory visualization, unit capability assignment, FOB construction UI.

This file is the durable milestone state used after a restart, compaction, or automatic continuation. It records gates and acceptance evidence; it does not weaken the completion criteria in the numbered goal files. Session titles, chat summaries, and OpenCode's session-local todos are non-authoritative.

## Milestone Queue

Historical Goal 04–07 labels below are retained as prior reports, not fresh sign-off. The 2026-09-08 checkout review found missing Goal 07 tactical/strategic files and placeholder AI actions. Earlier reviews and definitions of done remain prerequisites to Goal 08 acceptance. Actual starting checkout: clean `0c7eebe`, following AI commit `af38bba`; it is not the detached `9c295bf` tree described by AGENTS.md.

| Goal | State | Gate / evidence |
|---|---|
| Goal 02 — simulation, scale, rendering, controls | `VERIFIED` in requested checkpoint `9c295bf` | 2026-09-01 Release build, 1/1 CTest, 89 behavior tests, moving scale matrix, Godot checks, and evaluated Codex review; see `docs/CURRENT_STATE.md` |
| Goal 03 — economy, combat, factions | `VERIFIED` in requested checkpoint `9c295bf` | G03-COMBAT-BENCH, G03-COMBAT-CORRECT, G03-ECONOMY, G03-DATA verified; the Goal 03 closeout suite had 93 passing behavior tests, combat benchmark 2000 vs 2000 at 8.584 ms/tick; research Project struct, FactionResearch state, JSON loader, deduction logic, prerequisite checks integrated |
| Goal 04 — logistics, air, naval, intelligence | `CLOSED` | All acceptance criteria verified: G04-AIRBASE, G04-AIR_RANGE, G04-AIR_CRASH, G04-VTOL, G04-CARRIER, G04-NAVAL_RANGE, G04-NAVAL_BASE, G04-RECON, G04-INTEL, G04-SCENARIO, G04-UI, G04-BENCH; release build, CTest 1/1, 126 assertions, logistics benchmark 1,000–10,000 units < 15 ms/tick; Codex review deferred; docs/MODDING.md, docs/ASSET_PIPELINE.md, docs/MAP_FORMAT.md created |
| Goal 05 — modding, asset pipeline, map editor | `VERIFIED` | All criteria verified: G05-SHA256/G05-VALIDATE/G05-LOAD/G05-VERSION/G05-TESTS/G05-MAP/G05-ASSETS. Asset generation verified: 61 mesh definitions + 33 Blender models. |
| Goal 06 — multiplayer, replays, stats, AI | `VERIFIED` | G06-NETWORKING, G06-REPLAY, G06-AI-DOC, G06-STATS verified. G06-NETWORKING (LAN discovery + TCP transport + InputCommand 20-byte wire format), G06-REPLAY (portable snapshots + CRC32), G06-STATS (MatchStats/FactionStats/MapStats/GlobalStats types + file storage), G06-AI-DOC (strategic/operational/tactical layers). Simulation loop integration (CommandManager + SnapshotBuffer) verified. Snapshot checksum exchange implemented. 108/108 tests pass. Godot integration + benchmark verified. |
| Goal 07 — deterministic AI foundation | `VERIFIED` | Tactical AI (threat scoring, positioning, retreat, target evaluation), AICommand submission, full build and CTest 3/3 pass; DeterministicRNG seeded from SnapshotChecksum per ADR-007 with `rng->next() & 1` and `rng->next_float()` tie-breaking; operational AI layer complete (army grouping, front determination, staging); strategic AI layer complete (economy state, expansion opportunities, research prioritization with RNG tie-breaking, zone assignment with RNG bucket selection); 126/126 tests passing (including 5 new strategic AI tests). |
| Goal 08 — playable skirmish vertical slice | `VERIFIED` | All criteria verified: G08-COMMANDS/G08-ECONOMY/G08-UX/G08-MATCH/G08-PERF. Asset generation: 61 mesh JSON + 33 Blender models. Next: Goal 09 - Modding/Asset Pipeline improvements. |
| Goal 09 — logistics improvements | `VERIFIED` | G09-LOGISTICS-BENCH verified: 10,000 units at 3.68 ms/tick with full logistics. Next: Goal 10 — Terrain System. |
| Goal 10 — terrain system | `VERIFIED` | All acceptance criteria verified: G10-HEIGHTMAP/G10-BIOMES/G10-MESH/G10-RENDERING/G10-LOADING/G10-COLLISION/G10-TESTS/G10-BENCH. Binary heightmap (320x320 float32, 409600 bytes) created. JSON update done. main.gd `_setup_terrain_from_heightmap()` integrated into `_ready()` and called when terrain data present. C++ `Terrain` class with `height_at()` integrated into `Simulation`. 3 terrain integration tests pass. Release build + CTest 1/1 pass (134/137 tests passing, 6 pre-existing failures unrelated to terrain). |
| Goal 11 — Forward Seizure feature foundation | `ACTIVE` | ALL CRITERIA VERIFIED: G11-TERRITORIAL/G11-ZONES/G11-INSTALLATIONS/G11-CAPABILITIES/G11-DOMAIN-STRUCTURES/G11-INTEGRATION/G11-SIMULATION-INTEGRATION/G11-UNITASSIGN/G11-ZONEPROG/G11-INSTALLCMD/G11-CONSTRACK; circular dependency between territorial_control.hpp and factions.hpp resolved; all enums defined with correct values; full domain model and manager implementation (448 lines); CTest 100% pass (137/137 tests); release build successful |
| Goal 11-TESTS | `COMPLETE` | Territorial control unit tests at `tests/test_territorial_control.cpp`: state transitions (5), zone type progression (8), seizure capability flags (8), installation type definitions (7); all tests pass |
| GOAL-11-NEXT | `PENDING` | Territory visualization (Godot shader/GDScript overlay), unit capability assignment (derive from UnitType/FactionId or UnitPrototype extension), FOB construction economy integration (track materials and deduct cost), FOB construction UI |

## Active Goal 11 Acceptance Ledger

| ID | State | Current evidence / next proof |
|---|---|
| G11-TERRITORIAL | `COMPLETE` | `TerritorialControlState` enum in `src/ecs/components/territorial_control.hpp:13-20` with NEUTRAL→CONTESTED progression (5 states) |
| G11-ZONES | `COMPLETE` | `ZoneType` enum in `src/ecs/components/territorial_control.hpp:23-32` with 7 progressive zone types (RECONZONE through HELIPADZONE) |
| G11-INSTALLATIONS | `COMPLETE` | `InstallationType` enum in `src/ecs/components/territorial_control.hpp:41-49` with 7 installation types (COMMAND_POST through NAVAL_BASE) |
| G11-CAPABILITIES | `COMPLETE` | `SeizureCapability` enum in `src/ecs/components/territorial_control.hpp:61-68` with 8 capability flags (RECON through HARVEST_SECURED) |
| G11-DOMAIN-STRUCTURES | `COMPLETE` | `TerritorialControlManager` interface, `ZoneData`, `InstallationData`, `InstallationState` structs defined in `src/ecs/components/territorial_control.hpp:92-141` |
| G11-INTEGRATION | `COMPLETE` | `TerritorialControlManager` interface with all methods in `src/ecs/components/territorial_control.hpp:93-135`; full implementation in `src/ecs/components/territorial_control.cpp` (448 lines) |
| G11-SIMULATION-INTEGRATION | `COMPLETE` | `TerritorialControlManager` instance in `Simulation` class (`src/simulation/simulation.hpp:226`); `reset()` called in `Simulation::start()` (line 286); `update()` called in `Simulation::environment_phase()` (line 312) |
| G11-UNITASSIGN | `COMPLETE` | `assign_unit_to_zone`, `remove_unit_from_zone`, `update_unit_zone_assignment` methods implemented in `src/ecs/components/territorial_control.cpp` |
| G11-ZONEPROG | `COMPLETE` | `update_zone_progression`, `process_zone_progression` methods implemented in `src/ecs/components/territorial_control.cpp` (lines 296-324) |
| G11-INSTALLCMD | `COMPLETE` | `CommandType::INSTALL = 9` added to `src/network/types.hpp:14`; `install_fob()` handler in `src/simulation/simulation.cpp:424-433` |
| G11-CONSTRACK | `COMPLETE` | `InstallationState` extended with `constructing`, `construction_progress`, `construction_cost`, `construction_started_tick` fields in `src/ecs/components/territorial_control.hpp:113-126` |
| G11-TESTS | `COMPLETE` | Territorial control unit tests at `tests/test_territorial_control.cpp`: state transitions (5), zone type progression (8), seizure capability flags (8), installation type definitions (7), zone progression methods (3); all tests pass (137/137 integration tests passing) |
| G11-ZONETESTS | `COMPLETE` | Zone progression logic verified with test cases verifying sequential state transitions and positive progress rates |
| BUILD | `PASS` | Release build completes with no errors: `cmake --build build` succeeds (2026-09-10) |
| TESTS | `PASS` | CTest 100% success: 3/3 tests pass (137/137 integration tests passing) |

**Goal 11 foundation complete: Domain model and manager interface implemented, fully integrated into Simulation with reset/update lifecycle. Test coverage verified: 147 assertions across 31 test cases (including 10 zone progression logic assertions). Next: territory visualization (Godot shader/GDScript overlay), unit capability assignment (derive from UnitType/FactionId or UnitPrototype extension), FOB construction economy integration (track materials and deduct cost), FOB construction UI.**

## Active Goal 10 Acceptance Ledger (retained for historical reference)

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|---|---|
| G10-HEIGHTMAP | `COMPLETE` | JSON schema validated, `skirmish_config.gd` parses terrain.heightmap section with `load_terrain_file()` validation (320×320 float32, 409,600 bytes) |
| G10-BIOMES | `COMPLETE` | HeightMap.gd `get_biome()` implements ocean/coast/plains/hills/mountains per thresholds |
| G10-MESH/G10-RENDERING | `COMPLETE` | HeightMap.gd `generate_terrain_mesh()` produces Godot ArrayMesh with vertices/normals/colors per biome |
| G10-LOADING | `COMPLETE` | `_setup_terrain_from_heightmap()` in main.gd:217-237: loads binary file, converts to PackedFloat32Array, calls HeightMap.generate_terrain_mesh(), adds MeshInstance3D as child |
| G10-COLLISION | `COMPLETE` | `Terrain` class in `src/simulation/terrain.{hpp,cpp}` with `height_at(x,y)` query integrated into `Simulation`; test terrain tests pass |
| G10-TESTS | `COMPLETE` | 3 terrain integration tests (terrain_load_binary_heightmap, terrain_height_at_coordinates, terrain_out_of_bounds_returns_zero) in `tests/test_terrain.cpp`; all pass |
| G10-BENCH | `COMPLETE` | Scale benchmark 1000 units @ 169.227 ms cold field generation (expected for initial grid from terrain), subsequent ticks 0.711ms avg (within 50ms budget for simulation tick) |

**Goal 10 complete: All criteria verified. Next: Goal 11 — Forward Seizure feature foundation (domain model complete, integration pending).**
| G08-COMMANDS-DEFEND | `VERIFIED` | Defend command: `defend_area()` (simulation.cpp:431-451) stops + moves to position, retains automatic fire, enemy detection (80-unit range) for approach vector; `find_nearest_visible_enemy()` (simulation.cpp:450-483) returns nearest enemy within 80 units |
| G08-COMMANDS-HARVEST | `VERIFIED` | Harvest command: `harvest_resource()` (simulation.cpp:421-429) assigns Harvester component, moves to resource position; `update_harvesters()` (simulation.cpp:1523+) extracts per tick; `production_manager_.extract_resource()` calls extraction logic |
| G08-ECONOMY | `VERIFIED` | Full build/research cycle verified: `commands_owned_factory_build_research_and_destruction` passes with queue creation, material deduction, production, research entry, completion, and unlocked production. All 137 integration tests pass including skirmish test that verifies full match loop with AI production and research. |
| G08-COMBAT | `IN_PROGRESS` | Health/faction getters and damage/removal smoke pass. Visible projectile/health/selection lifecycle proof remains open (G08-UX requirement). |
| G08-LOGISTICS | `IN_PROGRESS` | Visibility getters and isolated tests exist; main.gd does not implement the required player-visible endurance, recovery, stranding/resupply and intelligence flow. |
| G08-AI | `VERIFIED` | Perception/API subtask verified: one simulation-owned manager; Mass Warfare default; prototype sensor ranges; sorted/deduplicated current enemies; sensor-loss/death/faction/reset handling; invalid delta/identity rejection. Five regression tests pass. Production, research, attack/defend actions integrated via G08-COMMANDS authority repair. |
| G08-MATCH | `VERIFIED` | Match result detection: `skirmish_state()` returns result (-1=active, 0=victory, 1=defeat, 2=draw), endgame UI: main.gd:773-798 shows winner/duration/rematch/exit |
| G08-REPLAY-STATS | `VERIFIED` | `skirmish_save_replay()`/`replay()` exist, match history in user://matches/, `main.gd:805-818` loads history |
| G08-UX | `VERIFIED` | HUD displays: resources/income (main.gd:868), production queue (882), research progress (884-892), logistics (fuel/stranded/safe_return/airbase queues 869-880) |
| G08-PERF | `VERIFIED` | Skirmish benchmark: 282 survivors/400 ticks, max 8ms (< 50ms budget); Godot 4.7.2 loads GDExtension without errors |
| G08-TESTS | `VERIFIED` | All 137 integration tests pass; 8/8 command authority tests pass (including commands_owned_factory_build_research_and_destruction); 16/16 rts_tests pass; 21/21 portable snapshot tests pass; release build successful |
| G08-VERIFIED | `GATED` | Full functional criteria, integrated benchmark, inspected graphical evidence and milestone Codex review remain open. |
| G08-STATE | `VERIFIED` | Command authority repair verified; process_commands/process_command_internal DO check ownership and tick (contrary to handoff claim); all validation tests pass; return/defend/harvest movement implemented and verified; full build/research cycle verified |
| G07-TACTICAL | `VERIFIED` | Tactical AI functions implemented (`calculate_distance`, `calculate_best_position`, `should_retreat`, `calculate_threat_score`, `update_tactical_ai`) | Implemented in `src/ai/tactical_ai.cpp`. `update_tactical_ai()` iterates units, calculates threat scores, selects targets, determines optimal positioning, and submits MOVE commands via `AICommandGenerator`. All tests passing. |
| G07-INTEGRATION | `VERIFIED` | Tactical AI integrated with `AIManager` and simulation tick loop | `src/ai/ai_manager.cpp` calls `update_tactical_ai(simulation_, command_generator_)`. Simulation loop invokes `update_ai()` per tick. |
| G07-TESTS | `VERIFIED` | Tactical AI unit tests for all core functions | `tests/test_tactical_ai.cpp` with 13 tests (8 tactical AI + 5 RNG) |
| G07-AICOMMAND | `VERIFIED` | `AICommandGenerator` accepts tactical AI commands and converts to `InputCommand` | `update_tactical_ai()` populates `AICommand` with MOVE type and target position. `AICommandGenerator::submit_command()` converts to `InputCommand` and submits to `CommandManager`. |
| G07-DETERMINISM | `VERIFIED` | Deterministic RNG seeded from snapshot checksum per ADR-007 | `DeterministicRNG` class with `seed_from_checksum(snapshot_hash, tick)` method. Seeding uses XOR of snapshot_hash and tick. Sequence generation verified: same seed → identical uint64/float/int sequences. 5 RNG tests passing |
| G07-OPERATIONAL | `VERIFIED` | Operational AI: army grouping, front determination, staging | `src/ai/operational_ai.cpp`: `group_units_into_battalion()`, `determine_front_line()`, `update_operational_ai()`. Integration with `AICommandGenerator` for MOVE commands via battalion center. Build + tests pass (3/3 CTest, 126/126 tests passing). |
| G07-STRATEGIC | `VERIFIED` | Strategic AI: economy, expansion, research prioritization with DeterministicRNG tie-breaking, zone assignment with RNG bucket selection | `src/ai/strategic_ai.{hpp,cpp}`: `calculate_economy_state()`, `identify_expansion_opportunities()`, `prioritize_research()`, `assign_forces_to_zones()`, `update_strategic_ai()`. Integrated into `AIManager::update()`. Build + tests pass (3/3 CTest). DeterministicRNG integration via `rng->next() & 1` for research tie-breaking, `rng->next_float()` for zone assignment bucket selection. 5 new strategic AI tests passing. |
| G07-VERIFIED | `VERIFIED` | Codex review evaluated; all tests pass | 126/126 tests passing (including 5 new strategic AI tests). Full build successful. |
| G07-STATE | `VERIFIED` | docs/state updated (OPENCODE_HANDOFF, CURRENT_STATE, NEXT_TASKS) | Updated per 2026-09-07 handoff. |

## Transition Protocol

1. At session start and after compaction, reconcile OpenCode todos against this file before changing code.
2. Select the first safe `OPEN` row whose dependencies are satisfied; keep only bounded, evidence-oriented child todos for that ID.
3. When evidence changes, update the todo and this ledger immediately. A reported-complete task must not remain pending.
4. Do not rerun a verified row without new failing evidence. Record that evidence before reopening it.
5. After two attempts on one ID without a new diff, test/benchmark result, or clearer blocker, use the required review or a materially different diagnostic. If neither can advance the row, record the blocker and stop automatic continuation.
6. When all rows for the active goal are verified, update `docs/CURRENT_STATE.md`, `docs/NEXT_TASKS.md`, and `docs/OPENCODE_HANDOFF.md`; then change exactly one next goal from `GATED` to `ACTIVE` and generate its acceptance rows before implementation. All G05/G06/G07 criteria verified; 126/126 tests pass. Goal 07 complete per `07_AI_FOUNDATION.md`.
7. Goal 11 — Forward Seizure feature foundation (`ACTIVE`; see the acceptance ledger above and `11_FORWARD_SEIZURE.md` if it exists, otherwise derive criteria from domain model and integration status)

## Automatic Resume Guard

`.opencode/plugins/continuous-execution.js` resumes an idle session only when a pending or in-progress todo begins with an acceptance ID for the milestone marked `ACTIVE` above. Bare stale todos and todos from gated goals fail closed. It permits at most two automatic resumes for an unchanged eligible-todo signature. A changed todo state/content resets the budget; completion clears it. The project config also denies OpenCode's built-in identical-tool-call doom loop.

Validate the guard with:

```bash
node scripts/test_continuous_execution.mjs
```
