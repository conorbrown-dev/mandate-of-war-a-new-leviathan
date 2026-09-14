# Godot 4.x Build and Validation

> **Goal 0A update:** Prefer `python3 scripts/dev.py build` and `python3 scripts/dev.py smoke`; set `GODOT_BIN` when Godot 4.7.2 is not on `PATH`. The commands below are historical examples.

## Build

The checked-in executable reports Godot `4.7.2.stable.official`.

```bash
cd /path/to/mandate-of-war
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The build copies the current Linux libraries to:

- `godot/project/bin/librts_gdextension.so`
- `godot/project/bin/librts_simulation.so`

## Automated Validation

```bash
./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --editor --path godot/project --quit-after 8

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test.gd

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --quit-after 30
```

Expected results:

- The editor scans `rts.gdextension` and loads `main.tscn` without resource or GDScript failures.
- The smoke test prints `RtsExtension smoke test passed`.
- The runtime prints that 1,000 batched units were spawned.

## Graphical Validation

```bash
./Godot_v4.7.2-stable_linux.x86_64 --editor --path godot/project
```

Run the main scene and verify:

- 1,000 blue units appear on the terrain.
- Left click or drag selects units and turns them amber.
- Right click sends selected units toward a formation around the target.
- Mouse wheel zooms; arrow keys/WASD or middle drag pan.
- The overlay reports native-extension status, unit count, selection count, and FPS.

## Troubleshooting

- Confirm `godot/project/rts.gdextension` exists and references `res://bin/librts_gdextension.so`.
- Run `ldd godot/project/bin/librts_gdextension.so` and confirm `librts_simulation.so` resolves from the same directory.
- Rebuild after every C++ change; do not copy an older root-level `.so` over the files in `bin/`.
- Treat editor debugger TCP-listen failures in a restricted headless sandbox separately from extension-loading failures.
