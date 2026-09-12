# Execution Ledger

**Last updated:** 2026-09-12

**Sequence mode:** reconciliation complete; Goals 03 and 04 are verified and Goal 05 is ACTIVE. Goals 06–11 are GATED.

**Milestone state:** Goals 02–04 have current local validation evidence. Goal 05 is now the active acceptance gate; candidate later-goal implementation cannot bypass it. See `docs/WORKTREE_RECONCILIATION.md`.

This file is the durable milestone state used after a restart, compaction, or automatic continuation. It records gates and acceptance evidence; it does not weaken the completion criteria in the numbered goal files. Session titles, chat summaries, and OpenCode's session-local todos are non-authoritative.

## Reconciliation Authority — 2026-09-12

This section supersedes contradictory historical status claims below. It is based on the current repository object graph and fresh local validation:

- `9c295bf` is absent; claims tied to it are historical and non-verifiable here.
- Release build, CTest (3/3), direct native runners (18/18, 21/21, 164/164), Godot smoke, Godot presentation (91/91), and the four repository-owned validation scenarios pass on the current dirty tree.
- `G03-COMBAT-BENCH` passes: `rts_combat_benchmark 2000 100` reports 4,000 initial units, 2,000 destroyed, 1,038 projectiles, changed state hash, and 11.058 ms cache-hit average (2026-09-12).
- `G04-BENCH` passes: `rts_logistics_benchmark 10000 30 100` measures 10,000 airborne conventional aircraft, 30 moving carriers, safe-return/facility cache behavior, and intelligence updates at 0.951 ms cache-hit average (p95 1.488 ms; 2026-09-12). It is isolated from prediction/combat/economy/snapshot timing by design.
- No canonical Goal 09 or Goal 11 specification exists at repository root. The Goal 06 and Goal 08 specifications also contain explicit TODO/in-progress gates that contradict their historical closure claims.

## Milestone Queue

Historical Goal 04–11 labels below are retained as implementation reports, not fresh sign-off. They must not be used to advance the milestone sequence.

| Goal | State | Gate / evidence |
|---|---|
| Goal 02 — simulation, scale, rendering, controls | `HISTORICAL` | Functional scale harnesses remain available, but historical performance rows are not current evidence for this dirty checkout. |
| Goal 03 — economy, combat, factions | `VERIFIED` | `G03-COMBAT-BENCH` passes with meaningful target acquisition, projectile fire, destruction, state evolution, and a cache-hit average under the 12 ms gate. |
| Goal 04 — logistics, air, naval, intelligence | `VERIFIED` | All 14 required behaviors map to current deterministic assertions; focused review and remediation are recorded in `docs/LOGISTICS_ARCHITECTURE_REVIEW.md`; Release/CTest/direct/benchmark evidence passed 2026-09-12. |
| Goal 05 — modding, asset pipeline, map editor | `ACTIVE` | Generated-unit development spawning, native map save/load, and dependency/version enforcement are verified: map terrain/resources/spawns/entities round-trip; a test mod's unit/mesh pair spawns with authored data; missing/incompatible `>=` dependencies reject; batch loading is deterministic, dependency-first, and staged atomically (170 direct assertions; 2026-09-12). Editor surface and required review remain. |
| Goal 06 — multiplayer, replays, stats, AI | `GATED` | Its own numbered specification marks all acceptance criteria TODO. |
| Goal 07 — deterministic AI foundation | `GATED` | Candidate implementation exists, but it cannot close while earlier goals are gated. |
| Goal 08 — playable skirmish vertical slice | `GATED` | Its specification retains in-progress rows and requires complete-flow visual/replay/stat evidence. |
| Goal 09 — logistics improvements | `UNSPECIFIED` | No canonical root goal document exists. |
| Goal 10 — terrain system | `GATED` | Its specification retains pending roads and terrain-collision criteria. |
| Goal 11 — Forward Seizure feature foundation | `UNSPECIFIED` | Candidate dirty-tree implementation and tests exist, but no canonical root goal document exists. |
| Goal 11-TESTS | `COMPLETE` | Territorial control unit tests at `tests/test_territorial_control.cpp`: state transitions (5), zone type progression (8), seizure capability flags (8), installation type definitions (7); all tests pass |
| GOAL-11-NEXT | `COMPLETE` | FOB construction completion notification is rendered and covered by the Godot presentation harness |
| VAL-FRAMEWORK | `COMPLETE` | Cross-platform test/validation commands, explicit scenario registry, bounded Godot runner, schema-checked reports, logs, and non-zero failure propagation implemented |
| VAL-SCENARIOS | `COMPLETE` | Selection/move, strategic zoom, airfield/fighter ferry, and 1,000-unit benchmark scenarios pass; forced-failure registry run returns non-zero and all reports say FAIL |
| VAL-VISUAL | `COMPLETE` | Inspected 1280x720 strategic checkpoints, 1.000000 repeat score, calibrated 0.995 references, corrupted-reference 0.445200 FAIL, and generated diffs verified |
| VAL-VIDEO | `COMPLETE` | Airfield/fighter scenario records an automatic fixed-step 1280x720 30 FPS AVI; final proof is 149 frames and metadata/non-empty checks pass |
| VAL-CONTRACT | `COMPLETE` | AGENTS gameplay completion contract and validation workflow/baseline/catalog/report documentation added |
| VAL-REVIEW | `COMPLETE` | Independent boundary review found no false-PASS or production-path blocker; final `tools/test` passed and `docs/validation/CODEX_VALIDATION_REVIEW.md` ends ACCEPT |

