# Goal 09 — Logistics Improvements

**Status:** VERIFIED on 2026-09-13
**Reference:** `docs/LOGISTICS.md`, `docs/LOGISTICS_ARCHITECTURE_REVIEW.md`, `docs/EXECUTION_LEDGER.md`

## Purpose

Improve logistics decisions beyond the Goal 04/08 baseline while preserving
deterministic simulation behavior and the existing capability-flag contract.

## Scope and acceptance criteria

| ID | Requirement | Evidence |
|---|---|---|
| G09-PATH | Safe-return estimates use an available pathfinding route, including traversal costs and detours, instead of assuming unobstructed straight-line distance. Air units retain a deterministic straight-line fallback when no route exists. | Assertion-backed logistics scenario with a blocked direct route and a longer detour. |
| G09-SUPPLY | Naval resupply is finite-stock and costed: a request is accepted only for a compatible owned facility with sufficient energy/material, deducts those resources, restores bounded fuel, and rejects insufficient stock without mutation. | Direct integration assertions for success, ownership, capacity, and fail-closed stock handling. |
| G09-TELEMETRY | Recovery estimates expose route distance, time, and resource costs to the native state/UI without leaking hidden state. | Native state assertions and inspected HUD telemetry. |
| G09-BENCH | The improved route calculation remains bounded under a representative aircraft/facility workload. | Release benchmark with recorded workload and timing. |
| G09-TESTS | CTest, direct runners, and a deterministic Godot logistics scenario pass; intentional failure proves the scenario detects a broken expectation. | Current reports and validation catalog entry. |
| G09-REVIEW | A Codex review evaluates the implementation, evidence quality, and limitations. | `docs/GOAL_09_CODEX_REVIEW.md`. |

## Definition of done

Every row above is verified with current evidence, documentation is synchronized,
and no Goal 10+ work is advanced before this gate closes.
