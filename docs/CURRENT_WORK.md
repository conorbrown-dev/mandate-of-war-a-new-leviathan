# Current Work

## Current Initiative

Goal 0A established the reproducible foundation and Goal 0B narrowed the
Godot/GDExtension boundary. Both are complete; no successor work package is
currently assigned. [STATUS.md](STATUS.md) is the canonical current-state
document.

When a successor architecture/refactoring task is explicitly assigned, it
should preserve these operating constraints:

- the simulation is understandable and maintainable by a human developer;
- simulation code is cleanly separated from Godot presentation/integration;
- systems have explicit ownership and boundaries;
- unit behavior does not depend on deeply coupled scene/node logic;
- high-scale simulation remains the primary architectural constraint;
- MIRIAM/Codex can modify individual systems without needing to reason about the entire game;
- automated validation protects behavior during refactoring.

## Important

Do not infer work from historical numbered goals, `NEXT_TASKS.md`, or an
absent `docs/REFACTOR_PLAN.md`. Do not begin gameplay feature work or a broad
engine refactor until a successor task specifies its allowed modules,
non-goals, acceptance criteria, and validation evidence.
