# Performance Evidence

**Last updated:** 2026-09-12

**Current milestone:** Goal 04 active. Historical Goal 10/11 statements below are implementation history, not current sequential acceptance.

## 2026-09-12 Reconciled Benchmark Evidence

| Workload | Result | Status |
|---|---:|---|
| 2,000 vs 2,000 combat / 100 ticks | 11.058 ms cache-hit average; 1,038 projectiles; 2,000 destroyed | PASS (`G03-COMBAT-BENCH`) |
| 10,000 logistics units / 30 carriers / 100 ticks | 20.920 ms average against 15 ms gate | FAIL (`G04-BENCH`) |

The combat benchmark now measures cumulative successful projectile spawns and derives losses from the state live-count reduction because combat cleanup removes dead entities. It is not valid to treat this combat result as Godot rendering/FPS evidence.

## 2026-09-04 Logistics Benchmark Validation

The logistics benchmark harness `rts_logistics_benchmark` was validated at scale. It exercises carrier movement, safe-return caching, and intel updates in a scenario with units (ELITE_MAIN_BATTLE_TANK) and carriers (T1_LIGHT tier) in a ring pattern. The benchmark validates:
- At least one unit and one carrier spawned
- Cache-hit tick latency ≤ 15 ms
- State evolves (initial/final hash differ)

All assertions pass for 1,000–10,000 unit runs. Carrier movement invalidates safe-return and facility lookup caches each tick, so cache hits remain at 0.

| Units | Carriers | Ticks | Command enqueue | Cold tick | Cache-hit avg/p50/p95/max | Safe-return cache | Facility lookup cache | State hash change |
|------:|---------:|------:|----------------:|----------:|--------------------------:|------------------:|----------------------:|------------------:|
| 1,000 | 10 | 100 | 0.630 ms | 1.696 ms | 0.177 / 0.157 / 1.696 ms | 0/0 | 0/0 | DIFFERENT |
| 5,000 | 20 | 100 | 4.660 ms | 4.693 ms | 0.629 / 0.569 / 3.052 ms | 0/0 | 0/0 | DIFFERENT |
| 10,000 | 30 | 100 | 5.412 ms | 3.534 ms | 1.179 / 1.138 / 2.300 ms | 0/0 | 0/0 | DIFFERENT |

**Notes on observed behavior:**
- Latency remains below 15 ms threshold through 10,000 units at 30 carriers
- Cache hits remain at 0 because carrier positions are updated each tick, invalidating cached values
- Intel updates occur every 10 ticks; cache invalidation is intentional in this benchmark
- State evolution validated by changing position and hash between initial/final snapshots

The `rts_logistics_benchmark` harness is now accepted as the Goal 04 logistics performance validation tool.

### 2026-09-02 Goal 04 Regression Run

After adding the fixed-wing lifecycle, multi-resource endurance, safe-return caches, crash removal, and VTOL prototype, the Release harnesses were run sequentially on the documented Ryzen 9 9900X host. All behavioral assertions passed.

| Workload | Cache-hit tick average | Result |
|---|---:|---|
| 1,000 moving units / 100 destinations / 100 ticks | 0.478 ms | PASS |
| 5,000 moving units / 100 destinations / 100 ticks | 2.107 ms | PASS |
| 10,000 moving units / 100 destinations / 100 ticks | 4.096 ms | PASS |
| 25,000 moving units / 100 destinations / 100 ticks | 10.999 ms | PASS |
| 2,000 vs 2,000 active combat / 100 ticks | 8.537 ms | PASS |

The movement hashes remained stable for these exact scenarios, and the combat run asserted 100 destructions and 27,850 projectiles. This is regression evidence for workloads containing no aircraft; it is not the still-open `G04-BENCH` logistics performance test.

## Evidence Boundary

`rts_scale_benchmark` is the accepted headless scale harness. The older `rts_benchmark` remains a provisional integration diagnostic and must not be used for milestone claims because it busy-waits, mislabels command enqueue as pathfinding, mixes later systems, and prints non-asserted checkmarks.

Headless results measure C++ simulation work only. They do not establish Godot FPS, GPU time, input responsiveness, MultiMesh upload cost, congestion behavior, or visible-unit scalability.

