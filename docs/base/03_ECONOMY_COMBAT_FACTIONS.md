# Goal 3 — Economy, Combat, and Faction Vertical Slice

Read the charter and current architecture/state documents first.

## Objective

Turn the scale prototype into a minimal RTS battle loop while keeping all gameplay data driven.

## Implement Three Resources

### Material
Manufacturing/construction/ammunition/spares abstraction.

### Energy
Power/fuel/reactor/energy infrastructure abstraction.

### Research
R&D capacity used for advanced technologies and doctrine.

Research must not merely behave as "Material #2."

## Implement Three Factions

### Faction A — Elite Precision

Create approximately three starting combat units:

- elite main battle tank
- long-range artillery/missile platform
- advanced anti-air vehicle

Traits:

- expensive
- durable
- powerful
- advanced sensors/targeting

### Faction B — Mass Warfare

Create approximately three units:

- cheap swarm tank
- cheap assault vehicle
- cheap anti-air vehicle

Traits:

- inexpensive
- quick production
- high numbers
- replaceable

### Faction C — Industrial / Experimental

Create approximately three units:

- balanced MBT
- mobile missile/support platform
- engineering/support unit

Traits:

- balanced conventional force
- infrastructure/research advantages planned for later milestones

## Production

Implement:

- one basic factory type
- build queues
- resource consumption
- unit spawning
- build time
- rally/move orders if practical

## Combat

Implement purpose-built RTS projectile simulation supporting the initial weapon set.

Include as appropriate:

- projectile entities/data
- moving targets
- lead/intercept calculations
- ballistic arcs for artillery
- terrain/unit collision where practical
- AoE damage
- destruction

Avoid unnecessary heavyweight general-purpose rigid-body simulation.

Create `docs/PROJECTILE_SIMULATION.md`.

## Data Driven Content

Keep outside core code:

- Material cost
- Energy cost
- Research prerequisites
- HP
- armor
- speed
- acceleration
- weapon damage
- reload
- projectile speed
- range
- sensors

Add schema/content validation.

## Tests

Add automated tests for:

- resource generation/consumption
- build queues
- faction definitions
- projectile trajectories where deterministic
- interception calculations
- damage/AoE
- serialization/content validation

## Representative Combat Benchmark

Create a benchmark with meaningful combat activity, not idle entities.

Target a scenario such as:

- 5,000 vs 5,000 units
- active target acquisition
- movement
- firing
- projectile simulation

If this is too expensive at current architecture, scale down until measurable and document the bottleneck honestly.

## Required Review

Invoke Codex after the combat benchmark and ask specifically about:

- target acquisition scaling
- projectile scaling
- spatial queries
- allocation pressure
- faction/content coupling
- economy determinism
- benchmark realism

## Completion Criteria

This goal is complete when:

- all three resources function
- all three factions have a minimal unit identity
- units can be produced and fight
- combat uses scalable purpose-built projectile logic
- combat benchmark exists
- Codex review has been evaluated
- documentation/state are updated

Do not begin Goal 04 until every completion criterion above is verified and the durable state/docs are updated. When continuous sequence mode is authorized in `docs/EXECUTION_LEDGER.md`, advance exactly once to Goal 04 after this gate closes; otherwise stop at the verified boundary.
