# Next Tasks — Goal 11 Verified, Forward Seizure Feature Foundation

**Updated:** 2026-09-10. Goal 11 is VERIFIED. Territorial control domain model, unit-to-zone assignment, zone progression tracking, deterministic FOB construction progress, content-authored unit capability assignment, rendered FOB placement/progress, and completion notification are verified.

## Latest Gameplay Integration

The reachable Command Walker catalog includes all six Elite unit prototypes;
fighter, VTOL, and patrol-boat entries are no longer hidden after the first
three cards. Validation is CTest 3/3 and `test_skirmish.gd` with 42 passing
checks, including fighter queue, completion, and presentation registration.

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
