# Goal 04 Logistics Architecture Review

**Date:** 2026-09-12
**Scope:** safe return, moving carriers, flight-deck scheduling, naval endurance, intelligence memory, and replay risk.

## Findings and disposition

| Review question | Finding | Disposition |
|---|---|---|
| Safe-return scaling | Global cache invalidation on carrier movement caused repeated full estimation. | Fixed: bounded per-aircraft/facility cache validity; 10k-aircraft benchmark passes. |
| Moving carriers | Carrier movement did not update its recovery point. | Fixed and regression-tested in production movement. |
| Scheduling | Flight-deck queues are FIFO; landing takes priority and carrier capacity includes reservations. | Accepted; covered by lifecycle/capacity tests. |
| Stranded naval state | Stranded vessels remain present and resupply restores mobility. | Accepted; current authoritative RETURN also validates owned base proximity and Energy. |
| Intelligence | Intel was retained on recycled live entity IDs. | Fixed: archived remembered intelligence is separate from live ECS storage and cleared on reuse. |
| Networking/replay | Logistics updates use fixed ticks and ordered maps for facility/flight-deck/archived-intel state. | Accepted for current local/replay scope; network transport remains a later goal. |

## Evidence

- `rts_logistics_benchmark 10000 30 100`: 0.951 ms cache-hit average, p95 1.488 ms.
- CTest: 3/3 passing.
- Direct integration runner: 167 passed, 0 failed.
- Required behavior coverage is in `tests/test_logistics_scenario.cpp`, `tests/test_logistics_ecs.cpp`, `tests/test_logistics_visibility.cpp`, and `tests/test_skirmish.cpp`.

## Remaining non-blocking limits

Naval resupply is currently represented by the authoritative RETURN workflow and capability-bearing recovery facilities; a player-facing dedicated naval-base construction workflow is later product work.
