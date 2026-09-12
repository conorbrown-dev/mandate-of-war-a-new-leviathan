# Reusable Prompt — Implement a Gameplay Goal with Validation

Replace `<GOAL>` and `<ACCEPTANCE_CRITERIA>` before giving this to Codex or MIRIAM.

---

You are implementing the following Mandate of War gameplay goal:

## Goal

<GOAL>

## Acceptance criteria

<ACCEPTANCE_CRITERIA>

## Required workflow

Before editing:

1. Search the existing codebase for the production systems involved.
2. Search `docs/validation/SCENARIO_CATALOG.md`.
3. Identify the smallest existing scenario that can be extended, or create one new scenario if necessary.
4. State internally what observable game state proves each acceptance criterion.

Do not treat compilation as proof.

## Implementation rules

Use the real production architecture.

Do not create test-only gameplay paths merely to satisfy validation.

Preserve Mandate architectural priorities:

- Godot;
- very high unit counts;
- data-driven content/modding;
- physical/custom-math projectiles where practical;
- shared/hierarchical pathfinding rather than per-unit full-map A*;
- deterministic/command-oriented multiplayer and replay compatibility where practical;
- logistics as real gameplay state;
- stable content/entity IDs.

Avoid heavyweight per-unit Nodes/physics/allocations when the surrounding architecture is data-oriented.

## Validation requirements

Create or update a scenario that verifies the goal.

Where applicable include:

### State

Verify the actual state change.

Examples:

```text
order assigned
target reached
damage applied
endurance decreased
recovery point selected
intel state changed from CURRENT -> STALE
unit representation changed at strategic zoom
```

### Behavior

Exercise the real sequence of actions.

Prefer real input/command paths when the acceptance criterion includes UI/input behavior.

### Visual evidence

Capture screenshots at meaningful checkpoints when the feature has visible output.

Do not capture screenshots merely to satisfy a checkbox.

### Performance

If the feature is scale-sensitive, record relevant counters/timing and ensure the implementation does not introduce obviously O(N) expensive work where shared/batched work is expected.

### Video

Record video only if movement/timing/sequence is difficult to judge from state + screenshots.

## Debug discipline

If validation fails:

```text
read failure
-> inspect relevant state/log
-> form one hypothesis
-> make smallest fix
-> rerun only the narrow scenario
```

Do not repeatedly run every test or regenerate expensive video while iterating.

Do not weaken validation to match the implementation.

## Final regression

Once the narrow scenario passes:

1. run related validation scenarios;
2. run the appropriate broader automated test suite;
3. run any relevant benchmark if the feature is scale-sensitive.

## Required completion response

Use this exact structure:

```text
IMPLEMENTATION RESULT: PASS | FAIL

Goal:
Implementation summary:

Validation:
- scenario(s):
- seed(s):
- state assertions:
- screenshots:
- video:
- performance:
- related regression:

Evidence paths:

Files changed:

Known limitations:

Acceptance criteria:
[PASS/FAIL] criterion 1
[PASS/FAIL] criterion 2
...

FINAL STATUS: PASS | FAIL
```

If a required acceptance criterion fails, `FINAL STATUS` must be FAIL.
