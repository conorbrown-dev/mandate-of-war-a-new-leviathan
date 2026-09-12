# Gameplay Validation Baseline

## Existing project facts

- Engine/presentation: Godot 4.7.2, `gl_compatibility`, GDScript scenes, and the C++20 `RtsExtension` GDExtension declared by `godot/project/rts.gdextension`.
- Build/test: CMake builds `rts_tests`, `rts_integration_tests`, `test_portable_snapshot`, and four benchmark executables. CTest is the native aggregate entry point. Godot scene harnesses live under `godot/project/test*.gd`; `test_skirmish.gd` exercises the real `main.tscn` path.
- GdUnit4: not installed. The current assertion-backed native runners and Godot scene harness already cover the needed boundaries, so this work reuses them rather than adding another framework.
- CI: no hosted CI workflow is currently committed. Repository-owned local commands are therefore the canonical entry point.
- Existing developer tooling: `scripts/` contains asset generation, import auditing, demo launch, and continuation-guard checks. `tools/` now owns the cross-platform test and validation entry points.
- Simulation control: `Simulation::update(float delta_ms)` and the GDExtension `update_simulation` binding advance fixed simulation time independently of rendering. ADR 003 documents the fixed-tick decision.
- Determinism/state: native entity IDs, content/unit type IDs, stable scenario data, `DeterministicRNG`, portable snapshots, state hashes, command logs, and replay readers/writers already exist. The validation runner records an explicit seed and advances bounded fixed ticks.
- Metrics: `benchmark/scale_benchmark.cpp` reports warm-up/measurement counts, command enqueue, cold field generation, average/p50/p95/max simulation time, snapshot time, memory, moved units, and state hashes.
- Bootstrap: `godot/project/main.tscn` is instantiated and `_on_start_skirmish_pressed()` starts the production Broken Strait skirmish. `SkirmishConfig.load_definition()` loads scenario JSON and heightmap data.
- Units/orders: `main.gd` creates the native `RtsExtension`, starts the skirmish, registers native entity IDs, and routes mouse input through `_unhandled_input()` to authoritative command bindings such as `issue_move_commands` and `issue_build_commands`.
- Rendering evidence: Godot 4.7.2 supports `--write-movie`, `--fixed-fps`, and viewport image capture. A render-capable display is required here; the headless display driver does not produce usable viewport images.
- Headless operation: native tests and state/scene validation run without manually opening the editor. Rendered screenshots and movies are a separate evidence tier.

## Necessary gaps

Before this work there was no scenario registry, stable artifact layout/report schema, one-command scenario runner, screenshot checkpoint contract, visual comparison tool, or required gameplay completion contract for agents. Godot script errors could also be present in console output without necessarily making the process exit non-zero.

## Minimal architecture

```text
tools/validate <scenario>
  -> validation_runner.gd
  -> production main.tscn + RtsExtension
  -> fixed-tick actions and state assertions
  -> optional rendered checkpoints/movie
  -> validation/artifacts/<scenario>/<run-id>/report.json
```

`tools/validate.py` owns scenario discovery, process/error propagation, report-schema checks, artifact orchestration, benchmark adaptation, and optional visual comparison. `validation_runner.gd` only orchestrates the real game and records checks; it does not reproduce gameplay systems.

## File layout

```text
tools/validate[.ps1]          cross-platform command wrappers
tools/validate.py             registry and artifact orchestration
tools/visual_compare.py       normalized RGB comparison and diff output
godot/project/validation/     production-scene scenario runner
docs/validation/              contracts and operating documentation
validation/baselines/         explicitly accepted visual references
validation/artifacts/         generated, gitignored run evidence
```

## Determinism risks

- Terrain and civilian dressing use local authored seeds; adding unseeded procedural systems would undermine repeatability.
- The Godot scene still has render-frame-driven presentation updates. Functional scenarios therefore drive native fixed ticks directly and use frames only for input/capture presentation.
- Global native runtime state is process-owned. Each CLI scenario runs in a fresh Godot process and `main.gd` starts a fresh simulation.
- Screenshots may vary by GPU, driver, font rasterization, and renderer. Comparisons are optional, tolerance-based, and separate from state assertions.
- Wall-clock timestamps and measured performance naturally vary and are not treated as deterministic gameplay state.

## Recommended smoke scenario

`basic_selection_move` is the smallest production workflow: load `main.tscn`, start Broken Strait, find the real Field Engineer, select it with mouse events, issue a terrain-picked right-click move, advance fixed ticks, and assert movement plus destination tolerance.

## Baseline commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
tools/test godot
tools/validate basic_selection_move
```

The validation package was introduced on an intentionally dirty user worktree; unrelated existing changes are not baseline failures and must remain preserved.
