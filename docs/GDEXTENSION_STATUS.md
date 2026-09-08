# GDExtension Integration Status

## Status

✅ Loaded and smoke-tested with Godot 4.7.2 on Linux x86_64.

## Configuration

`godot/project/rts.gdextension` declares `rts_extension_library_init` and points to the matching extension and simulation libraries under `godot/project/bin/`. CMake copies both outputs there after building `rts_gdextension` and embeds an `$ORIGIN` runtime search path.

GDScript obtains the native object with:

```gdscript
var extension: Object = ClassDB.instantiate("RtsExtension")
extension.call("start_simulation")
```

## Validation

- Headless editor scan loads `main.tscn` without resource or script errors.
- Headless project startup creates 1,000 simulated units and a `MultiMeshInstance3D` view.
- `res://test.gd` verifies class registration and a simulation movement round trip.

Graphical interaction and rendering performance still require an editor playtest and profiler capture.
