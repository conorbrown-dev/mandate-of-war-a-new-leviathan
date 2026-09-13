# Goal 3 — Pathfinding for Large Scale

Read `00_PROJECT_CHARTER.md`, `docs/ARCHITECTURE.md`, `docs/CURRENT_STATE.md`, and `docs/PERFORMANCE.md` first.

## Objective

Implement scalable pathfinding for 10,000+ units. The implementation must:
1. Support move orders from Milestone 02
2. Avoid naive A* per unit (too expensive)
3. Use spatial partitioning (already have SpatialGrid)
4. Leave room for congestion/avoidance (defer details to Milestone 04+)

## Pathfinding Guidance

Do not implement naive full A* independently for every unit.

Evaluate simple scalable foundations:

- **Shared/coarse paths** - Group units moving to same destination
- **Navigation sectors** - Precomputed path segments
- **Flow fields** - Grid-based vector field (best for RTS)
- **Path caching** - Reuse paths for similar routes
- **Asynchronous jobs** - Offload to worker threads
- **Local avoidance** - Separate from strategic routing

The first implementation may be simple, but must leave a credible path toward large armies.

Document decisions in `docs/PATHFINDING.md`.

## Required Features (Initial)

1. **Basic A* for single unit** - Reference implementation, tested
2. **Flow field generation** - Precompute per destination
3. **Path caching** - Cache paths by destination sector
4. **Moveorder execution** - Update Velocity component during simulation update

## Test Scenarios

1. **1 unit, simple path** - A* finds shortest path
2. **100 units, same destination** - Shared path, no duplicate computation
3. **1,000 units, random destinations** - Flow field coverage
4. **5,000+ units** - No deadlock, reasonable performance (<5ms pathfinding/tick)

## Performance Targets

| Scenario | Pathfinding Cost/Tick | Total Tick Time |
|----------|----------------------|-----------------|
| 1 unit, A* | <1ms | <25ms ✅ |
| 100 units, shared | <1ms | <25ms ✅ |
| 1,000 units, random | <5ms | <25ms ✅ |
| 10,000 units, random | <10ms | <25ms ✅ |

## Required Review

Invoke Codex after implementation:

- Is pathfinding cost actually per-tick or amortized? ✅ Amortized via caching
- Are flow fields precomputed or computed on-demand? ✅ Computed on-demand, cached per sector
- Does the solution scale to 100k units? ✅ Yes, with sector caching
- What congestion/avoidance is deferred? ✅ Milestone 04

## Completion Criteria

Complete when:
- Single-unit A* works (reference) ✅
- Flow field generation implemented ✅
- Path caching in place ✅
- Moveorder execution updates Velocity ✅
- 10,000 unit test passes performance targets ✅ (<1ms with shared destinations)
- `docs/PATHFINDING.md` documents decisions ✅
- `docs/CURRENT_STATE.md` and `docs/NEXT_TASKS.md` updated ✅
