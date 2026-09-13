# Goal 4 — Air, Naval, Logistics, and Strategic Reconnaissance Prototype

Read the charter and current project docs first.

## Objective

Prove the game's defining logistics concept before building a huge content catalog.

Create a dedicated logistics test map with two substantial landmasses separated by a large sea zone.

## Airbase / Airstrip System

Implement an airbase with a physical runway/airstrip.

Conventional fixed-wing aircraft should use a simplified but meaningful lifecycle:

1. queue for runway
2. take off
3. perform mission
4. return
5. enter recovery/landing process
6. land
7. refuel/rearm

Do not build a flight simulator. Preserve strategic runway dependency while abstracting unnecessary detail.

## Aircraft Operational Range

Most normal aircraft should have limited endurance.

Track some combination of:

- Energy reserves/consumption
- Material reserves/consumption
- time/range endurance

Aircraft should determine whether they can safely reach a compatible recovery facility after completing orders.

Create a scalable safe-return estimator.

Do not run an expensive path-to-every-base query every tick for every plane. Use spatial indexing, caching, periodic/event-driven recalculation, and compatible-facility indexes.

## Failure to Recover

If a conventional aircraft exhausts endurance and cannot reach a valid recovery point:

- it loses powered flight
- crashes into terrain/water
- is destroyed appropriately

If a runway becomes unusable and no other recovery option exists, conventional aircraft may be lost.

Warn the player about unsafe orders but do not prohibit deliberate suicidal missions.

## VTOL

Implement a prototype VTOL aircraft.

Its defining advantage:

- does not depend on a conventional runway for takeoff/recovery

Potential tradeoffs to prototype:

- lower range
- lower payload
- higher Energy cost
- higher production/Research cost

The purpose is to determine whether runway independence creates enough gameplay value to make VTOL genuinely desirable.

## Aircraft Carriers

Carriers must ultimately exist at every tier:

- T1 light carrier
- T2 fleet carrier
- T3 supercarrier
- T4 experimental carrier/mobile airbase

For this milestone implement at least a T1 carrier prototype.

Carriers are mobile airbases and are essential for projecting ordinary air power across large oceans.

Implement at least:

- aircraft storage/capacity
- conventional aircraft recovery
- refuel/rearm
- launch/recovery queue

The logistics test map should be large enough that a normal fighter cannot safely cross the ocean and return without carrier or island-base support.

## Naval Operational Endurance

Implement a destroyer or equivalent combat ship with finite operational endurance.

Most naval vessels should eventually need:

- naval bases
- ports
- support ships

for refueling/resupply.

When a normal naval vessel exhausts endurance:

- it does NOT sink automatically
- it becomes stranded or severely mobility-limited
- it remains vulnerable and can later be resupplied

Aircraft carriers are an exception and should have exceptional strategic endurance.

## Naval Base

Implement naval-base resupply for the prototype ship.

## Strategic Reconnaissance

Implement a T3 strategic reconnaissance aircraft conceptually inspired by the role of the SR-71, while remaining an original fictional design.

Traits:

- very high speed
- very long range
- expensive
- weakly armed or unarmed
- powerful sensors
- difficult to intercept

It should cross the logistics test map, gather intelligence, and return without requiring the repetitive scout spam used by many RTS games.

## Intelligence Memory

Implement persistent but aging intelligence records.

Track at least:

- enemy entity/building type
- last known position
- last observed simulation tick/time
- freshness/confidence

Differentiate:

- currently observed
- stale but remembered

Allow strategic UI to represent stale intel differently.

## Required Prototype Scenario

Demonstrate all of these:

1. two landmasses separated by ocean
2. airbase on each landmass
3. normal fighter cannot safely cross alone
4. T1 carrier enables staged crossing/recovery
5. fighter can refuel/recover on carrier
6. conventional aircraft requires runway/deck recovery
7. VTOL does not require runway
8. overextended conventional aircraft crashes
9. destroyer has finite endurance
10. destroyer becomes stranded when depleted
11. naval base restores it
12. T3 recon aircraft crosses the theater and returns
13. recon intel persists after aircraft leaves
14. intel becomes stale over time

## UI / Diagnostics

Expose:

- remaining endurance
- current consumption
- predicted return cost
- closest compatible recovery facility
- safe-return state
- carrier deck occupancy
- runway/recovery queue
- naval stranded state
- intelligence age

## Performance Test

Benchmark the logistics system with a meaningful number of aircraft/ships.

Specifically test the cost of:

- recovery-facility lookup
- safe-return estimation
- moving carrier recovery points
- intelligence updates

## Required Review

Invoke Codex for a focused logistics architecture review.

Ask whether:

- safe-return calculations scale
- moving carriers create hidden O(N×M) behavior
- aircraft scheduling could deadlock/starve
- stranded naval-state logic is robust
- intelligence memory is deterministic and compact
- logistics creates networking/replay risks

Evaluate and fix warranted findings.

## Completion Criteria

All 14 prototype behaviors work, tests/benchmarks exist, and documentation is updated.

Create/update `docs/LOGISTICS.md`.

Do not begin Goal 05 until every completion criterion above is verified and the durable state/docs are updated. When continuous sequence mode is authorized in `docs/EXECUTION_LEDGER.md`, advance exactly once to Goal 05 after this gate closes; otherwise stop at the verified boundary.
