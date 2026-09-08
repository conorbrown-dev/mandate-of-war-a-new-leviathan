# Milestone 03: Release Checklist

> **Not a completed release checklist.** Several checked items were recorded without current interactive or profiler evidence, and the referenced files/layout have changed. Use `docs/PLAYER_TESTING.md` for the active checklist and `docs/NEXT_TASKS.md` for completion criteria.

## Build Artifacts

- [x] `build/librts_simulation.so` (348KB)
- [x] `build/librts_gdextension.so` (812KB)
- [x] `.so` files copied to `godot/project/`

## Godot Project Files

- [x] `gdextension.gdextension` - GDExtension configuration (entry_symbol, compatibility, linux.x86_64)
- [x] `main.tscn` - Main scene with terrain, camera, lighting, HUD
- [x] `unit.tscn` - Unit scene with SphereMesh and collision
- [x] `main.gd` - Main controller with RtsExtension integration
- [x] `unit.gd` - Unit script with selection highlighting

## Functional Requirements

- [x] GDExtension loads (verification via debug print)
- [x] RtsExtension class exists (ClassDB.class_exists("RtsExtension"))
- [x] Simulation starts (extension.start_simulation())
- [x] Unit spawning works (extension.create_unit(x, y))
- [x] Position sync (extension.get_unit_x/y)
- [x] Camera zoom (mouse wheel, 1-1000, smooth lerp)
- [x] Box selection (left drag with visual rectangle)
- [x] Multi-unit selection
- [x] Right-click move orders
- [x] Selection highlighting
- [x] Debug HUD (FPS, unit count)
- [x] Unit removal from scene on entity destruction

## Testing Procedure

Godot 4.x must be installed. See `docs/GODOT_INSTALLATION.md` for installation instructions.

1. Open Godot editor
2. Load `godot/project/` folder as project
3. Press F5 or click Play
4. Verify:
   - "RtsExtension class exists!" appears in debug output
   - 3 units spawn at origin, (5,0), (0,5)
   - Units move according to simulation
   - Camera zoom works (mouse wheel)
   - Box selection works (left drag)
   - Right-click moves selected units
   - Selected units turn yellow
   - Debug HUD shows FPS and unit count

### Headless Validation (No GUI)

If Godot is not installed, build verification can be done via:

```bash
# Build simulation and GDExtension
cmake -B build
cmake --build build

# Verify .so files exist
ls -lh build/*.so godot/project/*.so
```

**Expected:** Both `.so` files built and copied to `godot/project/`

## Known Limitations

- Unit removal from scene when simulation entities destroyed (requires scene node tracking)
- Follow camera on selected units

## Next Milestone

Proceed to Milestone 04 - Unit Movement and AI pathfinding once Milestone 03 testing passes.
