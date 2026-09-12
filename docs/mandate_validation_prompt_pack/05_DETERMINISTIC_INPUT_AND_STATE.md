# Prompt 05 — Deterministic Input and State Helpers

## Goal

Make validation scenarios reproducible enough that failures can be investigated instead of disappearing on rerun.

This step should add **test orchestration controls**, not rewrite production simulation.

## Required capabilities

### Seed control

Where the game uses randomness, provide a validation-run seed that can be recorded and replayed.

Every scenario report should contain the effective seed.

If a subsystem uses an independent RNG, document it and ensure validation can seed it when practical.

Do not silently force production gameplay to use one global RNG if that damages architecture.

### Simulation progression

Avoid arbitrary wall-clock waits such as:

```text
sleep 5 seconds
hope unit arrived
```

Prefer:

- advancing a known number of simulation frames/ticks;
- waiting for an explicit signal;
- polling a state predicate with a bounded timeout;
- waiting for the job system to reach a known synchronization point.

Create helpers that make these patterns concise.

### Input helpers

Use the project's real input actions/order APIs wherever possible.

Support the minimum required RTS interactions discovered in the repository, for example:

- click/select;
- drag selection if already implemented;
- issue move;
- issue attack or attack-move;
- zoom camera;
- pan camera;
- press mapped gameplay action.

Do not bypass the command pipeline for tests whose purpose is to validate the input-to-command flow.

It is acceptable for lower-level simulation tests to invoke the command API directly when UI/input is not what is being tested.

### State queries

Provide reusable queries/helpers for common assertions without exposing giant amounts of implementation detail.

Potential examples, only if applicable to current code:

```text
entity exists
entity position
entity alive/dead
selected entity count
active order type
resource amount
fuel/endurance amount
current recovery target
camera zoom/altitude
strategic icon active
```

Prefer stable IDs and domain-facing state over fragile scene-tree paths.

## Bounded waits

Every asynchronous wait must have a timeout or frame/tick limit.

On timeout, include diagnostic data in the failure message.

Bad:

```text
wait until unit arrives forever
```

Good:

```text
wait up to 600 simulation ticks for unit to reach target
on failure: report current position, target, order state, velocity
```

## Demonstration

Update the existing smoke scenario so that running it twice with the same seed produces the same meaningful outcome.

Where exact floating point values are inappropriate, use explicit tolerances.

## Completion response

```text
DETERMINISM RESULT: PASS | FAIL

Scenario:
Seed:
Run 1:
Run 2:
Observed reproducibility:
Input path exercised:
State path asserted:
Timeout behavior tested:
Files changed:
Known nondeterministic systems:
```
