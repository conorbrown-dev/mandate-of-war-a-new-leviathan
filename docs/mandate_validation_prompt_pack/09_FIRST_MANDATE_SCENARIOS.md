# Prompt 09 — First Real Mandate Validation Scenarios

## Goal

Use the completed harness on actual Mandate gameplay.

Do not create fake miniature implementations of these systems. Test the real production path that currently exists.

Implement only scenarios for features that already exist enough to test. If one listed scenario does not yet exist in production, mark it NOT YET APPLICABLE and choose another existing feature.

## Priority scenarios

Implement approximately three meaningful scenarios.

### A. Selection and move order

Expected evidence:

```text
unit exists
-> unit selected through real input/selection path
-> move order issued
-> order accepted
-> unit changes position
-> unit reaches destination tolerance
```

Capture screenshots at selection, order, and arrival if practical.

### B. Strategic zoom

If strategic zoom currently exists:

```text
start at tactical zoom
-> known units visible
-> zoom outward through real camera/input path
-> reach strategic threshold
-> expected icon/detail state changes
-> selection/commandability remains valid if designed to do so
```

State assertions should inspect actual camera/representation state. Screenshots should show near and strategic views.

Do not validate strategic zoom only by comparing images.

### C. Existing combat or logistics behavior

Choose whichever is most mature in the repository.

Examples:

```text
weapon fires -> projectile created -> target damaged

or

aircraft launches -> endurance decreases -> return behavior occurs

or

unit consumes/resupplies Material/Energy
```

Again: only test implemented behavior.

## Naming

Give each scenario a stable semantic ID.

Examples:

```text
basic_selection_move
strategic_zoom_transition
basic_projectile_hit
```

Do not include temporary ticket numbers in scenario IDs.

## Acceptance criteria

Each scenario must include:

- deterministic seed when applicable;
- bounded runtime;
- at least two state assertions;
- diagnostic failure messages;
- screenshots where visually relevant;
- a machine-readable report;
- non-zero exit on failure.

At least one scenario should support video recording.

## Failure proof

For each scenario, during development temporarily alter one expected value or condition and prove that the test detects failure. Revert the intentional failure.

## Documentation

Create:

```text
docs/validation/SCENARIO_CATALOG.md
```

For each scenario document:

```text
ID
purpose
production systems exercised
setup
actions
assertions
visual checkpoints
performance metrics if any
expected runtime
how to run
```

## Completion response

```text
MANDATE SCENARIOS RESULT: PASS | FAIL

Scenarios implemented:
1.
2.
3.

For each:
- state result:
- screenshots:
- video support:
- intentional failure detection:
- report:

Files changed:
Deferred scenarios and why:
```
