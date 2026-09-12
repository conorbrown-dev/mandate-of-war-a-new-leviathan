# Codex Gameplay Validation Review

**Date:** 2026-09-12
**Scope:** validation prompt-pack implementation and the production fixes it exposed

## Contract and implementation boundary

Required documentation exists: `VALIDATION_BASELINE.md`, `RUNNING_TESTS.md`, `SCENARIO_CATALOG.md`, `AGENT_WORKFLOW.md`, and the gameplay contract in `AGENTS.md`.

The gameplay scenarios instantiate `res://main.tscn`, start the production Broken Strait scene, send Godot mouse-wheel/button events, call the real `RtsExtension`, and advance the native fixed-tick simulation. They do not define substitute units, movement, zoom, production, or logistics systems. The airfield scenario invokes the existing structure/unit queue and production presentation helpers; native installation and aircraft state determine its acceptance.

All loops have explicit tick/frame bounds. There are no sleeps, uncontrolled waits, hard-coded developer-machine paths, automatic baseline replacement paths, or validation-only branches in production gameplay. Runtime artifact paths are supplied by the wrapper and report artifact links are relative to their run directory.

## Failure and report integrity

The wrapper rejects missing/malformed reports, mismatched IDs, empty assertion lists, Godot `SCRIPT ERROR` output, failed assertions, missing requested screenshots/video, malformed visual manifests, and visual mismatches. It rewrites status to `FAIL` before returning non-zero when an engine error would otherwise exit zero.

`tools/validate all --force-failure` returned 1. All four reports recorded `FAIL` with understandable forced-failure reasons:

- `validation/artifacts/airfield_fighter_ferry/20260912T132709.397745Z/report.json`
- `validation/artifacts/basic_selection_move/20260912T132710.216115Z/report.json`
- `validation/artifacts/simulation_scale_1000/20260912T132710.939380Z/report.json`
- `validation/artifacts/strategic_zoom_transition/20260912T132713.074009Z/report.json`

The restored `tools/validate all` returned 0, and the final `tools/test` returned 0.

## Artifact inspection and determinism

Strategic zoom was run twice with seed 640640 on the same rendered environment. Both tactical and strategic checkpoint comparisons scored 1.000000. Accepted 1280x720 references use a calibrated 0.995 threshold from `validation/baselines/manifest.json`. An intentionally negated reference scored 0.445200, returned 1, and produced `/tmp/mandate-corrupted-reference-diff.png`; the unchanged accepted baseline subsequently passed.

The inspected strategic screenshot visibly shows the selected engineer's fixed-pixel strategic marker after the tactical model transition. The inspected air screenshots show the selected completed airfield label and the fighter ingress marker. Report paths resolve to non-empty files.

The final recorded airfield/fighter run is `validation/artifacts/airfield_fighter_ferry/20260912T130900.435651Z/report.json`. Its AVI is non-empty and records 149 frames at 1280x720, 30 FPS. Functional airfield/fighter state assertions passed independently of the recording.

The 1,000-unit benchmark uses the existing native scale runner three times, separates 100 warm-up ticks from 100 measured ticks, reports cold flow-field work separately, and explicitly warns that its results are not rendering FPS.

## Findings

### LOW — Visual baseline is currently one renderer/host profile

- Evidence/path: `validation/baselines/manifest.json`; rendered reports identify NVIDIA OpenGL Compatibility.
- Why it matters: different GPU drivers, operating systems, and font rasterizers may reduce similarity despite correct gameplay.
- Smallest correction: collect supported-platform repeat captures before making visual comparison mandatory in cross-platform CI; split manifests by renderer only if observed variance requires it.

### LOW — No hosted CI invokes the repository command yet

- Evidence/path: repository has no committed hosted CI workflow; `docs/validation/VALIDATION_BASELINE.md` records this.
- Why it matters: the contract is enforceable locally but is not yet an automated merge gate.
- Smallest correction: when CI is selected for the project, run `tools/test` headlessly and publish `validation/artifacts/` on failure; keep rendered evidence on a display-capable worker.

### LOW — Scenario seed is reproducibility metadata, not a map-variant control

- Evidence/path: `godot/project/validation/validation_runner.gd` seeds global randomness, while terrain/civilian dressing currently use authored local seeds in production content.
- Why it matters: changing `--seed` does not create a different Broken Strait terrain layout.
- Smallest correction: only plumb a scenario seed into production map generation when randomized map variants become a real gameplay feature; do not add test-only generation.

## Verification summary

- Release build: PASS.
- CTest: 3/3 PASS.
- Direct native behavior runner: 164/164 PASS.
- Godot skirmish presentation: 91 checks, 0 failures.
- Validation registry: four scenarios PASS after intentional failure restoration.
- Rendered screenshots, tolerance comparison, failure diff, and fixed-step AVI: PASS.

REVIEW DECISION: ACCEPT