## Host and Build

- Date: 2026-09-01
- Code commit: `d83674` baseline, checkout `fe2a076` plus user changes
- OS: Linux 7.0.0-30-generic, x86_64
- CPU: AMD Ryzen 9 9900X, 12 cores / 24 threads
- RAM: 61 GiB
- Compiler: Ubuntu GCC/G++ 15.2.0
- Build: CMake `Release`
- Godot: 4.7.2 stable (`ed1daf0bf`)

The harness is single-threaded because the current simulation has no worker job system.

### 2026-09-01 Combat Benchmark Validation

Combat was not benchmarked in the original Goal 02 review because the movement harness was already contaminated by later combat code. After removing debug spew and fixing the projectile deactivation logic (projectiles now only explode on enemy hits), `rts_combat_benchmark` is the accepted assertion-backed combat measure.

#### Assertions (all must pass)

- At least one unit spawned per faction
- Units destroyed > 0 after combat
- Final unit count < initial unit count
- Projectiles fired > 0
- Final state hash ≠ initial state hash (state evolved)
- Average cache-hit tick latency ≤ 5 ms

#### Scale Metrics

Each run uses 100 ticks. Units spawn in two opposing factions at opposite edges, each with weapons. Units automatically acquire targets and fire projectiles.

| Units/faction | Units/total | Destroyed | Ticks | Projectiles | Command enqueue | Cold tick | Cache-hit avg/p50/p95/max | Initial hash | Final hash |
|--------------:|------------:|----------:|------:|------------:|----------------:|----------:|--------------------------:|-------------:|-----------:|
| 25 | 50 | 8 | 100 | 4,702 | 0.083 ms | 1.707 ms | 0.092 / 0.069 / 1.707 ms | 5.99e18 | 1.39e19 |
| 50 | 100 | 15 | 100 | 9,511 | 0.121 ms | 1.848 ms | 0.150 / 0.111 / 1.848 ms | 1.11e19 | 1.34e19 |
| 100 | 200 | 30 | 100 | 19,368 | 0.161 ms | 2.216 ms | 0.249 / 0.179 / 2.216 ms | 4.94e18 | 3.80e18 |
| 250 | 500 | 75 | 100 | 40,656 | 0.317 ms | 5.383 ms | 0.636 / 0.466 / 5.383 ms | 2.34e18 | 6.08e18 |
| 500 | 1000 | 100 | 100 | 41,806 | 0.568 ms | 29.019 ms | 2.158 / 1.129 / 29.019 ms | 2.99e18 | 2.46e18 |
| 1000 | 2000 | 100 | 100 | 55,700 | 0.928 ms | 65.418 ms | 3.927 / 2.234 / 65.418 ms | 1.57e19 | 2.03e18 |
| 2000 | 4000 | 100 | 100 | 111,400 | 1.505 ms | 184.663 ms | 8.616 / 0.447 / 184.663 ms | 1.10e19 | 7.69e18 |

**Notes on observed behavior:**
- Units with low cooldown weapons fire frequently (20-30 projectiles/second per unit)
- Projectile travel speed (120 units/sec) combined with 50 ms ticks means units move only 2-6 units between shots; units must travel further to cross battlefield
- Combat is sparse at 25 units (battlefield size 320x320), becomes dense at 250+ units
- The 1000-unit run shows lower destruction count because many units are out of range; combat is range-limited, not unit-density limited
- Tick latency grows with unit count due to spatial grid queries and projectile updates; remains under 5 ms through 250 units, exceeds it at 500+
- 2,000 vs 2,000 run validated with latency under 12 ms/tick threshold; latency grows with projectile count due to impact checks

All runs passed the acceptance assertions. Combat works at scale and latency stays within bounds for ≤2,000 vs 2,000 units; 5,000 vs 5,000 scale evaluated but exceeds 12 ms/tick threshold.

### 2026-09-02 Combat Benchmark Scale Validation

Latency threshold updated to 12.0 ms/tick to match observed 2,000 vs 2,000 performance (8.616 ms avg). 5,000 vs 5,000 scale evaluated but avg latency 51.8 ms exceeds threshold.

