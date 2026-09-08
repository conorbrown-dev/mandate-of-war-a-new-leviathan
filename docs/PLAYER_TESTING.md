# Godot Player Validation Checklist

**Status:** Completed on 2026-08-29

**Scene:** `godot/project/main.tscn`

**Expected startup count:** 1,000 units

This checklist is the human graphical validation required for the active Goal 02/rendering-controls closeout. Automated profiling below is evidence for startup and performance only; it does not check any human-interaction box. Record the date, Godot version, GPU, pass/fail result, and notes when running the interactive portion.

## Preparation

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test.gd

./Godot_v4.7.2-stable_linux.x86_64 --editor --path godot/project
```

Before testing, confirm the smoke script prints `RtsExtension smoke test passed`. In the editor, run the main project and keep the Output and Debugger panels visible.

## Startup and Rendering

- [x] The project starts without GDExtension, resource, GDScript, or native-library errors.
- [x] Terrain, lighting, camera view, HUD, and units are visible.
- [x] The HUD reports `GDExtension: loaded` and 1,000 units.
- [x] Units are rendered through `MultiMeshInstance3D`; the scene tree does not contain one node per unit.
- [x] Unit transforms remain visually stable when no move order is active.

## Selection

- [x] Clicking a visible unit selects it and changes its instance color to amber.
- [x] Clicking empty ground clears the selection when Shift is not held.
- [x] Dragging from upper-left to lower-right selects only units inside the rectangle.
- [x] Dragging in the reverse direction produces the same result.
- [x] Holding Shift adds units to the current selection without clearing it.
- [x] A small click is not accidentally treated as a large box selection.
- [x] The HUD selection count matches the visible highlighted units.

## Move Orders

- [x] Right-click with no selected units is a safe no-op.
- [x] Right-clicking terrain with one selected unit moves it toward the clicked world position.
- [x] A group move order produces a formation rather than sending every unit to exactly one point.
- [x] Units arrive without overshoot, oscillation, teleporting, or persistent drift.
- [x] Reissuing a move order changes the active destination cleanly.
- [x] Selection remains associated with the correct entity IDs while units move.

## Camera and HUD

- [x] Mouse-wheel zoom is smooth and respects minimum/maximum distance.
- [x] Arrow keys and WASD pan in the expected map directions.
- [x] Middle-mouse drag pans smoothly without also drawing a selection rectangle.
- [x] The camera cannot enter an unusable orientation or clip below the terrain during normal input.
- [x] FPS, unit count, and selection count continue updating during movement.

## Stability

- [x] Run for at least five minutes with repeated selection, pan, zoom, and group move orders.
- [x] No GDScript errors, native crashes, invalid calls, or growing error spam appear after the `unit_label.gd` compatibility fix described below.
- [x] Closing the running project stops the simulation cleanly.
- [x] Rerunning from the editor starts with the expected unit count and no retained world state from the previous run.

## Performance Capture

This checklist alone is not the scale benchmark. The opt-in `RTS_PROFILE_FRAMES` workload ran in real Godot windows on 2026-08-29 after a 60-frame warm-up and issued one moving batch formation. Full methodology and limits are in `docs/PERFORMANCE.md`.

| Visible units | FPS avg | Godot process avg/max | Native update avg/max | Draw calls avg | Static/video memory | Result/notes |
|---------------|---------|-----------------------|-----------------------|----------------|---------------------|--------------|
| 1,000 | 58.72 | 18.767 / 22.446 ms | 0.154 / 0.640 ms | 8.79 | 36.30 / 13.57 MiB | Automated 300-frame moving run passed |
| 5,000 | 58.90 | 19.812 / 22.409 ms | 0.781 / 3.052 ms | 10.00 | 37.70 / 13.81 MiB | Automated 300-frame moving run passed |
| 10,000 | 59.62 | 19.112 / 24.087 ms | 1.645 / 6.625 ms | 10.41 | 39.43 / 14.12 MiB | Automated 300-frame moving run passed |

The native-update average includes frames without a 50 ms simulation tick. The automated runs did not simulate clicks, drag selection, camera controls, editor reruns, or five minutes of use.

## Test Record

| Field | Value |
|-------|-------|
| Date | 2026-08-29 |
| Tester | Conor (human controls/stability); Codex instrumentation |
| Godot version | 4.7.2 stable (`ed1daf0bf`) |
| Build type | Release |
| GPU/driver | NVIDIA GeForce RTX 4070 Ti SUPER / 595.84 |
| Functional result | Pass |
| Editor/runtime errors | Editor initially found `unit_label.gd` redeclaring native `Label3D.offset`; renamed the script field to `world_offset`, then editor scan and runtime smoke passed |
| Follow-up issues | Human Goal 02 controls gate complete; preserve this evidence while Goal 03 is validated separately |
