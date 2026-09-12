# MIRIAM — Token and Iteration Efficiency Rules

Use this as persistent guidance for MIRIAM/OpenCode when working on Mandate of War.

## Primary objective

Do not solve gameplay bugs by repeatedly reading large portions of the repository and making speculative edits.

Use the validation harness as the feedback loop.

## Narrow-first workflow

For each task:

```text
locate relevant production code
-> locate relevant validation scenario
-> make smallest coherent implementation
-> run narrow validation
-> inspect report/log
-> fix one demonstrated failure
-> rerun narrow validation
```

Only after the narrow scenario passes should you run broader regression.

## Search discipline

Prefer targeted searches for:

```text
class/type name
stable content ID
signal
order type
scenario ID
error text
method name from stack trace
```

Do not reread giant design documents or unrelated subsystems on every iteration.

## Log discipline

Do not dump entire engine logs into reasoning when a targeted error section is enough.

On a failed run inspect, in order:

1. failed assertion;
2. scenario diagnostics;
3. stack trace/error;
4. state snapshot relevant to failure;
5. broader log only if necessary.

## Visual discipline

Do not regenerate a movie after every code change.

Use:

```text
state assertions -> screenshots -> video
```

in increasing order of cost.

Use video when the failure involves timing, motion, sequencing, formation behavior, projectiles, or camera transitions that static evidence cannot explain.

## Benchmark discipline

Do not run 10k/25k/50k-unit benchmarks while fixing a one-unit functional bug.

Use a small correctness scenario first.

Run scale tests only after functional correctness.

## Completion discipline

Never say:

```text
should work
appears correct
likely fixed
build passes so complete
```

when a validation scenario exists.

Say:

```text
Scenario X PASS
```

and include the report path.

If validation could not be run, state that explicitly and do not convert it into PASS.
