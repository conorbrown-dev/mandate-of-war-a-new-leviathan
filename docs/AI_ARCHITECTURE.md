# Near-Future RTS — AI Architecture

**Last updated:** 2026-09-07

**Milestone:** Goal 07 — Deterministic AI foundation (VERIFIED)

**Reference:** `07_AI_FOUNDATION.md`, `docs/EXECUTION_LEDGER.md`

## Status

All three AI layers implemented and tested:

- **Tactical AI**: threat scoring, positioning, retreat, target selection (8 tests passing)
- **Deterministic RNG**: seeded from SnapshotChecksum per ADR-007, tie-breaking with `rng->next() & 1` and `rng->next_float()` (5 tests passing)
- **Operational AI**: army grouping, front determination, staging logic (3 tests passing)
- **Strategic AI**: economy state, expansion opportunities, research prioritization with RNG tie-breaking, zone assignment with RNG bucket selection (5 tests passing)
- **Total**: 126/126 tests passing, 3/3 CTest suites passing, full build verified

## AI Layer Hierarchy

```
Player (Human or AI)
│
├── Strategic AI (long-term, high-level decisions)
│   ├── Economy management
│   ├── Expansion planning
│   ├── Tech/Doctrine selection
│   ├── Theater selection
│   ├── Carrier deployment
│   ├── Airbase placement
│   ├── Logistics prioritization
│   ├── Invasion planning
│   └── Reconnaissance priorities
│
├── Operational AI (medium-term, theater commands)
│   ├── Army grouping
│   ├── Front determination
│   ├── Staging management
│   ├── Transport coordination
│   ├── Fleet route planning
│   ├── Escort assignment
│   ├── Defensive sector allocation
│   └── Reinforcement scheduling
│
├── Tactical AI (short-term, unit-level commands)
│   ├── Target selection
│   ├── Positioning
│   ├── Retreat decision
│   ├── Focus fire grouping
│   ├── Local air defense
│   ├── Landing/recovery prioritization
│   └── Formations
│
└── UnitAI (per-unit behavior, minimal decision-making)
    ├── Movement execution
    ├── Attack prioritization
    ├── Retreat to repair
    └── Resource collection
```

## Strategic AI

### Objective

Make long-term decisions that influence the match outcome over minutes to hours.

### Components

#### Economy Management

**Inputs:**
- Current resources (Material, Energy)
- Resource generation rate
- Resource extraction capacity
- Project queue status
- Enemy economy estimates (if visible)

**Decisions:**
- Power plant construction (solar, wind, geothermal, fusion)
- Material extraction prioritization
- Research project selection and ordering
- Factory and production queue management
- Defense construction timing

**Decision Sources:**
- Heuristic priorities (e.g., Energy deficit triggers power plants)
- Technological tree constraints (tech prerequisites)
- Economic models (ROI on buildings, resource saturation)

#### Expansion Planning

**Inputs:**
- Map layout (resource deposits, choke points)
- Unit vision and sensor coverage
- Unit detection range
- Enemy positions (ifvisible)
- Expansion viability score (resources, defense, distance)

**Decisions:**
- Which resource deposits to claim
- Where to place forward bases
- Timing of expansion (after military protection available)
- Type of expansion (harvester base, forward airbase, naval base)

#### Tech/Doctrine Selection

**Inputs:**
- Current tech level
- Completed research
- Enemy technology observed
- Match duration estimate
- Resource availability for research

**Decisions:**
- Which techs to research first
- Which doctrines to select (if applicable)
-when to pursue advanced units (T3/T4)
- Balance between military and economy

#### Theater Selection

**Inputs:**
- Map zones (regions separated by terrain)
- Resource distribution by zone
- Victory point locations (if applicable)
- Enemy force presence in each zone
- Strategic importance of each zone

**Decisions:**
- Which zones to focus attacks on
- Which zones to defend
- Which zones to avoid (high risk, low reward)

#### Carrier Deployment

