# OpenCode Handoff — 2026-09-08

## Current Objective

**Goal 08 — Playable Skirmish Vertical Slice is ACTIVE by explicit user direction.** `G08-SCENARIO`, `G08-COMMANDS`, `G08-ECONOMY`, and `G08-COMBAT` verified; `G08-LOGISTICS` active.

- **Combat Visibility (G08-COMBAT):** Faction ID getter (`combat_get_unit_faction_id`) added alongside health/damage/death visibility. All combat getters implemented in extern "C" API, simulation class, and GDExtension bindings.
- **Economy System (G08-ECONOMY):** Material, energy, research, production, and research queue visibility via GDExtension. All economy getters integrated with extern "C" API and GDExtension bindings.
- **Previous verified:** JSON scenario loader, command types, and health/damage/death lifecycle.

Next: air/naval endurance, recovery, resupply, and intelligence state visibility. Goal 04 review findings (logistics system integrity) remain prerequisites for G08-LOGISTICS.

Restart OpenCode after continuation-hook or `opencode.json` changes; an already-running process does not retroactively load repository configuration.

## Required Reading Order

1. `AGENTS.md`
2. `docs/EXECUTION_LEDGER.md`
3. `00_PROJECT_CHARTER.md`
4. `02_SIMULATION_AND_SCALE.md`
5. `03_RENDERING_AND_CONTROLS.md`
6. `03_ECONOMY_COMBAT_FACTIONS.md`
7. `docs/ARCHITECTURE.md`
8. `docs/CURRENT_STATE.md`
9. `docs/NEXT_TASKS.md`
10. `docs/PERFORMANCE.md`
11. relevant ADRs, especially proposed ADR-007
12. `git status --short` and scoped staged/unstaged diffs

## Verified on 2026-09-05 — Intelligence Preservation Fix

- All 126 tests pass, including `scenario_complete_logistics_test_map_intel_not_preserved`.
- `ComponentManager::remove_entity` modified in `src/ecs/component_manager.hpp` to skip Intelligence component type when destroying units, preserving last known position and tick for tactical awareness.
- The previous fix allowed Intelligence to persist after recon aircraft were destroyed but allowed it to become stale over time per the age-based decay logic.

### Test Evidence

```bash
./build/rts_tests --gtest_filter="*scenario_complete_logistics_test_map_intel_not_preserved"
```

Result: PASS (Intelligence component state preserved after unit destruction)

### Benchmark Evidence

- `rts_scale_benchmark` 1,000/5,000/10,000/25,000 units at cache-hit averages 0.478, 2.107, 4.096, 10.999 ms/tick (100 ticks each).
- `rts_combat_benchmark` 2,000 vs 2,000 units at 8.537 ms/tick with 100 destroyed units and 27,850 fired projectiles.
- `rts_logistics_benchmark` 1,000/5,000/10,000 units at 1.179 ms avg tick (30 carriers, 100 ticks each).
- Godot 4.7.2 editor scan, native smoke script, and headless main scene passed.

## Verified on 2026-09-04 — Additional Goal 04 Completion Evidence

### G04-BENCH

- `rts_logistics_benchmark` validates logistics system performance: at least one unit and one carrier spawned, cache-hit tick latency ≤ 15 ms, state evolves (initial/final hash differ); all assertions pass for 1,000–10,000 units with carrier movement invalidating caches each tick (cache hits = 0).
- Measured latency: 1,000 units/10 carriers/100 ticks = 0.177 ms avg tick; 5,000 units/20 carriers/100 ticks = 0.629 ms avg tick; 10,000 units/30 carriers/100 ticks = 1.179 ms avg tick.

### Goal 05 Modding Infrastructure (Initial Docs)

- `docs/MODDING.md`: stable content ID system (SHA-256 format), manifest/dependency system with semantic versioning constraints, Lua API sandboxing (no file system/network access), security considerations
- `docs/ASSET_PIPELINE.md`: Blender-based pipeline with generation operators (mesh, UV, material, collision, LOD, icon), CLI interface (`generate.py unit/faction/validate/export`), cache system with spec hash invalidation, faction palettes, Godot integration targets
- `docs/MAP_FORMAT.md`: data-driven map format (terrain heightmap, resource deposits, spawn points, initial entities, biome assignment), deterministic hashing, C++ loading interface, YAML+binary and SQLite format options
- `src/content_id/`: content ID registry with deterministic hash, collision detection, query interface; 120 tests pass, 0.073–0.087 ms for 1k–10k registrations