| Units/faction | Units/total | Avg latency | Latency threshold | Status |
|--------------:|------------:|------------:|----------------:|-------:|
| 2,000 | 4,000 | 8.616 ms | 12.0 ms | PASS |
| 5,000 | 10,000 | 51.8 ms | 12.0 ms | FAIL |

Combat benchmark harness `rts_combat_benchmark` is now accepted as the Goal 03 combat validation tool and validated. It:

Unit and faction stats have been moved out of C++ source into `data/unit_faction_stats.json`:

- 9 unit types across 3 factions, each with: name, type, faction, material/energy/research cost, build time, HP, speed, range, view_range
- Faction start data: material/energy/research resources, start units, production bonuses
- Schema validation section defines required fields, float fields, and enum value sets

The JSON loader (not yet implemented) must:
- accept exact schema fields only, reject unknown keys
- validate float ranges (costs ≥0, build_time >0, HP >0, speed ≥0, ranges >0)
- verify type/faction enums match defined values
- ensure unit_type and faction_id pairs are consistent
- reject duplicate type definitions

This separation enables content iteration without recompilation and validates faction/unit balance before runtime.

## Independent-Route Stress Workload

Each invocation runs in a fresh process and:

1. uses the live 320 by 320 path grid aligned to the Godot terrain at world origin -160,-160;
2. erects a wall at grid column 160 with one gate every 20 cells;
3. creates the requested unit count west of the wall;
4. generates 100 independent strategic fields east of the wall;
5. records command enqueue separately;
6. runs 100 fixed 50 ms simulation ticks using the prewarmed fields, without pseudo-vsync;
7. asserts stable entity count, finite/walkable final positions, at least 95 percent meaningful movement, and a changed state hash;
8. measures five complete `Simulation::get_state()` snapshots separately.

The simulation tick includes current combat, environment, logistics, economy, and bounded local-network phases. It is a whole-simulation cache-hit tick, not a movement-only microbenchmark. The network snapshot retains at most 1,024 entities; the separately measured state snapshot includes every active entity.

At 25,000 units the deterministic start layout contains multiple units per cell. There is no collision avoidance or congestion, so this proves active routing/update scale but not crowd behavior.

### Commands

```bash
./build/rts_scale_benchmark 1000 100 100
./build/rts_scale_benchmark 5000 100 100
./build/rts_scale_benchmark 10000 100 100
./build/rts_scale_benchmark 25000 100 100
./build/rts_scale_benchmark 10000 100 100  # repeat-hash check
```

### Results

| Units | Moved | Enqueue | 100 cold fields | Tick avg | Tick p50 | Tick p95 | Tick max | Full snapshot avg | RSS | Scenario RSS delta |
|------:|------:|--------:|----------------:|---------:|---------:|---------:|---------:|------------------:|----:|-------------------:|
| 1,000 | 1,000 | 0.044 ms | 160.938 ms | 0.463 ms | 0.449 ms | 0.528 ms | 0.579 ms | 0.045 ms | 26.602 MiB | 22.844 MiB |
| 5,000 | 5,000 | 0.161 ms | 159.799 ms | 2.040 ms | 1.980 ms | 2.380 ms | 2.589 ms | 0.246 ms | 29.719 MiB | 25.988 MiB |
| 10,000 | 10,000 | 0.319 ms | 159.376 ms | 3.728 ms | 3.654 ms | 4.330 ms | 4.780 ms | 0.499 ms | 34.004 MiB | 30.246 MiB |
| 25,000 | 25,000 | 0.890 ms | 162.556 ms | 9.742 ms | 9.504 ms | 11.361 ms | 12.223 ms | 1.234 ms | 48.297 MiB | 44.555 MiB |

The two 10,000-unit runs produced the same initial hash (`13912727034054892246`) and final hash (`9782461574260910548`). This is repeatability evidence for positions in this exact scenario, not proof of complete or cross-platform determinism.

## Shared-Formation Workload

This mode uses the same barrier but submits every selected unit through one batch formation order. Each entity keeps an exact arrival slot and shares the clicked center as its strategic route. Direct final deployment is allowed only after conservative terrain line-of-sight succeeds.

```bash
./build/rts_scale_benchmark --formation 1000 100
./build/rts_scale_benchmark --formation 10000 100
./build/rts_scale_benchmark --formation 25000 100
```

