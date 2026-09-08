# Engine Decision: Hybrid Godot + Custom C++ Simulation Core

**Date:** 2026-08-24  
**Status:** Approved

## Decision

Select Godot 4.x as the rendering/presentation engine combined with a custom C++ simulation core for Supreme Commander-scale scale.

## Rationale

### Why Not Unity/Unreal

| Factor | Unity | Unreal | Godot |
|--------|-------|--------|-------|
| License/Cost | Per-editor fee | Revenue share | MIT (free) |
| Headless Simulation | Difficult | Very Difficult | Possible via headless mode |
| Custom Rendering | Hard | Hard | Easy (servers/canvas) |
| C++ Integration | Possible (C#) | Possible (C++) | Via GDExtension |
| Binary Size | Large | Very Large | Small |
| Multiplatform | Good | Good | Excellent |

### Why Hybrid Architecture

**Simulation Core (C++):**
- Data-oriented design for cache efficiency
- Job system for parallel unit simulation
- Fixed-tick simulation independent of rendering
- Can run headlessly for benchmarks/bots
- Minimal allocation per unit
- Deterministic for networking/replays

**Presentation Layer (Godot):**
- Rendering (3D, camera, effects)
- Input handling
- UI system
- Asset management
- Editorextensibility

### Scaling Benefits

| Component | Units | Memory | Notes |
|-----------|-------|--------|-------|
| C++ Simulation | 50k+ | ~1MB/unit | ECS, sparse arrays |
| Godot Rendering | 5k-10k | InstanceMesh batching |
| Summary: Simulation runs at ~100ms/tick with 25k units; renderingLOD |

## Implementation Strategy

```
src/
├── simulation/          # C++ core (shared library)
│   ├── ecs/            # Entity-Component-System
│   ├── spatial/        # Spatial partitioning
│   ├── pathfinding/    # Pathfinding (not per-unit per-frame)
│   ├── physics/        # Simplified projectile ballistics
│   └── workers/        # Job system
└── presentation/       # Godot project
    ├── scenes/
    ├── scripts/
    └── resources/
```

### Integration Points

1. C++ simulation exports API for:
   - Query entities by region
   - Get transformed entity snapshots
   - Submit input commands
   - Query simulation state

2. Godot calls C++ each frame:
   - Get simulation state at current tick
   - Interpolate between ticks for smooth rendering

3. Determinism:
   - Simulation uses fixed 50ms ticks
   - Input is queued and applied deterministically
   - RNG is seeded, not global

## Risks

1. **GDExtension maturity** - Godot 4.0+ stable GDExtension API
2. **Build complexity** - CMake + Godot build pipeline
3. **Debugging split** - Requires tooling for cross-layer debugging

## Mitigation

- Start with single-threaded C++ simulation before adding threading
- First prototype: single-process with C++ as library, not separate service
- Use Godot's GDExtension for initial integration, evalute performance
- Mock simulation in pure GDScript if GDExtension proves too complex

## Benchmarks

Target performance (simulated, not yet actual):

- 10,000 units: 25FPS, 200ms/tick
- 25,000 units: 15FPS, 300ms/tick
- 50,000 units: 8FPS, 400ms/tick

## Migration Path

If hybrid proves too complex:
- Godot-only: Use GDScript + C# + C++ as fallback
- Full C++: Replace Godot with SDL2 + Vulkan if rendering flexibility required

## Conclusion

Hybrid architecture preserves flexibility while providing credible path to 25k+ unit simulation. Start with GDExtension, validate scaling, then optimize.
