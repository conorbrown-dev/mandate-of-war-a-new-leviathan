# Near-Future Large-Scale RTS — Project Charter

## Role

You are the primary implementation agent for a new large-scale near-future RTS inspired by the strategic scale and systemic simulation of Supreme Commander: Forged Alliance.

This is not a clone. Create original factions, names, assets, code, maps, and mechanics.

## Core Vision

Build a cross-platform RTS for Windows, macOS, and Linux with:

- Supreme Commander-style strategic zoom
- massive unit counts and sprawling maps
- land, air, and naval warfare
- physical/projectile-based combat where practical
- logistics and operational range as first-class strategy
- three asymmetric fictional factions
- Material, Energy, and Research resources
- T1–T4 technology progression
- built-in map editor
- modding support
- LAN/direct-connect multiplayer
- replays and historical stats
- offline-capable modern AI
- automated/semi-automated 3D asset pipeline

## Setting

Use a near-future military setting, roughly 2035–2055.

Equipment should be fictional but may be visually and doctrinally inspired by historical and modern military designs such as:

- P-51 Mustang
- P-38 Lightning
- B-17 Flying Fortress
- Zero
- Spitfire
- Mosquito
- Lancaster
- Me 262
- Ju 87
- F-14 / F-15 / F-16 / F/A-18
- A-10
- SR-71
- Abrams / Leopard / Sherman / T-34 / Bradley / BMP lineages
- destroyers, cruisers, submarines, battleships, carriers, landing craft

Do not directly copy proprietary assets, branding, names, or copyrighted creative expression. Use fictional descendants and original designs.

## Factions

### Faction A — Elite Precision

- expensive
- low unit count
- high survivability
- precision firepower
- excellent sensors
- advanced aircraft and active defenses

### Faction B — Mass Warfare

- cheap units
- rapid production
- very high unit counts
- attrition-friendly
- cheap logistics and transports
- map control through numbers

### Faction C — Industrial / Experimental

- balanced early and mid game
- superior infrastructure scaling
- advanced Material/Energy/Research generation
- strong long-game technology
- exceptional T4 experimental units

## Resources

### Material
Physical manufacturing resources, ammunition, construction feedstock, spares.

### Energy
Fuel, power generation, electrical capacity, reactor output, synthetic fuels.

### Research
R&D capacity used for upgrades, doctrines, advanced systems, and T3/T4 technology.

Keep all balance/content data outside core simulation code and validate it with schemas.

## Engineering Principles

Prefer:

- data-oriented architecture
- simulation/presentation separation
- fixed simulation tick independent of rendering
- cache-friendly structures
- batching
- worker threads/job systems
- scalable spatial partitioning
- explicit state
- automated tests
- headless benchmarks
- profiling before optimization claims

Avoid:

- heavyweight per-unit engine objects/scripts
- per-frame simulation tied to rendering
- naive per-unit A* at large scale
- hard-coded unit definitions
- unnecessary heavyweight rigid-body physics
- replicating every unit transform every network frame

## Scale Targets

Treat these as aspirational until proven by benchmarks:

- 10,000 active units: excellent
- 25,000 active units: practical target
- 50,000 active units: stretch
- 100,000 total simplified entities: investigate

Never claim a target is achieved without a representative benchmark.

## Persistent Project Documents

Maintain these throughout development:

- `AGENTS.md`
- `README.md`
- `docs/ARCHITECTURE.md`
- `docs/CURRENT_STATE.md`
- `docs/NEXT_TASKS.md`
- `docs/PERFORMANCE.md`
- `docs/adr/`

Before every task, read the relevant documents.

At the end of every task, update `docs/CURRENT_STATE.md` with:

- what now exists
- what was actually tested
- benchmark results if relevant
- known limitations/problems
- architectural decisions made
- next recommended task

## External Review Policy

Codex CLI is available as an independent senior reviewer.

Use Codex at important checkpoints, including:

- engine/architecture selection
- simulation-core completion
- major performance milestones
- pathfinding architecture
- networking/determinism design
- logistics architecture
- before declaring a major milestone complete
- after 2–3 unsuccessful attempts at the same difficult bug

Codex should normally REVIEW rather than directly implement.

Ask it to focus on:

1. correctness
2. scalability risks
3. memory/allocation problems
4. engine-object overhead
5. threading hazards
6. simulation determinism
7. architectural coupling
8. test/benchmark gaps

Do not blindly accept review findings. Validate them using code, tests, profiler data, and benchmarks.

Avoid recursive agent conversations. One review pass followed by implementation and verification is preferred.

## Workflow

For every milestone:

1. Inspect repository and current state.
2. Read the charter and relevant architecture docs.
3. Plan only the requested milestone.
4. Implement incrementally.
5. Build.
6. Run tests.
7. Run representative benchmarks where applicable.
8. Inspect failures/profiler output.
9. Fix warranted issues.
10. Review the diff.
11. Invoke Codex review if the milestone requires it.
12. Evaluate and fix warranted review findings.
13. Rerun tests/benchmarks.
14. Update docs.
15. Commit logical changes if appropriate.

Do not expand scope merely because future features are described in this charter.