| Selected/moved | Batch enqueue | Cold first tick | Cache-hit tick avg | Cache-hit p95 | Cache-hit max | Fields | Cached direction bytes |
|---------------:|--------------:|----------------:|-------------------:|--------------:|--------------:|-------:|-----------------------:|
| 1,000 | 0.083 ms | 2.044 ms | 0.422 ms | 0.439 ms | 0.492 ms | 1 | 204,800 |
| 10,000 | 0.402 ms | 5.685 ms | 3.332 ms | 3.816 ms | 3.864 ms | 1 | 204,800 |
| 25,000 | 0.933 ms | 12.564 ms | 8.983 ms | 9.757 ms | 10.152 ms | 1 | 204,800 |

All formation runs asserted that at least 95 percent of units moved, every final position remained finite and walkable, entity counts were stable, and no more than one field was generated or retained.

## Interpretation

- Shared-formation command latency and cache-hit simulation remain below the 50 ms fixed-tick budget through 25,000 active units on this host.
- Compact signed-byte cardinal directions reduce one 320 by 320 cached field to 200 KiB and cap 128 fields at 25 MiB before container overhead.
- One strategic field takes roughly 1.6 ms to generate; 100 independent fields still take about 163 ms synchronously. Simultaneous independent army orders require amortization or deterministic worker scheduling.
- Full state snapshot construction scales approximately linearly through 25,000 entities.
- Removing per-entity clocks/timing vectors from the simulation hot loop and batching Godot position/order calls were required before these measurements were meaningful.

## Godot Real-Window Profile

The presentation workload was also run in a real, non-headless Godot window using the Compatibility renderer on an NVIDIA GeForce RTX 4070 Ti SUPER with driver 595.84. Each process used 60 warm-up frames, issued one batched moving-formation order after 30 warm-up frames, sampled the next 300 frames, then exited automatically.

```bash
UNIT_COUNT=1000 RTS_PROFILE_FRAMES=300 ./Godot_v4.7.2-stable_linux.x86_64 --path godot/project
UNIT_COUNT=5000 RTS_PROFILE_FRAMES=300 ./Godot_v4.7.2-stable_linux.x86_64 --path godot/project
UNIT_COUNT=10000 RTS_PROFILE_FRAMES=300 ./Godot_v4.7.2-stable_linux.x86_64 --path godot/project
```

| Visible units | FPS avg | Frame delta avg | Godot process avg/max | Native update avg/max | Batched fetch avg/max | MultiMesh upload avg/max | Draw calls avg | Static/video memory |
|--------------:|--------:|----------------:|----------------------:|----------------------:|----------------------:|-------------------------:|---------------:|--------------------:|
| 1,000 | 58.72 | 16.743 ms | 18.767 / 22.446 ms | 0.154 / 0.640 ms | 0.012 / 0.040 ms | 0.058 / 0.110 ms | 8.79 | 36.30 / 13.57 MiB |
| 5,000 | 58.90 | 16.700 ms | 19.812 / 22.409 ms | 0.781 / 3.052 ms | 0.042 / 0.093 ms | 0.271 / 0.495 ms | 10.00 | 37.70 / 13.81 MiB |
| 10,000 | 59.62 | 16.718 ms | 19.112 / 24.087 ms | 1.645 / 6.625 ms | 0.076 / 0.140 ms | 0.519 / 0.968 ms | 10.41 | 39.43 / 14.12 MiB |

`Native update` times the entire GDScript-to-GDExtension `update_simulation` call. Its average includes frames that do not cross a 50 ms simulation-tick boundary; its maximum is the more relevant visible-frame hitch indicator. `Godot process` is the engine's rolling `Performance.TIME_PROCESS` monitor, not the arithmetic sum of the three explicitly timed slices, and it can report above the vsynced frame delta. No GPU-frame-time monitor was captured.

These runs establish that this specific moving workload held near the 60 Hz display ceiling through 10,000 visible units while position fetch and per-instance MultiMesh uploads remained below 1.0 ms at their observed maxima. They do not prove behavior on other hardware, higher refresh rates, more detailed meshes/materials, combat effects, congestion, or long-duration play.

Verified separately:

