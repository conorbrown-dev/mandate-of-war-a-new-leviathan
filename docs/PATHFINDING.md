# Pathfinding Design and Verified State

**Last updated:** 2026-09-01

**Status:** Goal 02 reference routing, representative headless scale, real-window movement profiles, and human controls/stability validation are complete; congestion remains open.

## Current Model

- Rectangular grid with configurable positive dimensions and cell size.
- Four-directional cardinal movement.
- Walkability stored once per grid cell.
- Configurable world origin; the live simulation maps the full Godot terrain from -160 to 160 onto a 320 by 320 grid.
- World coordinates use floor-based cell conversion relative to that origin.
- Invalid dimensions, non-positive/non-finite cell sizes, and non-finite query coordinates are rejected safely.

## Reference A*

`Pathfinding::find_path()` is the correctness/reference route implementation.

- The open set owns value records rather than raw pointers.
- Priority is ordered deterministically by `f` score, then `g` score, then cell index.
- Scores, predecessor cells, and closed state use bounded vectors sized to the grid.
- Returned paths include the start and destination cell centers.
- Blocked, unreachable, negative, and out-of-bounds requests return an empty path.

This implementation is suitable as a reference and for low-volume queries. It is not intended to run independently for every unit in a large army.

## Shared Flow Fields

`Pathfinding::generate_flow_field()` performs a reverse breadth-first search from an exact destination cell.

- Each reachable cell points to its immediate next cardinal BFS cell.
- The destination, blocked cells, and unreachable cells have a zero direction.
- Cache keys use the exact destination cell, not a coarse sector that aliases different goals.
- `set_cell()` and `clear_blocks()` invalidate cached fields.
- Cached cardinal directions use two signed bytes per cell rather than two floats.
- The cache retains at most 128 fields using deterministic insertion-order eviction. On the current 320 by 320 grid, a full cache contains 25 MiB of direction data before container overhead.
- `flow_direction()` lets the simulation consume a cached direction without copying the full field on every unit tick.

## Simulation Integration

Move orders retain exact world-space arrival and strategic-route destinations. Single-unit orders use the same point for both. Formation orders assign exact per-unit slots while every member shares one strategic center as its route. If the requested footprint would cross the simulation grid, spacing is reduced as needed and the center is shifted inward so every arrival remains on a valid cell center.

While terrain blocks direct deployment, units follow the shared strategic field. A conservative grid traversal proves line of sight before switching to the exact arrival slot, preventing diagonal corner clipping through blocked cells. The final step snaps to the target without overshoot.

The integration test erects a four-cell wall, checks every traversed cell remains walkable, and verifies exact arrival behind the obstacle.

## Automated Evidence

The registered suite covers:

- shortest straight and obstacle-detour A* paths;
- unreachable, negative, out-of-bounds, and non-finite requests;
- stable results across 256 repeated A* queries;
- immediate flow-field directions and zero directions for blocked/destination cells;
- distinct destinations within one former sector;
- terrain-change cache invalidation;
- the 128-field cache bound;
- compact cache byte accounting and origin-offset coordinates;
- conservative line-of-sight rejection through blocked cells;
- simulation movement around blocked cells and exact arrival.
- 200-unit formation deployment through a one-cell wall gate with exactly one generated field.
- oversized 100-unit formation fitting at the far world corner, including exact boundary-cell arrivals.

Release and AddressSanitizer/UndefinedBehaviorSanitizer runs pass this coverage. LeakSanitizer cannot run in the current traced sandbox, but A* no longer performs raw per-node allocation and ASan reports no invalid ownership behavior.

## Performance Boundary

The accepted scale harness separates cold generation, command enqueue, cache-hit ticks, and state snapshots. A 25,000-unit shared formation order generates one 200 KiB field and completes its cold first tick in 12.564 ms on the current host. A stress workload with 100 independent strategic destinations still needs about 163 ms to build all fields synchronously, so many simultaneous independent army routes require amortization or jobs.

## Known Limits

- Flow-field generation remains synchronous on the simulation thread.
- Cache eviction is insertion-order, not usage-aware.
- No hierarchical routing, portals, congestion cost, or local avoidance exist.
- Dynamic terrain invalidates every field rather than updating affected regions.
- `generate_flow_field()` expands compact directions into a full float field by value for compatibility; live movement uses `flow_direction()` and benchmarks use `prewarm_flow_field()`.
- Automated real-window movement profiling exists through 10,000 visible units and the human checklist passed on 2026-08-29; large-army congestion behavior remains open.
