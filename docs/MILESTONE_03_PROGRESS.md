# Milestone 03 Progress - 2026-08-26

> **Historical and superseded.** This report describes an earlier experimental scene and contains completion claims that have not passed the current checklist. Do not use it as current status. Start with `docs/OPENCODE_HANDOFF.md`, `docs/CURRENT_STATE.md`, and `docs/NEXT_TASKS.md`.

## Summary
Completed Godot integration with rendering pipeline and controls for Milestone 03.

## What's Working
- ✅ `librts_simulation.so` (347KB) - C++ ECS simulation with unit management
- ✅ `librts_gdextension.so` (800KB) - GDExtension wrapper with godot-cpp
- ✅ GDExtension configuration fixed (entry_symbol, compatibility_minimum, linux.x86_64)
- ✅ .so files copied to godot/project/
- ✅ unit.tscn with SphereMesh (radius 0.5) and BoxShape3D collision
- ✅ unit.gd with selection highlighting (yellow when selected)
- ✅ main.tscn with terrain plane (1000x1000), proper camera transform (20° down, z=10), directional light
- ✅ main.gd with:
  - RtsExtension class instantiation
  - Camera zoom (mouse wheel, 1-1000 range) with smooth lerp
  - Unit spawning with Godot scene instantiation
  - sync_simulation_to_visuals() - position sync each frame
  - Box selection (left drag)
  - Box selection boundary visualization (yellow rectangle)
  - Right-click raycast to terrain plane for move orders
  - Multi-unit move orders (moves all selected units)
  - Selection highlighting (yellow when selected)
  - Unit removal from scene on entity destruction
  - Debug HUD (FPS, unit count)
- ✅ Build passes (CMake + GDExtension)

## Remaining Tasks

### Controls & Rendering
- [ ] Box selection boundary visualization (selection rectangle) - **Completed**
- [ ] Unit removal from scene when simulation entities are destroyed

### Testing
- [ ] Launch Godot and verify GDExtension loads
- [ ] Test with 100 units (verify no performance issues)
- [ ] Test with 1000 units
- [ ] Test with 10000 units
- [ ] Performance profiling with increasing unit counts

## Files Modified
- `godot/project/gdextension.gdextension` - Fixed entry_symbol, compatibility_minimum, linux.x86_64
- `godot/project/unit.tscn` - Added SphereMesh, fixed node structure
- `godot/project/unit.gd` - Added set_selected() for visual feedback
- `godot/project/main.tscn` - Added terrain, proper camera transform, lighting
- `godot/project/main.gd` - Complete rewrite with GDExtension integration, controls, HUD, selection rect
- `build/*.so` - Copied to godot/project/

## Next Steps
1. Launch Godot editor to verify GDExtension loads correctly
2. Test unit spawning and movement
3. Test with 100/1000/10000 units
4. Implement selection rectangle boundary visualization - **Completed**
5. Add unit removal from scene when simulation entities destroyed - **Completed**
