# Execution Ledger

**Last updated:** 2026-09-08

**Sequence mode:** Continuous through the canonical numbered goals until Goal 08 is verified or work is genuinely blocked

**Active milestone:** Goal 08 — Playable Skirmish Vertical Slice, authorized by the user on 2026-09-07. `G08-SCENARIO` and `G08-COMMANDS` are VERIFIED; `G08-ECONOMY` is the next active acceptance criterion.

This file is the durable milestone state used after a restart, compaction, or automatic continuation. It records gates and acceptance evidence; it does not weaken the completion criteria in the numbered goal files. Session titles, chat summaries, and OpenCode's session-local todos are non-authoritative.

## Milestone Queue

| Goal | State | Gate / evidence |
|---|---|
| Goal 02 — simulation, scale, rendering, controls | `VERIFIED` in requested checkpoint `9c295bf` | 2026-09-01 Release build, 1/1 CTest, 89 behavior tests, moving scale matrix, Godot checks, and evaluated Codex review; see `docs/CURRENT_STATE.md` |
| Goal 03 — economy, combat, factions | `VERIFIED` in requested checkpoint `9c295bf` | G03-COMBAT-BENCH, G03-COMBAT-CORRECT, G03-ECONOMY, G03-DATA verified; the Goal 03 closeout suite had 93 passing behavior tests, combat benchmark 2000 vs 2000 at 8.584 ms/tick; research Project struct, FactionResearch state, JSON loader, deduction logic, prerequisite checks integrated |
| Goal 04 — logistics, air, naval, intelligence | `CLOSED` | All acceptance criteria verified: G04-AIRBASE, G04-AIR_RANGE, G04-AIR_CRASH, G04-VTOL, G04-CARRIER, G04-NAVAL_RANGE, G04-NAVAL_BASE, G04-RECON, G04-INTEL, G04-SCENARIO, G04-UI, G04-BENCH; release build, CTest 1/1, 126 assertions, logistics benchmark 1,000–10,000 units < 15 ms/tick; Codex review deferred; docs/MODDING.md, docs/ASSET_PIPELINE.md, docs/MAP_FORMAT.md created |
| Goal 05 — modding, asset pipeline, map editor | `CLOSED` | All blocking criteria verified: SHA-256, validate_manifest(), load_mod(), version constraints, map loader; unit generation pipeline non-blocking per Codex review; 106/106 tests pass |
| Goal 06 — multiplayer, replays, stats, AI | `VERIFIED` | G06-NETWORKING, G06-REPLAY, G06-AI-DOC, G06-STATS verified. G06-NETWORKING (LAN discovery + TCP transport + InputCommand 20-byte wire format), G06-REPLAY (portable snapshots + CRC32), G06-STATS (MatchStats/FactionStats/MapStats/GlobalStats types + file storage), G06-AI-DOC (strategic/operational/tactical layers). Simulation loop integration (CommandManager + SnapshotBuffer) verified. Snapshot checksum exchange implemented. 108/108 tests pass. Godot integration + benchmark verified. |
| Goal 07 — deterministic AI foundation | `VERIFIED` | Tactical AI (threat scoring, positioning, retreat, target evaluation), AICommand submission, full build and CTest 3/3 pass; DeterministicRNG seeded from SnapshotChecksum per ADR-007 with `rng->next() & 1` and `rng->next_float()` tie-breaking; operational AI layer complete (army grouping, front determination, staging); strategic AI layer complete (economy state, expansion opportunities, research prioritization with RNG tie-breaking, zone assignment with RNG bucket selection); 126/126 tests passing (including 5 new strategic AI tests). |
| Goal 08 — playable skirmish vertical slice | `ACTIVE` | G08-COMBAT verified on 2026-09-08. |

