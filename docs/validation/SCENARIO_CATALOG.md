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

## Evidence policy

Headless runs record checkpoint names and an explicit warning that rendered capture was skipped. `--require-screenshots` converts missing screenshots into FAIL. A hidden development-only `--force-failure` flag exists solely to prove exit/report failure propagation and is not an acceptance bypass.
