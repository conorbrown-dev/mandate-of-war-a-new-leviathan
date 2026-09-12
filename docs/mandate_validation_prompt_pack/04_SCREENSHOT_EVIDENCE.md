# Prompt 04 — Screenshot Evidence

## Goal

Add named screenshot checkpoints to Mandate validation scenarios.

Screenshots are evidence for humans and vision-capable agents. They do not replace state assertions.

Godot can capture the active viewport through the viewport texture. Ensure capture occurs after a rendered frame has completed so the image is not blank/stale.

## Required API behavior

A scenario should be able to request conceptually:

```text
checkpoint("unit_selected")
checkpoint("move_order_issued")
checkpoint("unit_arrived")
```

and produce:

```text
screenshots/
  01_unit_selected.png
  02_move_order_issued.png
  03_unit_arrived.png
```

The report must list these artifact paths.

## Capture requirements

Screenshots must:

- use the real game viewport;
- have deterministic/stable naming;
- wait for an appropriate rendered frame before capture;
- detect save failures;
- create directories safely;
- avoid overwriting another run;
- record dimensions;
- record checkpoint name;
- preserve enough UI to inspect gameplay unless a scenario explicitly asks for a world-only capture.

If rendering is unavailable, the screenshot checkpoint must be reported as unavailable/failure according to whether it is required. Never fabricate a screenshot artifact.

## Stable capture mode

Add the smallest practical mechanism for test capture consistency.

Consider, where already supported by the project:

- fixed validation resolution;
- stable UI scale;
- known camera transform;
- deterministic map/seed;
- optional disabling of irrelevant animation/noise.

Do not globally degrade the actual game's visual settings just to make tests easier.

## Smoke scenario enhancement

Enhance the existing validation smoke scenario to capture at least two meaningful checkpoints.

Examples:

```text
before action
after successful action
```

The state assertion must still determine functional PASS/FAIL.

## Artifact metadata

Add screenshot artifact entries to `report.json`, conceptually:

```json
{
  "kind": "screenshot",
  "checkpoint": "unit_arrived",
  "path": "screenshots/03_unit_arrived.png",
  "width": 1920,
  "height": 1080
}
```

## Validation

Run the scenario and verify:

- files actually exist;
- files have non-zero size;
- dimensions are correct;
- images are not trivially blank if a simple sanity check is practical;
- report paths resolve to the created files.

Do not manually declare success without opening/inspecting at least one produced screenshot.

## Completion response

```text
SCREENSHOT EVIDENCE RESULT: PASS | FAIL

Scenario:
Checkpoints captured:
Resolution:
Artifact directory:
State assertions:
Screenshot sanity checks:
Files changed:
Known limitations:
```
