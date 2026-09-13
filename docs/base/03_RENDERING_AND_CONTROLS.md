# Milestone 03 — Rendering and Controls

## Current Status (2026-09-01)

**Verified complete as a supporting work package for Goal 02 in the current dirty tree.** This document does not replace the canonical milestone sequence.

Confirmed:

- Godot 4.7.2 loads `rts.gdextension` in editor and runtime modes.
- `RtsExtension` supports simulation lifecycle, unit creation/query, and move orders.
- `main.gd` starts 1,000 entities and displays them through `MultiMeshInstance3D`.
- The native smoke test and graphical project startup pass.

Completed evidence:

- CTest-backed correctness coverage and an assertion-backed isolated movement harness;
- Conor's graphical checklist in `docs/PLAYER_TESTING.md`;
- real-window 1,000/5,000/10,000-unit profiles in `docs/PERFORMANCE.md`;
- batched position fetch and formation-order calls across the native boundary;
- documentation separating the dummy C++ renderer from Godot's actual MultiMesh rendering;
- the 2026-09-01 Goal 02 Codex review and warranted combat-isolation fix.

For the current Goal 03 starting task and order, follow `docs/OPENCODE_HANDOFF.md` and `docs/NEXT_TASKS.md`.

## Objective

Implement the rendering and input systems that allow players to view the simulation and issue commands. This milestone proves the simulation-to-rendering pipeline works end-to-end.

Read `00_PROJECT_CHARTER.md`, `docs/ARCHITECTURE.md`, `docs/CURRENT_STATE.md`, and `02_SIMULATION_AND_SCALE.md` first.

## Required Features

1. **Godot Editor Scene**
   - 3D scene with camera
   - Simple terrain plane
   - Unit mesh (basic sphere/cube)

2. **Rendering Pipeline**
   - C++ simulation → GDExtension → Godot nodes
   - Entity lifecycle (create/destroy) in Godot
   - Position updates from simulation

3. **Camera System**
   - Zoom range: 100m to 10km
   - Smooth zoom transitions
   - Follow selected units (optional)

4. **Input System**
   - Mouse selection (box select)
   - Right-click move orders
   - Unit highlighting

5. **Debug Overlay**
   - FPS counter
   - Tick time (ms/tick)
   - Unit count

## Implementation Strategy

### Phase 1: Basic Scene
1. Create Godot 3D scene
2. Add terrain (plane mesh)
3. Create unit mesh (basic sphere)
4. Test scene loads in editor

### Phase 2: GDExtension Integration
1. Implement `RtsExtension` class with:
   - `create_unit(x, y)` → returns entity ID
   - `update_positions()` → syncs simulation to Godot nodes
   - `get_entity_count()` → for debug display
2. Create unit nodes in Godot when `create_unit` called
3. Update positions every frame from simulation

### Phase 3: Camera
1. Camera with zoom control (mouse wheel or buttons)
2. Smooth interpolation for zoom
3. Follow mode (optional)

### Phase 4: Input
1. Raycast from camera to terrain
2. Box selection (drag mouse)
3. Right-click move command
4. Highlight selected units

### Phase 5: Debug Overlay
1. HUD with FPS/tick time/unit count
2. Optional: toggle debug info

## Constraints

- Keep the canonical `godot-cpp` GDExtension API narrow and batched
- Use fixed simulation tick (50ms)
- Avoid Godot scene node per unit if performance requires batching
- No factions, economy, combat, or advanced AI yet

## Benchmarks

The recorded real-window moving profile tested 1,000, 5,000, and 10,000 simple visible units and held near the 60 Hz display ceiling on the documented host. Treat those as scoped measurements, not portable FPS guarantees.

If benchmarks fail, re-evaluate architecture (batching, instanced meshes, etc.)

## Required Review

When rendering + controls work with 1,000 units, invokes Codex for review.

Ask Codex to challenge:
- Is the rendering pipeline efficient?
- Are unit transforms being updated per-frame?
- Is there unnecessary per-unit allocation?
- Can this scale to 10,000+ units?

## Completion Criteria

- Godot scene loads and renders units
- Units can be selected with box select
- Move orders update simulation
- Debug overlay shows FPS/tick time/unit count
- 1,000 units render at acceptable FPS
- `docs/CURRENT_STATE.md` and `docs/NEXT_TASKS.md` updated

Stop after this milestone.
