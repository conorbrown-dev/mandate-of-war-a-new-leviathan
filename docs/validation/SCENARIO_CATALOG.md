# Validation Scenario Catalog

## `basic_selection_move`

- Purpose: prove a player can locate, select, and move the production Field Engineer.
- Production systems: `main.tscn`, `main.gd` input/terrain picking, `RtsExtension`, authoritative command queue, fixed-tick movement, visual registration.
- Setup/actions: start Broken Strait, focus the camera, send left-button events over the engineer, then right-button events over a nearby terrain point.
- Assertions: engineer exists, click selects it, target is valid land, position changes, destination is reached within 5 m, scenario returns normally.
- Visual checkpoints: `engineer_selected`, `move_order_issued`, `engineer_arrived`.
- Metrics: simulation ticks, moved distance, target error, duration.
- Expected runtime: under 2 seconds headless on the verified development host.
- Run: `tools/validate basic_selection_move`.

## `strategic_zoom_transition`

- Purpose: prove tactical-to-strategic representation and cursor-anchored zoom behavior.
- Production systems: camera input, terrain ray picking, model LOD, screen-space strategic icon overlay, selection state.
- Setup/actions: select the engineer, wheel outward at an off-center cursor through the strategic threshold, then wheel inward at the same cursor.
- Assertions: production wrapper/model exist, strategic distance is reached, tactical model hides, selection persists, terrain anchor error stays within 25 m, scenario returns normally.
- Visual checkpoints: `tactical_model`, `strategic_icon`.
- Metrics: cursor anchor error, camera distance, duration.
- Expected runtime: under 2 seconds headless.
- Run: `tools/validate strategic_zoom_transition`.

## `oak_grove_showcase`

- Purpose: prove the default Broken Strait forest uses the imported oak-pack variety as large terrain-grounded groves.
- Production systems: converted LOD1/LOD2 oak GLBs, visual registry, heightmap placement, MultiMesh batching, deterministic cluster distribution, and tactical/strategic forest LOD.
- Assertions: three active user-supplied GLB forms, 960 trees, configured size variation, a visible imported-mesh height spread, nine groves with all three forms in the showcased cluster, actual mesh-bottom roots embedded 8 cm into the heightmap, and reduced strategic instance population.
- Visual checkpoints: `close_oak_tree`, `tactical_oak_grove`, `strategic_oak_grove`.
- Run: `tools/validate oak_grove_showcase --rendered --require-screenshots --record`.

## `airfield_fighter_ferry`

- Purpose: prove runway aircraft are gated by a tactical airfield and then fly in from outside the 40 km theater.
- Production systems: terrain placement validation, structure queue/completion, territorial airfield registration, build command, cost reservation, aircraft ECS/logistics state, movement, visual registration.
- Setup/actions: select the Field Engineer, queue an airfield on valid land, advance its real construction queue, request a fighter through the production path, and advance its ingress.
- Assertions: airfield queues/completes, fighter request is accepted, fighter appears beyond the west map edge already airborne, and it crosses onto the tactical battlefield.
- Visual checkpoints: `airfield_complete`, `fighter_ingress`.
- Metrics: airfield completion, fighter delivery, and ingress ticks.
- Expected runtime: under 3 seconds headless.
- Run: `tools/validate airfield_fighter_ferry`; fixed-step recording is supported with `--record`.

## `simulation_scale_1000`

- Purpose: repeatably measure modest-scale native movement without calling it rendering FPS.
- Production systems: accepted `rts_scale_benchmark`, formation commands, flow-field cache, movement, snapshots, process RSS.
- Method: three separate runs, each with 100 warm-up ticks and 100 measured cache-hit ticks at 1,000 moving units and 100 unique destinations.
- Assertions: all runs exit successfully with structured metrics and all 1,000 units move in each run.
- Metrics: enqueue, cold field generation, simulation average/p50/p95/max, snapshot average, RSS, state hashes, and cross-run average variance.
- Expected runtime: under 5 seconds on the verified development host; hardware-dependent.
- Run: `tools/validate simulation_scale_1000`.

## `combat_benchmark_2000`

- Purpose: prove the active Goal 03 workload has target acquisition, firing, destruction, and evolving authoritative state at 2,000 units per faction.
- Production systems: `rts_combat_benchmark`, combat targeting, projectile manager, health/destruction cleanup, and simulation state hashing.
- Assertions: 4,000 initial units, nonzero projectiles and destroyed units, fewer final live units, different initial/final state hashes, and the benchmark's latency gate.
- Metrics: command enqueue, cold tick, cache-hit latency distribution, unit losses, projectile count, and state hashes.
- Boundary: this is simulation timing, not Godot rendering FPS. The 12 ms gate is host-sensitive and must be reported from the current run.
- Run: `tools/validate combat_benchmark_2000`.

## `native_skirmish_match_to_result`

- Purpose: prove the deterministic native Goal 08 controller is the player-facing setup, command, terminal-result, replay, and rematch route.
- Production systems: `native_skirmish.tscn`, `native_skirmish.gd`, `RtsExtension.skirmish_*`, native `Skirmish`, replay persistence under `user://matches/`.
- Assertions: authored scenario loads with fog-limited state, a controllable Elite unit accepts a legal order, empty selection is rejected, the match reaches its terminal result, a replay is saved and verified, and rematch resets the controller to active.
- Run: `Godot_v4.7.2-stable_linux.x86_64 --headless --path godot/project --script res://test_native_skirmish.gd` (also included by `tools/test godot`).

## Evidence policy

Headless runs record checkpoint names and an explicit warning that rendered capture was skipped. `--require-screenshots` converts missing screenshots into FAIL. A hidden development-only `--force-failure` flag exists solely to prove exit/report failure propagation and is not an acceptance bypass.

## Godot contract harnesses

These headless SceneTree tests verify content and presentation contracts used
by playable routes. They are included in `tools/test godot` and `tools/test all`.

| Harness | Coverage |
|---|---|
| `test_goal10_terrain.gd` | Heightmap, biome, mesh, and terrain material contract. |
| `test_visual_pack_compatibility.gd` | Visual-pack handshake and manifest compatibility. |
| `test_visual_asset_validator.gd` | Manifest assets resolve; no missing GLBs. |
| `test_native_visual_ids.gd` | Stable native visual IDs and authored definitions. |
| `test_unit_visual_root.gd` | Model wrapper, overlays, hardpoints, pose, and LOD. |
| `test_reference_model_load.gd` | All reference-model prototype slots load. |
| `test_map_editor_model.gd` | Valid and invalid editable map mutations. |
| `test_map_editor_export.gd` | Native map-editor export assertions. |