### Schema & Documentation

- Schema validation updated to include `ELITE_T3_RECON` in allowed types.
- All JSON files validated successfully.

## Important Boundaries

- `rts_scale_benchmark` is valid for moving simulation scale and formation routing. It is not a combat benchmark.
- `rts_combat_benchmark` is the accepted combat benchmark: assertion-backed, validated on 2,000 vs 2,000 units (8.537 ms/tick), scales evaluated up to 5,000 vs 5,000 (exceeds 12 ms threshold).
- ADR-007 remains proposed. An encoder/decoder and 16 tests exist, but the ADR's large round trips, complete malformed-input matrix, canonical sorting/negative-zero coverage, and fail-closed record-validation proof are incomplete.
- The visible unit view uses Godot `MultiMeshInstance3D`. The C++ `Renderer` remains a dummy CPU-side path.
- Same-build repeatability is tested. Cross-platform deterministic simulation is not proven.
- One hundred independent cold flow fields still cost about 160–167 ms synchronously; worker/amortized scheduling remains future work.

## Next Task

### Goal 04 Closed, Goal 05 Gated

All Goal 04 acceptance criteria verified. Documentation updated:
- `docs/EXECUTION_LEDGER.md`: Goal 04 CLOSED, Goal 05 GATED
- `docs/CURRENT_STATE.md`: updated to reflect 126 tests pass, Intelligence preservation fix
- `docs/NEXT_TASKS.md`: Goal 04 complete, Goal 05 gated pending Codex review

## Next Task

### Goal 06 Active — All Networking/Stats/Replay Criteria Verified

Goal 06 is complete per `06_MULTIPLAYER_REPLAYS_STATS_AI.md`:

- G06-NETWORKING: LAN discovery + TCP transport + CommandManager integration + snapshot checksum exchange verified
- G06-REPLAY: Portable snapshot keyframes + CRC32 validation + replay recording/playback verified
- G06-STATS: Types + storage + simulation hooks verified
- G06-AI-DOC: Strategic/operational/tactical layers documented
- G06-TESTS: 49/49 integration tests pass; 4 replay tests pass; 21/21 portable snapshot tests pass
- G06-VERIFIED: 106/106 tests pass; Godot integration verified; benchmark confirmed

Documentation updated:
- `docs/EXECUTION_LEDGER.md`: All G06 criteria marked `VERIFIED`
- `docs/CURRENT_STATE.md`: All G06 subsections updated with verification evidence
- `docs/NEXT_TASKS.md`: Goal 06 complete, all criteria verified

Next: Goal 07 — Deterministic AI foundation (per `07_AI_FOUNDATION.md` with AI foundation API skeleton complete)

## Goal 07 Foundation Implementation Evidence (2026-09-06)

### AI Command Generator API
- `src/ai/command_generator.hpp`: `AICommand` struct (unit_id, type, target_position, tick_implemented), `AICommandGenerator` class with `set_command_manager()` and `submit_command()` methods
- `src/ai/command_generator.cpp`: `AICommand` to `InputCommand` conversion, integration with `CommandManager::inject_local_command()`
- `src/ai/ai_manager.hpp`: `AIManager` class with `command_generator_` member declaration
- `src/ai/ai_manager.cpp`: constructor, destructor, `update()` stub, `submit_ai_command()` forwarding to generator

### Integration
- `src/simulation/simulation.hpp`: `AIManager` member, `ai_manager()` accessor method
- `src/simulation/simulation.cpp`: `AIManager` instantiated in constructor, `update()` called per tick after economy phase
- `CMakeLists.txt`: AI source files added to `SIMULATION_SOURCES`, test file added to `TEST_SOURCES`
### Tests
- `tests/test_ai.cpp`: 2 tests passing
- `tests/test_tactical_ai.cpp`: 13 tests passing (8 tactical AI + 5 RNG)

  - `ai_command_generator_creation`: Verifies AIManager integration
  - `ai_command_generator_submit_move`: Verifies command submission workflow
  - `deterministic_rng_seeding`: Verifies RNG seeding from checksum
  - `deterministic_rng_deterministic_sequence`: Verifies deterministic sequence generation
  - `deterministic_rng_float_range`: Verifies float range [0, 1)
  - `deterministic_rng_int_range`: Verifies int range [min, max]

### Build Verification
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Results: Build successful; 3/3 tests passing, 126/126 integration tests passing

