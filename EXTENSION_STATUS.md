# Godot Integration Status

The Linux x86_64 GDExtension integration is working with the repository's Godot 4.7.2 binary.

## Working Path

- Descriptor: `godot/project/rts.gdextension`
- Native class: `RtsExtension`
- Extension library: `godot/project/bin/librts_gdextension.so`
- Simulation dependency: `godot/project/bin/librts_simulation.so`
- Entry symbol: `rts_extension_library_init`

Godot discovers the `.gdextension` resource during project scanning. No `project.godot` native-extension section and no GDScript `dlopen` mechanism are required.

## Verified

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --editor --path godot/project --quit-after 8

./Godot_v4.7.2-stable_linux.x86_64 \
  --headless --path godot/project --script res://test.gd
```

The editor loads `main.tscn`; the smoke test verifies native class registration, unit creation/query, and movement.

## Remaining

- Native builds for Windows and macOS
- Regeneration of `godot-cpp` bindings against the checked-in Godot 4.7.2 API

The Linux graphical editor playtest and visible 1,000/5,000/10,000-unit profiles were completed on 2026-08-29; see `docs/PLAYER_TESTING.md` and `docs/PERFORMANCE.md`.
