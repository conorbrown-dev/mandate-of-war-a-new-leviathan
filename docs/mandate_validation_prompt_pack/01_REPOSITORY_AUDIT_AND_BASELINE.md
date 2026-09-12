# Prompt 01 — Repository Audit and Validation Baseline

You are working in the **Mandate of War** repository, a large-scale near-future RTS built in Godot.

Your task is to prepare for an automated gameplay-validation framework.

## Goal

Inspect the repository and produce a factual implementation plan based on what actually exists.

Do **not** begin large-scale implementation in this step.

## Required investigation

Determine:

- Godot version and renderer.
- Whether the project uses GDScript, C#, or both.
- Existing test frameworks and test directories.
- Whether GdUnit4 is already installed.
- Existing CI scripts.
- Existing developer scripts under `tools/`, `scripts/`, or equivalent.
- Existing logging, metrics, debug overlays, replay, deterministic simulation, random-seed, or benchmark systems.
- Existing screenshot or video utilities.
- Existing scene-loading/bootstrap conventions.
- Existing command-line arguments or debug/test modes.
- How gameplay scenes are launched.
- How maps are created and loaded.
- How units are spawned.
- How orders are issued.
- Whether simulation time can be controlled independently of rendering.
- Whether the repository already has stable IDs for units/entities/content.
- Whether tests can currently run headlessly.

Search the codebase before proposing new abstractions. Reuse existing systems wherever reasonable.

## Deliverable

Create:

```text
docs/validation/VALIDATION_BASELINE.md
```

It must contain:

### Existing project facts

Record concrete paths/classes/scripts for each relevant existing system.

### Gaps

Identify only gaps necessary for automated gameplay validation.

### Proposed minimal architecture

Describe the smallest system that could provide:

```text
scenario
  -> controlled setup
  -> scripted actions
  -> state assertions
  -> screenshots
  -> report
```

Do not design an enormous general-purpose framework.

### Proposed file layout

Use existing repository conventions where possible.

### Risks

Call out anything that threatens deterministic testing, such as:

- global RNG;
- dependence on wall-clock time;
- asynchronous jobs with no synchronization point;
- physics/render timing dependencies;
- hidden singleton state;
- persistent user settings;
- procedural maps without seeds.

### Recommended first scenario

Select a **small existing behavior**, not a future feature, to serve as the smoke test for the validation framework.

Good examples include:

- load a tactical map;
- spawn one unit;
- select a unit;
- issue a move command;
- verify it moved.

Choose the smallest scenario that exercises the real game.

## Constraints

Do not:

- rewrite gameplay architecture;
- change game behavior merely to make it testable;
- install dependencies without checking existing project conventions;
- invent class names or paths in the report;
- claim a feature exists without locating it.

## Completion gate

Before marking this task complete:

1. Confirm the project opens or parses successfully using the repository's normal workflow.
2. Run any existing test suite if one exists.
3. Record the exact commands used.
4. Record existing failures separately from changes introduced by this task.

Your final response must state:

```text
AUDIT RESULT: COMPLETE
Baseline project status:
Existing tests:
Recommended validation smoke scenario:
Files created/changed:
Commands run:
Known blockers:
```