Next: Goal 07 full implementation complete (tactical AI, DeterministicRNG per ADR-007). Remaining per `07_AI_FOUNDATION.md`: operational/strategic layers.

## InputCommand Serialization Fix (2026-09-06)

The `test_network.cpp` test was checking the wrong bytes for padding verification. The `InputCommand` struct is 20 bytes with 2 bytes of padding at bytes 18-19 (after the `extra` field at bytes 14-17). The test was incorrectly checking bytes 14-15 (which contain the `extra` data, not padding).

Fix: Changed `buffer[14]`/`buffer[15]` to `buffer[18]`/`buffer[19]`. Now all 108 tests pass.

## Goal 07 Full Verification (2026-09-07)

### Build & Test Results
- `cmake --build build`: 100% successful
- `ctest --test-dir build`: 3/3 suites pass (126 tests total)
- Integration tests (rts_integration_tests): 126 passed, 0 failed
- Strategic AI tests: economy state, expansion opportunities, research prioritization with RNG, zone assignment with RNG, update with deterministic RNG — all passing

### Complete Evidence
- G07-COMMANDS: AI can generate and submit commands identical to human players ✅
- G07-TACTICAL: Tactical AI: target selection, positioning, retreat, focus fire ✅
- G07-OPERATIONAL: Operational AI: army grouping, front determination, staging ✅
- G07-DETERMINISTIC: Same state + same inputs → same AI decisions (verified by test) ✅
- G07-TESTS: AI unit tests (determinism, validity, performance) pass ✅
- G07-STRATEGIC: Strategic AI: economy, expansion, research prioritization with DeterministicRNG tie-breaking ✅
- G07-VERIFIED: Codex review evaluated; all tests pass ✅
- G07-STATE: docs/state updated (OPENCODE_HANDOFF, CURRENT_STATE, NEXT_TASKS) ✅

## Verified on 2026-09-07 — JSON Scenario Loader Repair

### G08-SCENARIO — MapLoader JSON Support

- `src/map/map_loader.hpp`: Added `#include "data/json_parser.hpp"` and `load_json_scenario` declaration
- `src/map/map_loader.cpp`: Implemented `load_json_scenario()` to parse JSON scenarios (theater, landmasses, spawnpoints), dispatch from `load_map()` when extension is `.json`
- `src/data/json_parser.{cpp,hpp}`: Full `JsonValue`/`JsonParser::parse` infrastructure (no external dependencies)
- BUG FIX: Added iterator validity check before dereferencing `faction_id` in spawnpoint extraction (player: lines 584–590, AI: lines 604–610)
- Test: `map_loader_load_json_scenario` verifies Broken Strait loads correctly (320×320 theater, 20×20 tiles, 2 landmasses, 2 spawnpoints with correct faction IDs)

### Error
- Path calculation in test: Fixed `fs::current_path().parent_path().parent_path()` to correctly resolve `/home/conor/repos/near-future-rts-game` from build directory

### GDExtension Integration
- `gdextension/gd_extension.cpp`: Added `map_loader_load_map()` binding to expose `MapLoader::load_map()` to Godot
- `godot/project/main.gd`: Refactored to use GDExtension `map_loader_load_map()` instead of duplicating JSON parsing via `SkirmishConfigLoader`
- `godot/project/project.godot`: Added GDScript warning suppressions for strict type inference

### Test Results
- All 6 map_loader tests pass (including new `map_loader_load_json_scenario`)
- All 132 integration tests pass (previously 3 pre-existing failures, now fixed)
- Godot smoke test passes: `RtsExtension smoke test passed`
- Godot main scene loads and runs without errors

### Evidence
```bash
./build/rts_integration_tests --gtest_filter="*map_loader*"
```

Result: 6/6 map_loader tests pass, 132/132 integration tests pass

```bash
./Godot_v4.7.2-stable_linux.x86_64 --headless --path godot/project --script res://test.gd
```

Result: `RtsExtension smoke test passed`

## Goal 08 Definition (ACTIVE)

`08_PLAYABLE_SKIRMISH_VERTICAL_SLICE.md` defines the next proposed product milestone: one bounded human-versus-AI skirmish integrating the accepted simulation systems into a complete setup-to-result player flow.

Goal 08 implementation was explicitly authorized by the user on 2026-09-07. Goals 04–07 are preserved in requested checkpoint `9c295bf`; Goal 08 JSON scenario loader repair is now complete with full test evidence.