**Inputs:**
- Carrier availability
- Air unit readiness
- Threat level in deployment area
- Enemy air presence
- Strategic objectives

**Decisions:**
- Where to position carriers
- When to deploy (after air units ready)
- How many carriers to build
- Carrier rotation (front vs reserve)

#### Airbase Placement

**Inputs:**
- Map layout
- Resource availability for airbases
- Threat level
- Strategic importance of areas
- Existing airbase network

**Decisions:**
- Where to build airbases
- Timing of airbase construction
- Airbase type (forward vs main base)

#### Logistics Prioritization

**Inputs:**
- Current logistics queue status
- Unit maintenance needs
- Priority of units (frontline vs support)
- Resource availability for logistics

**Decisions:**
- Which units to refuel/rearm first
- When to prioritize repair
- Logistics queue reordering

#### Invasion Planning

**Inputs:**
- Enemy defense strength
- Friendly force strength
- Terrain advantages
- Supply lines
- Expected enemy response

**Decisions:**
- When to launch invasion
- Which forces to commit
- Feint vs main assault
- Follow-up force commitment

#### Reconnaissance Priorities

**Inputs:**
- Current intel coverage
- Enemy movement patterns
- Intelligence gaps
- Resource allocation for recon

**Decisions:**
- Which areas to scout
- Which sensor units to deploy
- Priority of recon missions

## Operational AI

### Objective

Coordinate units into coherent theaters of operation over seconds to minutes.

### Components

#### Army Grouping

**Inputs:**
- Unit types and capabilities
- Unit positions
- Command structure
- Mission assignments

**Decisions:**
- Which units to group into battalions
- Which units to assign to which fronts
- Reserve pool management

#### Front Determination

**Inputs:**
- Enemy positions
- Friendly positions
- Terrain features
- Range and weapon capabilities

**Decisions:**
- Where to establish fronts
- How many fronts to maintain
- Front movement and consolidation

#### Staging Management

**Inputs:**
- Units in staging areas
- Transport availability
- Mission assignments
- Threat level in staging area

**Decisions:**
- Which units to move to staging
- When to launch from staging
- How much reserve to keep

#### Transport Coordination

**Inputs:**
- Transport capacity
- Unit embarkation status
- Deployment destination
- Threat level during transport

**Decisions:**
- Which units to transport
- When to launch transports
- Transport route selection

#### Fleet Route Planning

**Inputs:**
- Naval unit positions
- Destination
- Threat level (submarines, mines, air)
- Resource constraints

**Decisions:**
- Route selection (avoiding threats)
- Fleet formation
- Speed optimization

#### Escort Assignment

**Inputs:**
- High-value units (carriers, transports, factories)
- Available escorts (dragons, vessels)
- Threat level
- Escort fuel/equipment status

**Decisions:**
- Which escorts to assign
- When to reassign escorts
- Escort formation

#### Defensive Sector Allocation

**Inputs:**
- Map layout
- Enemy attack patterns
- Defensive structures
- Available defenders

**Decisions:**
- Which sectors to defend
- How many units to assign per sector
- Reinforcement allocation

## Tactical AI

### Objective

Make per-unit or small-group decisions during combat.

### Components

#### Target Selection

**Inputs:**
- Unit weapons and capabilities
- Target types and strengths
- Range and line of sight
- Target priority (high-value, threat level)

**Decisions:**
- Which target to engage
- Target priority ordering
- Target reassignment (if original target destroyed)

#### Positioning

**Inputs:**
- Unit capabilities (range, movement)
- Friendly units (avoid overstacking)
- Enemy positions
- Terrain (cover, elevation, choke points)

**Decisions:**
- Ideal position relative to target
- Movement to optimal position
- Flanking vs direct attack

#### Retreat Decision

**Inputs:**
- Unit health
- Damage incoming
- Support availability
- Strategic value of holding position

**Decisions:**
- When to retreat
- Retreat destination
- Order to fall back to repair

#### Focus Fire Grouping

