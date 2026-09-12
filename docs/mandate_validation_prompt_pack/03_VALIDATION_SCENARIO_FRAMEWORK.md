# Prompt 03 — Validation Scenario Framework

Read the validation baseline and test-foundation documentation before changing code.

## Goal

Create a small, reusable abstraction for **Mandate validation scenarios**.

This is not a second game framework. It is orchestration around the real game.

## Desired conceptual model

```text
ValidationScenario
    setup
    checkpoints
    scripted actions
    assertions
    metrics
    teardown
        |
        v
ValidationResult
    status
    scenario
    seed
    assertions[]
    artifacts[]
    metrics{}
    errors[]
```

Use names appropriate to the existing codebase. The names above are conceptual, not mandatory.

## Required features

Each scenario must be able to declare or record:

- scenario ID;
- human-readable description;
- deterministic seed where applicable;
- start time and duration;
- individual assertion results;
- checkpoint names;
- artifact paths;
- warnings/errors;
- final PASS/FAIL.

Create a machine-readable report, preferably:

```text
validation/artifacts/<scenario>/<run-id>/report.json
```

Use a predictable schema.

A scenario MUST fail if a required assertion fails.

## Assertion model

Support at least:

```text
true/false condition
equality
numeric tolerance/range
not-null / entity exists
```

Do not rebuild the entire assertion library if the test framework already provides this. The validation report may adapt existing assertion outcomes.

## Artifact layout

Use a stable layout such as:

```text
validation/artifacts/
  <scenario-id>/
    <run-id>/
      report.json
      engine.log
      screenshots/
      diffs/
      video/
```

Generated artifacts should normally be gitignored.

Reference/baseline images, if later introduced, belong in source control under a separate explicit path.

## Scenario discovery

Prefer an explicit registry or manifest over magical filesystem scanning if that better matches the current codebase.

The long-term command should be approximately:

```bash
tools/validate <scenario-id>
```

At this stage it is acceptable for the wrapper to invoke the test framework underneath.

## Exit behavior

The validation command must return:

```text
0 = scenario passed
non-zero = scenario failed or could not execute
```

A report-generation failure is itself a failure.

## First scenario

Migrate or wrap the existing smoke scenario from Prompt 02 so it produces a `report.json`.

The report must reflect an intentionally introduced assertion failure during development, proving that false positives do not produce PASS. Revert the intentional failure afterward.

## Report schema

At minimum:

```json
{
  "schemaVersion": 1,
  "scenarioId": "example",
  "runId": "...",
  "status": "PASS",
  "seed": 12345,
  "startedAt": "...",
  "durationMs": 1234,
  "assertions": [
    {
      "id": "unit-moved",
      "status": "PASS",
      "message": "..."
    }
  ],
  "metrics": {},
  "artifacts": [],
  "errors": []
}
```

Add fields only when they provide clear value.

## Constraints

Do not:

- duplicate the actual gameplay simulation;
- create test-only implementations of production behavior;
- swallow exceptions and convert them into PASS;
- treat "scene loaded" as proof that gameplay works;
- make tests pass by changing acceptance criteria.

## Completion gate

Run:

1. the normal passing scenario;
2. a temporary intentionally failing assertion;
3. the restored passing scenario.

Verify the process exit status and report status agree in all cases.

Final response:

```text
SCENARIO FRAMEWORK RESULT: PASS | FAIL

Scenario:
Passing run:
Intentional failure proven:
Restored passing run:
Report path:
Files changed:
Known limitations:
```
