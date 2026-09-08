# Near-Future RTS - Godot GDExtension

This is a binding for the C++ simulation library to Godot.

## Build

```bash
cmake --build build
cp build/librts_simulation.so godot/lib/librts_gdextension.so
```

## GDScript Usage

```gdscript
extends Node3D

@onready var extension = preload("res://gdextension.gdextension")

func _ready():
    extension.start_simulation()
    extension.create_unit(0, 0)
    
func _process(delta):
    extension.update_simulation(delta * 1000)
    extension.move_unit(0, $Camera3D.global_position.x, $Camera3D.global_position.z)
```