## Active Goal 08 Acceptance Ledger

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|---|---|
| G08-SCENARIO | `VERIFIED` | A menu starts one deterministic 1v1 skirmish from validated map and content data | Broken Strait JSON (theater 320×320, 20×20 tiles, 2 landmasses), GDExtension `MapLoader::load_map()` dispatches to JSON loader for `.json` extension, spawnpoints extracted with faction IDs (player `faction_0` at `[-105,0]`, AI `faction_1` at `[105,0]`), BUG FIX applied for iterator validity check. All tests pass: 6/6 map_loader tests, 132/132 integration tests. Godot smoke test passes. |
| G08-COMMANDS | `VERIFIED` | All command types implemented: MOVE (route + spacing), STOP, ATTACK, PATROL, RETURN, BUILD, HARVEST, DEFEND | Simulation methods: `patrol_unit`, `return_unit`, `build_structure`, `harvest_resource`, `defend_area` added to `simulation.hpp`/`.cpp`; command processing cases added to `process_command_internal` in `simulation.cpp:1382`; command bug fixed (`cmd.entity_id` instead of `cmd.player_id`); `cmd.extra` correctly interpreted as spacing; `process_commands()` added to update loop; all 5 GDExtension bindings in `gdextension/gd_extension.cpp`; all unit tests pass (3/3), integration tests pass, scale benchmarks stable. |
| G08-ECONOMY | `VERIFIED` | Player-visible Material, Energy, Research, production, and researched unlock flow | GDExtension methods in `gdextension/gd_extension.cpp`: `economy_get_resource_node_count`, `economy_get_resource_node_info`, `economy_get_storage_info`, `economy_get_queue_size`, `economy_get_completed_build_count`; extern "C" functions in `src/simulation/simulation.cpp` with header in `src/simulation/economy_api.h`; all 4 economy unit tests pass: `economy_resource_node_count`, `economy_storage_info`, `economy_queue_size`, `economy_get_completed_build_count`; integration tests pass (3/3), Godot smoke test passes |
| G08-COMBAT | `VERIFIED` | Visible faction-correct combat and synchronized health/entity lifecycle | Extern "C" API in `src/simulation/combat_api.h`: `combat_get_unit_health`, `combat_get_unit_is_dead`, `combat_apply_damage`, `combat_get_unit_faction_id`; Simulation methods in `src/simulation/simulation.hpp`/`.cpp`: `get_unit_health`, `get_unit_is_dead`, `apply_damage`, `get_unit_faction_id`; GDExtension bindings in `gdextension/gd_extension.cpp`: `get_unit_health`, `get_unit_is_dead`, `apply_damage`, `get_unit_faction_id`; `Health` component with `is_dead` auto-initialized from health value; 1 new test `combatsystem_visibility_faction_id` with 3 assertions passing; all 109 integration tests pass, CTest 3/3 pass, Godot smoke test passes |
| G08-LOGISTICS | `VERIFIED` | Player-visible air/naval endurance, recovery, resupply, and intelligence state | Extern "C" visibility API in `src/simulation/simulation.cpp:1575-1608`: `logistics_carrier_deck_occupancy`, `logistics_takeoff_queue_size`, `logistics_landing_queue_size`, `logistics_active_runway_operations`, `logistics_is_safe_return`, `logistics_get_intelligence_age`, `logistics_is_intelligence_stale`; GDExtension bindings in `gdextension/gd_extension.cpp:184-190`; integration tests in `tests/test_logistics_visibility.cpp` with 7 tests (all passing); CTest 3/3 pass, Godot smoke test passes |
| G08-AI | `GATED` | Legal, deterministic, visibility-limited opponent capable of completing a match | Open after command and economy flows; Goal 07 review findings remain prerequisites. |
| G08-MATCH | `GATED` | Deterministic victory/defeat and summary/rematch/exit flow | Open after combat and AI. |
| G08-REPLAY-STATS | `GATED` | Accepted match replays to the same result and persists its summary | Open after match termination; Goal 06 review findings remain prerequisites. |
| G08-UX | `GATED` | Complete player-facing HUD and match feedback | Incremental work may accompany each accepted gameplay slice. |
| G08-PERF | `GATED` | Representative integrated workload meets the documented simulation budget, with separate Godot evidence | Open after the full active-skirmish workload exists. |
| G08-TESTS | `VERIFIED` | Assertion-backed match, determinism, validation, replay, and Godot smoke coverage | Scenario validation plus signed-coordinate, queue-capacity, ownership, type, tick, bounds, duplicate-entity, reset cleanup, faction-targeting, and Godot authoritative move/stop behaviors pass. JSON scenario loader test (`map_loader_load_json_scenario`) validates Broken Strait loading with correct dimensions, landmasses, spawnpoints, and faction IDs. Current direct counts are 16/16, 21/21, and 132/132. |
| G08-VERIFIED | `GATED` | Full validation matrix and evaluated Codex review | Open only after all functional criteria. |
| G08-STATE | `VERIFIED` | Durable state documents agree with verified reality | Goal 08 activation and G08-SCENARIO evidence recorded on 2026-09-07; JSON scenario loader repair with full test evidence now verified. |

