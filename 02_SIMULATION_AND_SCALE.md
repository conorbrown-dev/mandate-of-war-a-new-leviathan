# Goal 2 — Simulation Core, Strategic Camera, and Scale Proof

## Current Status (2026-09-01)

**Verified complete in the current dirty tree; no new committed baseline created.** Release/CTest, the 1,000/5,000/10,000/25,000 assertion-backed moving workloads, Godot editor/runtime smoke checks, historical real-window profiles, human controls/stability validation, and the required Codex review are recorded in `docs/CURRENT_STATE.md`, `docs/PERFORMANCE.md`, and `docs/PLAYER_TESTING.md`.

The review found provisional combat contaminating this goal's scale harness. The warranted same-faction isolation fix was applied and the full moving matrix was rerun. This completion does not validate mixed-faction combat or any later milestone.

Read `00_PROJECT_CHARTER.md`, `docs/ARCHITECTURE.md`, and `docs/CURRENT_STATE.md` first.

## Objective

Build the smallest playable/simulatable prototype that proves the core architecture can support large numbers of lightweight RTS entities.

## Required Features

1. Fixed simulation tick independent of rendering.
2. Data-driven unit definition format.
3. Simple test terrain/map.
4. Strategic camera with smooth Supreme Commander-style zoom.
5. Spawn at least 1,000 lightweight units.
6. Render them efficiently without one heavyweight script/object per simulated unit unless benchmarks prove that architecture acceptable.
7. Box selection.
8. Move orders.
9. Basic scalable movement/pathfinding.
10. Instrumentation showing at least:
    - FPS
    - simulation tick time
    - unit count
    - memory usage if available
11. Headless simulation executable/mode.
12. Automated tests for core simulation data, movement commands, and serialization where appropriate.

## Pathfinding Guidance

Do not implement naive full A* independently for every unit at large scale.

Evaluate simple scalable foundations such as:

- shared/coarse paths
- navigation sectors
- flow fields
- path caching
- asynchronous jobs
- local avoidance separated from strategic routing

The first implementation may be simple, but it must leave a credible path toward large armies.

Document early pathfinding decisions in `docs/PATHFINDING.md`.

## Benchmarks

Create repeatable headless benchmarks for at least:

- 1,000 moving units
- 5,000 moving units
- 10,000 moving units

If practical, also test:

- 25,000 units

Record:

- simulation ms/tick
- memory
- pathfinding cost
- wall-clock runtime
- build/hardware configuration

Write results to `docs/PERFORMANCE.md`.

Do not benchmark only idle entities. Units must perform representative movement work.

## Required Review

Invoke Codex after the 10,000-unit benchmark.

Ask it to challenge both code and benchmark validity:

- Are units doing meaningful work?
- Are results hiding idle/no-op behavior?
- Are there per-unit allocations?
- Is engine integration likely to become a bottleneck?
- Does the architecture scale under congestion?
- What representative benchmark is missing?

Evaluate and apply warranted fixes, then rerun benchmarks.

## Completion Criteria

Complete only when:

- strategic zoom works
- units can be selected and moved
- the simulation runs headlessly
- benchmark results exist and are reproducible
- results are documented honestly
- Codex review has been evaluated
- `docs/CURRENT_STATE.md` and `docs/NEXT_TASKS.md` are updated

Stop after this goal.