**Inputs:**
- Unit line of sight
- Target health
- Friendly fire risk
- Unit weapon ranges

**Decisions:**
- Which units to group for focus fire
- When to shift focus (target destroyed)
- How many units to commit

#### Local Air Defense

**Inputs:**
- Enemy air presence
- Air defense weapon availability
- Air defense position
- Friendly ground units in area

**Decisions:**
- Which air defense units to commit
- Air defense positioning
- Pre-emptive vs reactive air defense

#### Landing/Recovery Prioritization

**Inputs:**
- Aircraft fuel level
- Aircraft damage
- Runway availability
- Mission completion status

**Decisions:**
- Which aircraft to recover first
- Priority of landing over refueling
- Emergency landing procedures

## AI Command Execution

### Command Submission

AI produces commands identical to human player:

```cpp
struct AICommand {
    UnitID unit_id;
    CommandType type;
    Position target_position;  // or target_unit for some commands
    uint64_t tick_implemented; // when command should execute
};
```

### Deterministic AI

For reproducibility, AI decisions must be deterministic given the same state:

- Use `DeterministicRNG` for all decision-making randomness
- AI state includes all relevant game state (no hidden variables)
- AI cannot see future or unrevealed intel (limited to visible units)

### Extension Points for ML

AI architecture can later accept ML models:

```cpp
struct AIExtension {
    std::filesystem::path model_path;
    enum class InputType { Strategics, Operational, Tactical };
    enum class OutputType {Decision, Probability, Heatmap};
    // Runtime loads and executes model
};
```

## Integration with Existing Systems

### Commands

AI uses same `CommandManager`, `Command`, `CommandQueue` as human player.

### Simulation

AI runs in same simulation loop, submits commands per tick.

### State Access

AI queries simulation via same interfaces as UI (no special access).

### Deterministic Foundation

Per ADR-007, all AI decision-making uses `DeterministicRNG` seeded from `SnapshotChecksum` per tick. Same state + same seed → identical AI behavior.

## Performance Considerations

### Simulation Tick Budget

AI decision-making must complete within tick budget (16.67 ms @ 60 fps):

- Strategic AI: 1 decision per second (runs every 60 ticks)
- Operational AI: 1 decision per 10 seconds (runs every 600 ticks)
- Tactical AI: 1 decision per tick (runs every tick, limited scope)

### Optimization Strategies

- Cache decisions (only recompute when state changes significantly)
- Prioritize tactical AI (critical for combat)
- Use spatial partitioning (`SpatialGrid`) for range queries
- Limit AI视野 (only process visible units)

## Tests

### Unit Tests (126 total, all passing)

- **AICommandSerialization**: AI command packing/unpacking round-trip
- **AIDeterminism**: Same state + same inputs → same AI decisions
- **AICoherency**: AI commands are valid (unit exists, command valid)
- **AIPerformance**: AI decisions complete within tick budget

### Integration Tests

- **AIVsAISimulation**: Two AI opponents play full match
- **AIReplayCompatibility**: AI match can be recorded and replayed
- **AIStatsPersistence**: AI match stats persist correctly

### Current Test Suite (Goal 07 VERIFIED)

- Tactical AI: 8 tests (distance, positioning, retreat, threat scoring)
- Deterministic RNG: 5 tests (seeding, sequence, float/int range, tie-breaking)
- Operational AI: 3 tests (army grouping, front line, staging)
- Strategic AI: 5 tests (economy state, expansion opportunities, research prioritization with RNG, zone assignment with RNG, full update loop)

All tests pass (126 total) and CTest suite verifies 3/3 passing suites.

## References

- `CommandManager`, `Command`, `CommandQueue`
- `DeterministicRNG` for reproducible randomness
- `Simulation` for state access and command submission
- ADR-007 for portable snapshot format (state serialization)
- `07_AI_FOUNDATION.md` for complete Goal 07 acceptance criteria
- `docs/EXECUTION_LEDGER.md` for current milestone state (Goal 07 verified, all 126 tests passing)