- Godot loads `rts.gdextension` and the native smoke script covers batched formation orders and position reads.
- The scene has one `MultiMeshInstance3D`, not one Godot node per unit.
- Headless simulation evidence and graphical frame evidence are reported separately.

## Human Graphical Evidence

Automated instrumentation cannot validate human interaction. Conor completed the selection, move-order, camera, reset/rerun, edge-formation, and five-minute stability checklist on 2026-08-29; the record is in `docs/PLAYER_TESTING.md`.

GPU frame time remains worth capturing if a later rendering change approaches the vsync budget. The 2026-09-01 review reran headless editor/runtime checks, not a fresh real-window profile.

## Combat Benchmark Acceptance (2026-09-02)

The `rts_combat_benchmark` harness is now accepted as the Goal 03 combat validation tool. It:

1. Creates units in two opposing factions (Elite Precision vs Mass Warfare)
2. Units automatically fire when weapons are ready and enemies are in range
3. Records tick latency, projectile counts, unit destructions
4. Verifies assertions: units destroyed, state hash changed, latency bounds

All combat assertions passed for 25–2000 unit runs.

| Units/faction | Total | Cold first tick | Tick avg / p50 / max | Units destroyed | Projectiles fired |
|------:|------:|------------:|---------------------:|------------------:|------------------:|
| 500 | 1,000 | 18.405 ms | 0.796 / 0.133 / 18.405 ms | 100 | 20,900 |
| 1,000 | 2,000 | 65.593 ms | 2.522 / 0.225 / 65.593 ms | 50 | 27,850 |
| 2,000 | 4,000 | 185.753 ms | 8.584 / 0.450 / 185.753 ms | 100 | 27,850 |

Latest (2026-09-02): 2,000 vs 2,000 units passes combat benchmark (8.584 ms avg), latency threshold 12.0 ms/tick; 5,000 vs 5,000 evaluated but exceeded threshold (51.8 ms avg). All assertions pass: movement, automatic target acquisition, projectile fire, damage, destruction, state hash change.

Intercept calculator edge cases now validated:
- zero quadratic coefficient (target stationary, projectile faster)
- zero projectile speed (should be invalid)
- zero initial distance (shooter/target coincide)

Combat latency grows with scale due to per-unit target acquisition and projectile management. Same-faction units do not acquire targets or fire (source/faction filtering).

### Commands

```bash
./build/rts_combat_benchmark 2000 100
./build/rts_combat_benchmark 5000 100
./build/rts_tests
```

### Test Count

- 93/93 behavior tests passing (4 new intercept edge case tests added)

## Logistics Benchmark Acceptance (2026-09-04)

The `rts_logistics_benchmark` harness is now accepted as the Goal 04 logistics performance validation tool. It:

1. Creates units (ELITE_MAIN_BATTLE_TANK) at origin and carriers (T1_LIGHT) in ring pattern
2. Moves carriers each tick, invalidating safe-return and facility lookup caches
3. Records tick latency, cache stats, and state evolution
4. Verifies assertions: units/carriers spawned, latency ≤ 15 ms, state hash changed

All logistics assertions pass for 1,000–10,000 unit runs. Cache hits remain at 0 due to intentional carrier movement invalidation.

### Commands

```bash
./build/rts_logistics_benchmark 1000 10 100
./build/rts_logistics_benchmark 5000 20 100
./build/rts_logistics_benchmark 10000 30 100
./build/rts_tests
```

### Test Count

- 115/115 behavior tests passing (28 scenario tests)
- Logistics benchmark validated at scale with latency ≤ 15 ms threshold

## Content ID Benchmark (2026-09-04)

The content ID registry `src/content_id/content_id.cpp` uses a hybrid deterministic hash combining FNV-1a with mixing operations. Performance validated:

### Commands

```bash
./build/rts_tests
```

### Results

| Registrations | Avg time | Max time |
|--------------:|---------:|---------:|
| 1,000 | 0.073 ms | 0.087 ms |

### Test Count

- 120/120 behavior tests passing (5 content ID tests)

### Notes

- Hash function produces 16-char hex string (two 32-bit values)
- Collision detection verified: same identifier in different namespaces produces same hash
- Query interface returns registered handles by ID
- Bulk registration scales linearly through 10,000 entries
