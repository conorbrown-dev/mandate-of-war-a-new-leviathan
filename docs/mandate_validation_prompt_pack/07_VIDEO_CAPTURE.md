# Prompt 07 — Deterministic Video Evidence

## Goal

Allow selected gameplay validation scenarios to produce deterministic or fixed-step video evidence.

Use Godot's Movie Maker / command-line movie recording capabilities rather than screen-recording software unless repository constraints make that impossible.

Current Godot command-line support includes concepts equivalent to:

```text
--write-movie
--fixed-fps
--quit-after
```

Verify the exact options against the installed Godot version before committing scripts.

## Desired workflow

Conceptually:

```bash
tools/validate carrier_recovery --record
```

should:

1. launch the requested scenario;
2. use a known seed;
3. run at a known simulation/capture rate;
4. terminate automatically;
5. write the video beneath that run's artifact directory;
6. record the artifact in `report.json`.

## Important separation

Video capture is **optional evidence**.

The scenario's functional assertions remain the source of PASS/FAIL unless the specific acceptance criterion is inherently visual.

A beautiful video does not prove correct fuel calculations, pathfinding architecture, logistics state, or performance.

## Recording metadata

Record at least:

- file path;
- resolution;
- FPS;
- seed;
- scenario ID;
- approximate frame count or duration.

## Scenario automation

A recorded validation must not require a human to press buttons to start/stop the scenario.

It should automatically progress and exit.

Where a visual recording requires a normal rendering display mode rather than the project's headless test mode, document that distinction.

## Failure behavior

If recording is requested but video creation fails:

- the run must not falsely claim the video exists;
- report the capture error;
- make the overall validation command fail if recording was required by the invocation.

## Demonstration

Record one short existing scenario.

The video should visibly show at least two meaningful scenario states and end automatically.

## Completion response

```text
VIDEO EVIDENCE RESULT: PASS | FAIL

Scenario:
Seed:
FPS:
Resolution:
Frames/duration:
Video path:
Functional assertions:
Automatic termination verified:
Files changed:
Known platform limitations:
```