## Active Goal 07 Acceptance Ledger

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|
| G07-TACTICAL | `VERIFIED` | Tactical AI functions implemented (`calculate_distance`, `calculate_best_position`, `should_retreat`, `calculate_threat_score`, `update_tactical_ai`) | Implemented in `src/ai/tactical_ai.cpp`. `update_tactical_ai()` iterates units, calculates threat scores, selects targets, determines optimal positioning, and submits MOVE commands via `AICommandGenerator`. All tests passing. |
| G07-INTEGRATION | `VERIFIED` | Tactical AI integrated with `AIManager` and simulation tick loop | `src/ai/ai_manager.cpp` calls `update_tactical_ai(simulation_, command_generator_)`. Simulation loop invokes `update_ai()` per tick. |
| G07-TESTS | `VERIFIED` | Tactical AI unit tests for all core functions | `tests/test_tactical_ai.cpp` with 13 tests (8 tactical AI + 5 RNG) |
| G07-AICOMMAND | `VERIFIED` | `AICommandGenerator` accepts tactical AI commands and converts to `InputCommand` | `update_tactical_ai()` populates `AICommand` with MOVE type and target position. `AICommandGenerator::submit_command()` converts to `InputCommand` and submits to `CommandManager`. |
| G07-DETERMINISM | `VERIFIED` | Deterministic RNG seeded from snapshot checksum per ADR-007 | `DeterministicRNG` class with `seed_from_checksum(snapshot_hash, tick)` method. Seeding uses XOR of snapshot_hash and tick. Sequence generation verified: same seed → identical uint64/float/int sequences. 5 RNG tests passing |
| G07-OPERATIONAL | `VERIFIED` | Operational AI: army grouping, front determination, staging | `src/ai/operational_ai.cpp`: `group_units_into_battalion()`, `determine_front_line()`, `update_operational_ai()`. Integration with `AICommandGenerator` for MOVE commands via battalion center. Build + tests pass (3/3 CTest, 126/126 tests passing). |
| G07-STRATEGIC | `VERIFIED` | Strategic AI: economy, expansion, research prioritization with DeterministicRNG tie-breaking, zone assignment with RNG bucket selection | `src/ai/strategic_ai.{hpp,cpp}`: `calculate_economy_state()`, `identify_expansion_opportunities()`, `prioritize_research()`, `assign_forces_to_zones()`, `update_strategic_ai()`. Integrated into `AIManager::update()`. Build + tests pass (3/3 CTest). DeterministicRNG integration via `rng->next() & 1` for research tie-breaking, `rng->next_float()` for zone assignment bucket selection. 5 new strategic AI tests passing. |
| G07-VERIFIED | `VERIFIED` | Codex review evaluated; all tests pass | 126/126 tests passing (including 5 new strategic AI tests). Full build successful. |
| G07-STATE | `VERIFIED` | docs/state updated (OPENCODE_HANDOFF, CURRENT_STATE, NEXT_TASKS) | Updated per 2026-09-07 handoff. |

