# GDExtension Implementation

The C++ simulation library is exposed to Godot via GDExtension.

## Architecture

### C++ Core (librts_simulation.so)
- ECS for entity management
- SpatialGrid for unit queries
- Simulation with fixed 50ms tick
- No Godot dependencies

### GDExtension (librts_gdextension.so)
- Thin wrapper around simulation
- Exposes Object-derived class to Godot
- Uses Godot's binding macros (GDCLASS, ClassDB)

## Build

```bash
cmake --build build
cp build/librts_simulation.so godot/lib/librts_gdextension.so
```

## GDScript Usage

```gdscript
@onready var extension = preload("res://gdextension.gdextension")

func _ready():
    extension.start_simulation()
    extension.create_unit(0, 0)
    
func _process(delta):
    extension.update_simulation(delta * 1000)
```
