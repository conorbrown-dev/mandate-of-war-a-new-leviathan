# GDExtension Loading Repair

## Root Cause

The failed integration combined several independent configuration and code errors:

1. The valid `.gdextension` descriptor had been deleted, so Godot had no resource to discover.
2. `project.godot` contained a custom `[gdextension]` section, which is not the extension descriptor format.
3. `main.gd` referenced `DynamicLibrary`, which is not a built-in GDScript class.
4. The extension used `dlopen("librts_simulation.so")`, making resolution depend on the process working directory.
5. The main scene and script disagreed on node names and used 3D projection methods in the wrong direction.

The previous conclusion that Godot 4.x could not load GDExtensions was therefore invalid.

## Repair

- Added `godot/project/rts.gdextension` with the correct entry symbol and platform library mappings.
- Linked the extension directly to the simulation library.
- Added `$ORIGIN` to the extension runtime search path.
- Made CMake copy both native libraries into `godot/project/bin/`.
- Registered `RtsExtension` as a `RefCounted` Godot class.
- Replaced GDScript library loading with `ClassDB.instantiate("RtsExtension")`.
- Rebuilt the project scene and scripts with valid nodes, resources, selection projection, camera controls, and batched units.

## Evidence

After a Release build:

- Godot's generated `.godot/extension_list.cfg` contains `res://rts.gdextension`.
- The extension entry point succeeds during headless editor startup.
- `ldd godot/project/bin/librts_gdextension.so` resolves `librts_simulation.so` from the same directory.
- `godot --headless --path godot/project --script res://test.gd` prints `RtsExtension smoke test passed`.
- A 30-frame headless project run starts the scene and spawns 1,000 batched units without script/runtime errors.

Sandboxed headless editor runs can emit TCP-listen errors when the editor debugger cannot bind a local port. Those errors are environment restrictions and are unrelated to extension loading.