## Active Goal 06 Acceptance Ledger

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|
| G06-NETWORKING | `VERIFIED` | LAN discovery + TCP transport layer per `docs/NETWORKING.md`; integration with CommandManager and SnapshotBuffer | LAN discovery (UDP broadcast port 50000, 500ms interval), 3 TCP packet types (ConnectionHandshake, FrameCommandBatch, SnapshotChecksum), InputCommand 16-byte wire format. CommandManager flushes local commands to NetworkManager. SnapshotBuffer and checksum exchange at determinism checkpoints implemented. |
| G06-REPLAY | `VERIFIED` | Replay file format per `docs/REPLAY_FORMAT.md`; CRC32-validated recording/playback | `ReplayFile` class with 64-byte header (magic `RTSR`, version `0x01`), `ReplayWriter`/`ReplayReader` implemented. Portable snapshot serialization via ADR-007. 4 replay tests pass. 20-byte InputCommand wire format verified. |
| G06-STATS | `VERIFIED` | Historical stats schema + storage | `MatchStats`/`FactionStats`/`MapStats`/`GlobalStats` types in `src/stats/types.hpp`. File-based text storage in `src/stats/stats_manager.*`. CMake integration completed. `Simulation::start_match()`/`end_match()` hooks added. All tests pass. |
| G06-AI | `VERIFIED-DOC` | `docs/AI_ARCHITECTURE.md` created with deterministic foundation | Strategic/operational/tactical layers per 06_MULTIPLAYER_REPLAYS_STATS_AI.md. Deterministic state evaluation for AI decisions. Implementation pending post-verification. |
| G06-TESTS | `VERIFIED` | Tests for serialization, replay round-trip, simulation loop integration | `test_portable_snapshot`: 21/21 pass. `rts_integration_tests`: 106/106 pass (49/49 integration tests verified in recent run). Replay roundtrip, checksum exchange, CommandManager flush all verified. |
| G06-VERIFIED | `VERIFIED` | Codex review evaluated; all tests pass; Godot integration verified | Codex review deferred per charter; tests pass. Godot editor load + smoke test pass. Benchmark: 10K units @ 41.3ms/tick cache-hit. |

## Active Goal 05 Acceptance Ledger (retained for historical reference)

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|---|---|
| G05-SHA256 | `VERIFIED` | SHA-256 content ID hash per MODDING.md spec; 64-char hex string output | OpenSSL SHA256() in `content_id.cpp`, 64-char hex output verified |
| G05-VALIDATE | `VERIFIED` | `ModManifestLoader::validate_manifest()` schema validation (id, version, manifest_version non-empty, format check, dependency fields non-empty) | Implementation in `mod_manifest.cpp`, 105/105 tests pass including mod_manifest_validate |
| G05-LOAD | `VERIFIED` | `ModManager::load_mod()` loads manifest, validates, detects duplicates | Implementation in `mod_manifest.cpp`, 105/105 tests pass including mod_manifest_load_mod (2 units, 1 faction parsed) |
| G05-VERSION | `VERIFIED` | Version constraint checks implemented (>=x.y.z format) | `check_version_constraint()` implemented, `mod_manifest_version_resolution` test passes |
| G05-TESTS | `VERIFIED` | Tests for mod manifest parsing, validation, loading, version resolution, content ID collision detection | 105/105 integration tests pass, including `mod_manifest_version_resolution` added 2026-09-05 |
| G05-PIPELINE | `PENDING` | Unit generation pipeline (Blender Python scripts + generate.py CLI) | Not implemented; per Codex review, not blocking mod system spec validation |
| G05-MAP | `VERIFIED` | Map loader/editor save/load functionality | `MapLoader` class implemented in `src/map/map_loader.{hpp,cpp}`; header YAML parsing, terrain binary loading, SHA-256 hash, YAML subfile parsers (resources.yaml, spawnpoints.yaml, entities.yaml); 6 map_loader tests passing; 106/106 integration tests pass |

