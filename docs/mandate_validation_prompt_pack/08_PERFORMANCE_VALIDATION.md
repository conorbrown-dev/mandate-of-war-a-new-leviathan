# Prompt 08 — Performance Validation

## Goal

Add performance acceptance criteria to Mandate gameplay validation without confusing rendering FPS with simulation scalability.

Mandate targets:

- 10,000 active units as practical;
- 25,000 active units as a major target;
- 50,000 active units as a stretch goal.

This prompt does **not** require achieving those unit counts now. It establishes repeatable measurement.

## Metrics

Inspect existing profiling/telemetry first.

Capture the smallest useful set of metrics available from the real engine/game architecture, such as:

- total frame time;
- simulation/update time;
- rendering time;
- navigation/path job time;
- AI time;
- projectile update time;
- active entity count;
- active projectile count;
- memory usage where practical;
- path requests/jobs;
- expensive fallback operations.

Do not fabricate metrics that the game cannot currently measure.

## Functional vs benchmark scenarios

Keep these distinct.

A functional scenario answers:

> Is the behavior correct?

A benchmark scenario answers:

> How much does this behavior cost?

They may share setup code but should not become the same pass/fail signal unless a performance threshold is deliberately part of the feature contract.

## Warm-up

Avoid measuring only startup/import/shader compilation where it would distort the gameplay metric.

Where appropriate:

```text
setup
warm-up
measurement window
teardown
```

Record the methodology.

## Report

Add benchmark metrics to the validation report.

Conceptual example:

```json
{
  "metrics": {
    "entityCount": 1000,
    "measurementTicks": 600,
    "simulationMsAvg": 4.1,
    "simulationMsP95": 6.8,
    "simulationMsMax": 12.9,
    "pathRequests": 14
  }
}
```

Prefer average plus percentile/max rather than one instantaneous sample.

## Architecture guardrails

Where the repository exposes a meaningful counter, add invariants that catch catastrophic scaling mistakes.

Examples:

```text
formation order must not trigger one full-map path search per unit
idle units must not continuously enqueue path jobs
offscreen strategic icons must not allocate every frame
```

Only add guardrails backed by actual instrumentation.

## First benchmark

Use a modest count the current project can reliably support.

The goal is repeatability, not an impressive number.

Run it at least three times and report variance.

## Completion response

```text
PERFORMANCE VALIDATION RESULT: PASS | FAIL

Benchmark:
Entity count:
Measurement window:
Runs:
Metrics:
Variance:
Architecture counters:
Artifact/report paths:
Files changed:
Known measurement limitations:
```
