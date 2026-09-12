# Prompt 06 — Visual Regression

## Goal

Add optional screenshot comparison without turning rendering differences into constant false failures.

The system must support:

```text
reference image
actual image
difference output
numeric similarity/difference result
threshold
PASS/FAIL
```

## Architecture

Keep image comparison outside the core gameplay simulation unless there is a strong repository-specific reason otherwise.

A small tool under `tools/` is preferred.

Use a lightweight dependency already available in the repository where practical. If a new dependency is necessary, document it and keep it minimal.

## Baseline storage

Generated run screenshots:

```text
validation/artifacts/...
```

Committed reference images:

```text
validation/baselines/<scenario-id>/...
```

or a repository-equivalent explicit path.

Do not mix baselines with transient artifacts.

## Comparison behavior

Do **not** use exact byte or pixel equality as the only 3D gameplay comparison.

Provide one or more of:

- normalized pixel difference;
- perceptual similarity;
- tolerance per channel;
- region-of-interest comparison;
- masks for intentionally dynamic regions.

The comparison result must include enough information to diagnose a failure.

Generate a diff image when comparison fails, and preferably on demand for passing runs.

## Baseline update safety

Baseline creation/update must require an explicit command or flag.

Bad:

```text
test failed -> automatically replace expected image with actual
```

Good:

```bash
tools/validate strategic_zoom --update-baseline
```

The tool must clearly state that references changed.

An agent is not allowed to update a baseline merely to make a failed test pass unless the visual change is an explicit part of the assigned goal.

## Manifest

A scenario should be able to associate:

```text
checkpoint -> baseline -> comparison settings
```

Conceptual example:

```json
{
  "checkpoint": "strategic_zoom",
  "baseline": "strategic_zoom.png",
  "threshold": 0.96
}
```

Do not blindly choose `0.96`; establish thresholds empirically for each kind of image.

## First visual test

Pick a visually stable existing screen/checkpoint.

Run:

1. actual against its accepted reference;
2. a deliberately modified/corrupted reference proving FAIL;
3. the restored reference proving PASS.

Produce and inspect the diff.

## Completion response

```text
VISUAL REGRESSION RESULT: PASS | FAIL

Scenario/checkpoint:
Comparison algorithm:
Threshold:
Passing score:
Intentional failure score:
Diff path:
Baseline path:
Baseline update command:
Files changed:
Known sources of rendering variance:
```