## Historical Candidate Goal 11 Evidence

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
| G11-CAPABILITYASSIGN | `COMPLETE` | All current unit prototypes author capability arrays in `data/unit_faction_stats.json`; `Simulation::create_unit_with_type()` assigns them and `unit_capabilities_are_assigned_from_prototypes` passes |
| G11-CONSTRUCTION | `COMPLETE` | FOB installations start inactive, advance deterministically over 10 seconds through `TerritorialControlManager::update()`, and contribute no bonuses before completion; `fob_construction_progress_completes_deterministically` passes |
| G11-UI | `COMPLETE` | `8`-then-left-click placement shows authoritative FOB assembly progress and the `FOB ONLINE  //  LOGISTICS LINK ESTABLISHED` completion notification; Godot presentation harness passes 39/39 |
| G11-TESTS | `COMPLETE` | Territorial control unit tests at `tests/test_territorial_control.cpp`: state transitions (5), zone type progression (8), seizure capability flags (8), installation type definitions (7), zone progression methods (3); all tests pass (137/137 integration tests passing) |
| G11-ZONETESTS | `COMPLETE` | Zone progression logic verified with test cases verifying sequential state transitions and positive progress rates |
| BUILD | `PASS` | Release build completes with no errors: `cmake --build build` succeeds (2026-09-10) |
| TESTS | `PASS` | CTest 100% success: 3/3 tests pass (137/137 integration tests passing) |

**Historical candidate evidence only:** the following records a domain-model and presentation slice present in the dirty tree. It is not a Goal 11 closure because the canonical Goal 11 specification is absent and earlier sequence gates are open.

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
6. When all rows for the active goal are verified, update `docs/CURRENT_STATE.md`, `docs/NEXT_TASKS.md`, and `docs/OPENCODE_HANDOFF.md`; then change exactly one next goal from `GATED` to `ACTIVE` and generate its acceptance rows before implementation. If no canonical next goal document exists, pause after recording the verified milestone.
7. Goal 11 — Forward Seizure feature foundation (`VERIFIED`; no canonical Goal 12 document exists)

## Automatic Resume Guard

`.opencode/plugins/continuous-execution.js` resumes an idle session only when a pending or in-progress todo begins with an acceptance ID for the milestone marked `ACTIVE` above. Bare stale todos and todos from gated goals fail closed. It permits at most two automatic resumes for an unchanged eligible-todo signature. A changed todo state/content resets the budget; completion clears it. The project config also denies OpenCode's built-in identical-tool-call doom loop.

Validate the guard with:

```bash
node scripts/test_continuous_execution.mjs
```
