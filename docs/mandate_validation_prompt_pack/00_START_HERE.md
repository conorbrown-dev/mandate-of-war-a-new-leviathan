# Mandate of War — Automated Gameplay Validation Prompt Pack

## Purpose

This pack builds a Playwright-like validation workflow for the Godot-based **Mandate of War** project.

The problem this solves is not merely "we need more tests." The immediate problem is that AI coding agents can spend large amounts of time and tokens implementing a gameplay goal, believe the goal is complete because the project compiles, and still leave behavior, visuals, input handling, or performance incorrect.

The validation system must give Codex and MIRIAM objective evidence.

A gameplay goal should eventually be able to produce:

- machine-verifiable state assertions;
- deterministic or reproducible scenario execution;
- screenshots at named checkpoints;
- optional visual-regression comparisons;
- optional deterministic video evidence;
- performance measurements where relevant;
- engine/test logs;
- a machine-readable validation report;
- a clear PASS / FAIL result.

The desired developer experience is approximately:

```bash
./tools/validate strategic_zoom
./tools/validate carrier_recovery
./tools/validate formation_move --record
```

with artifacts such as:

```text
validation/artifacts/strategic_zoom/<run-id>/
  report.json
  engine.log
  test-results/
  screenshots/
    01_near.png
    02_mid.png
    03_strategic.png
  diffs/
  video/
```

## Important philosophy

Screenshots and video are **evidence**, not the only source of truth.

For example, a formation may look correct while every unit performs its own expensive full-map path search. A screenshot cannot detect that architectural failure. Therefore the harness should validate four distinct categories:

1. **Functional state** — did the game actually do the correct thing?
2. **Behavioral flow** — did a scenario progress through the expected states?
3. **Visual output** — did the rendered result look acceptably correct?
4. **Performance / architecture invariants** — did the implementation remain scalable?

Do not make exact pixel matching the default for 3D gameplay. Rendering can vary slightly across hardware, drivers, antialiasing, shadows, and floating-point behavior. Use tolerances, masks, region checks, or perceptual comparison.

## Execution order

Run these prompts in order. Give an agent **one prompt at a time**. Do not paste the entire pack into MIRIAM.

```text
01  Repository audit and baseline
02  Test foundation
03  Validation scenario framework
04  Screenshot evidence
05  Deterministic scenario/input/state helpers
06  Visual regression
07  Video capture
08  Performance validation
09  First real Mandate scenarios
10  Agent completion contract
11  Codex independent review gate
```

Prompts 01–10 may be implemented by MIRIAM or Codex. Prompt 11 should preferably be run by **Codex as an independent reviewer** after MIRIAM performs implementation work.

After the framework exists, use `12_REUSABLE_FEATURE_VALIDATION_PROMPT.md` whenever a new gameplay feature is assigned.

## Non-negotiable rule

A validation failure must not be "fixed" by weakening the test unless the acceptance criterion itself is demonstrably wrong.

The implementation should be changed first.

If an agent believes the validation is incorrect, it must explain:

- which criterion is wrong;
- why it is wrong;
- what the corrected criterion should be;
- what evidence supports changing it.

Do not silently delete assertions, enlarge tolerances, skip tests, or replace deterministic checks with comments.

## Current tooling assumptions

The pack assumes Godot 4.x and is designed to use GdUnit4 where practical. Before writing code, agents must inspect the actual repository and current dependency versions rather than assuming paths, language, Godot minor version, or addon installation state.

Useful current capabilities include:

- GdUnit4 CLI test execution.
- GdUnit4 SceneRunner scene access and simulated input/actions.
- Godot `Viewport.get_texture().get_image()` screenshot capture after rendering completes.
- Godot command-line `--write-movie`, `--fixed-fps`, and `--quit-after`.

Do not hard-code commands until the repository's actual Godot executable conventions and scripts have been discovered.