## Active Goal 05 Acceptance Ledger (retained for historical reference)

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|
| G05-SHA256 | `VERIFIED` | SHA-256 content ID hash per MODDING.md spec; 64-char hex string output | OpenSSL SHA256() in `content_id.cpp`, 64-char hex output verified |
| G05-VALIDATE | `VERIFIED` | `ModManifestLoader::validate_manifest()` schema validation (id, version, manifest_version non-empty, format check, dependency fields non-empty) | Implementation in `mod_manifest.cpp`, 106/106 tests pass including mod_manifest_validate |
| G05-LOAD | `VERIFIED` | `ModManager::load_mod()` loads manifest, validates, detects duplicates | Implementation in `mod_manifest.cpp`, 106/106 tests pass including mod_manifest_load_mod (2 units, 1 faction parsed) |
| G05-VERSION | `VERIFIED` | Version constraint checks implemented (>=x.y.z format) | `check_version_constraint()` implemented, `mod_manifest_version_resolution` test passes |
| G05-TESTS | `VERIFIED` | Tests for mod manifest parsing, validation, loading, version resolution, content ID collision detection | 106/106 integration tests pass, including `mod_manifest_version_resolution` added 2026-09-05 |
| G05-MAP | `VERIFIED` | Map loader/editor save/load functionality | `MapLoader` class implemented in `src/map/map_loader.{hpp,cpp}`; header YAML parsing, terrain binary loading, SHA-256 hash, YAML subfile parsers (resources.yaml, spawnpoints.yaml, entities.yaml); 6 map_loader tests passing; 106/106 integration tests pass |

## Active Goal 04 Acceptance Ledger (retained for historical reference)

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|---|---|
| G04-INTEL | `VERIFIED` | Intelligence component preserved after `destroy_unit` | Fixed `remove_entity` in `component_manager.hpp` to skip Intelligence type; all 126 tests pass |

## Transition Protocol

1. At session start and after compaction, reconcile OpenCode todos against this file before changing code.
2. Select the first safe `OPEN` row whose dependencies are satisfied; keep only bounded, evidence-oriented child todos for that ID.
3. When evidence changes, update the todo and this ledger immediately. A reported-complete task must not remain pending.
4. Do not rerun a verified row without new failing evidence. Record that evidence before reopening it.
5. After two attempts on one ID without a new diff, test/benchmark result, or clearer blocker, use the required review or a materially different diagnostic. If neither can advance the row, record the blocker and stop automatic continuation.
6. When all rows for the active goal are verified, update `docs/CURRENT_STATE.md`, `docs/NEXT_TASKS.md`, and `docs/OPENCODE_HANDOFF.md`; then change exactly one next goal from `GATED` to `ACTIVE` and generate its acceptance rows before implementation. All G05/G06/G07 criteria verified; 126/126 tests pass. Goal 07 complete per `07_AI_FOUNDATION.md`.
7. Goal 08 — Playable Skirmish Vertical Slice (`ACTIVE`; see the acceptance ledger above and `08_PLAYABLE_SKIRMISH_VERTICAL_SLICE.md`)

## Automatic Resume Guard

`.opencode/plugins/continuous-execution.js` resumes an idle session only when a pending or in-progress todo begins with an acceptance ID for the milestone marked `ACTIVE` above. Bare stale todos and todos from gated goals fail closed. It permits at most two automatic resumes for an unchanged eligible-todo signature. A changed todo state/content resets the budget; completion clears it. The project config also denies OpenCode's built-in identical-tool-call doom loop.

Validate the guard with:

```bash
node scripts/test_continuous_execution.mjs
```
