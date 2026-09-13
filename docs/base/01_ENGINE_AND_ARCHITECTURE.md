# Goal 1 — Engine Selection and Architecture Bootstrap

Read `00_PROJECT_CHARTER.md` first.

## Objective

Determine whether the project can be built on an architecture capable of Supreme Commander-scale warfare without painting the project into a corner.

Do not build substantial gameplay yet.

## Tasks

1. Inspect the development machine, repository, installed SDKs, compilers, engines, build tools, and relevant hardware.
2. Evaluate viable technology choices, including as appropriate:
   - Godot
   - Unreal Engine
   - Unity
   - custom/native engine
   - hybrid engine + custom simulation core
3. Evaluate them specifically for:
   - massive unit simulation
   - multithreading
   - data-oriented architecture
   - deterministic/near-deterministic simulation
   - large-scale pathfinding
   - strategic zoom
   - custom rendering flexibility
   - headless simulation
   - networking
   - modding
   - editor extensibility
   - Windows/macOS/Linux support
   - automated asset import/tooling
4. Choose the best architecture and document it in `docs/ENGINE_DECISION.md`.
5. Create `docs/ARCHITECTURE.md` describing:
   - simulation/presentation separation
   - major modules
   - simulation tick model
   - data/content flow
   - threading strategy
   - expected rendering strategy
   - testing strategy
   - headless simulation strategy
6. Add appropriate ADRs under `docs/adr/`.
7. Bootstrap the selected project/engine.
8. Create or update:
   - `AGENTS.md`
   - `README.md`
   - `docs/CURRENT_STATE.md`
   - `docs/NEXT_TASKS.md`
9. Ensure the empty/bootstrap project builds or launches successfully.

## Constraints

- Do not implement factions, economy, combat, multiplayer, AI, or logistics yet.
- Avoid tying simulation entities directly to heavyweight engine objects.
- The simulation should be designed so it can eventually run headlessly.

## Required Review

After the architecture docs and bootstrap build are complete, invoke Codex CLI for an architecture review.

Ask Codex to inspect:

- `AGENTS.md`
- `docs/ENGINE_DECISION.md`
- `docs/ARCHITECTURE.md`
- ADRs
- repository structure
- current diff

Request findings specifically about scaling, determinism, engine coupling, headless simulation, and long-term portability.

Evaluate findings and fix only those that are warranted.

## Completion Criteria

This goal is complete only when:

- an engine/stack is selected and justified
- the project builds/runs
- simulation and presentation boundaries are documented
- headless simulation has a credible architectural path
- Codex review has been completed and evaluated
- `docs/CURRENT_STATE.md` reflects reality

Stop after this goal.
