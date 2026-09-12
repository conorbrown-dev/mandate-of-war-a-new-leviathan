# Running Tests and Gameplay Validation

Set `GODOT_BIN` when the repository-local Godot binary is unavailable. The Python entry points work on Linux, macOS, and Windows; thin shell and PowerShell wrappers are provided.

## All normal tests

```bash
tools/test
```

This builds the current CMake tree, runs CTest with failure output, runs the production Godot skirmish presentation harness, and executes every registered headless gameplay/benchmark validation scenario.

PowerShell:

```powershell
tools/test.ps1
```

## One area

```bash
tools/test native
tools/test godot
tools/test validation
tools/test basic_selection_move
```

The last form delegates to the named validation scenario. All commands return non-zero on build, assertion, script, schema, or process failure.

## Scenario discovery and narrow runs

```bash
tools/validate --list
tools/validate all
tools/validate basic_selection_move
tools/validate strategic_zoom_transition --seed 640640
tools/validate airfield_fighter_ferry
tools/validate simulation_scale_1000
```

PowerShell uses `tools/validate.ps1` with the same arguments. Every run prints its `report.json` path under `validation/artifacts/`.

## Rendered evidence

State checks default to headless execution. On a machine with a working graphical session:

```bash
tools/validate strategic_zoom_transition --rendered --require-screenshots
tools/validate strategic_zoom_transition --rendered --update-baseline --visual-threshold 0.995
tools/validate airfield_fighter_ferry --record
```

`--update-baseline` is the only path that changes accepted references and fails when no screenshots were produced. `--record` uses Godot Movie Maker at 1280x720 and 30 fixed FPS, verifies a non-empty video, probes its metadata, and records it in the report.

## Logs and debug output

Each scenario directory contains `engine.log` (captured console) and `engine-godot.log` (Godot log). Read the failed assertions and errors in `report.json` before opening the larger logs.

There are no arbitrary sleeps or manual editor steps. Scenario waits are bounded fixed-tick loops; screenshot capture waits on Godot's render completion signal only in rendered mode.
