# Architecture

**Status:** Directional architecture plus verified current-state notes
**Last updated:** 2026-09-01

> The diagrams and later modules in this document include target architecture. The current verified implementation is single-threaded, uses per-type sparse `unordered_map` component stores, one spatial hash grid, and synchronous shared flow fields, and has no socket transport or worker job system. See `docs/CURRENT_STATE.md` for the evidence-backed boundary.

## Overview

Hybrid architecture with simulation (C++) and presentation (Godot) separated by clean API boundary.

## High-Level Structure

```
+-------------------+     +-------------------+
|   Godot Layer     |     |   C++ Simulation  |
|   (Presentation)  |<--->|   (Core Logic)    |
|                   | API |                   |
| - Rendering       |     | - ECS             |
| - Camera/Zoom     |     | - Spatial Grid    |
| - Input Queue     |     | - Pathfinding     |
| - UI              |     | - Combat          |
| - Asset Mgmt      |     | - Resource Mgmt   |
+-------------------+     +-------------------+
```

## Simulation Layer (C++)

### Key Principles

1. **Data-oriented direction**: Current sparse component stores are ownership-correct but not yet contiguous archetype/SoA storage
2. **Fixed tick rate**: 50ms (20 ticks/sec) independent of rendering; caller deltas are finite/positive and retain at most five catch-up ticks
3. **Allocation control**: Measure and remove avoidable per-tick allocations before claiming zero-allocation behavior
4. **Cache-friendly access patterns**: Spatial locality
5. **Deterministic target**: Fixed tick and explicit state are present; repeat-run state-hash proof remains required

### Modules

| Module | Responsibility | Constraints |
|--------|---------------|-------------|
| `ecs/` | Entity-Component-System | Components are POD types |
| `spatial/` | Current single-level spatial hash grid | Average hash lookup; hierarchy is future work |
| `pathfinding/` | Reference A*, shared formation routes, and flow fields | Compact exact-route cache capped at 128 fields |
| `combat/` | Simplified projectile and damage logic | No full rigid-body physics |
| `workers/` | Target job system | Not implemented |
| `network/` | Local command/snapshot buffers | No socket transport or proven deterministic replay |
| `content/` | Target data-driven unit definitions | Loader/schema not implemented |

### Simulation Tick Model

```cpp
struct SimulationState {
    uint32_t tick_number;
    uint64_t timestamp_ms;
    std::vector<Unit> units;
    std::vector<Projectile> projectiles;
    ResourceGrid resources;
};
```

1. **Prediction phase**: Follow move-order flow fields and update positions/spatial cells.
2. **Combat phase**: Update projectiles and damage.
3. **Environment phase**: Currently a placeholder.
4. **Logistics phase**: Update endurance and intelligence prototypes.
5. **Economy phase**: Update provisional production and spawn completed units.
6. **Network phase**: Update local command/snapshot buffers.

Total target: ~25ms/tick for 25k units

## Presentation Layer (Godot)

### Rendering Strategy

1. **Static objects**: InstanceMesh for bases, destructibles
2. **Units**: Hierarchical InstanceMesh with bone-like transforms
3. **Projectiles**: Particle system + simple meshes
4. **Ground**: Large mesh with texture atlasing
5. **Zoom**: Camera distance scaling + level-of-detail

### Camera System

```gdscript
# Pseudo-code
func _process(delta):
    var target_zoom =compute_zoom_level()
    camera.fov = lerp(camera.fov, target_zoom * BASE_FOV, ZOOM_LERP)
    camera.global_position = lerp(camera.global_position, follow_target, CAMERA_LERP)
```

### Interaction Model

```
+---------------+     +----------------+     +----------------+
| Input Handler | --> | Command Queue  | --> | C++ Simulation |
+---------------+     +----------------+     +----------------+
         |                                         |
         v                                         v
+---------------+                           +----------------+
| Camera/View   | <---  State Snapshots   <---| Renderer       |
+---------------+     (interpolated)        +----------------+
```

## Thread Model

The following is the intended thread model, not the current implementation. Simulation presently runs on the Godot/caller thread.

```
Main Thread (Godot)
├── Render loop (60-144Hz)
├── Input handling
└── State interpolation

Simulation Thread (C++)
├── Fixed-tick simulation (20Hz)
├── Job system workers (N cores)
└── Spatial partitioning updates

Network Thread (C++)
├── Packet serialization
├── Sync input from other players
└── Replay recording
```

## Data Flow

1. **Input path**: Godot submits one batched formation order containing entity IDs, center, and spacing; C++ assigns exact slots plus one shared strategic route.
2. **Render path**: Godot requests one packed position array per frame, then uploads one transform per visible MultiMesh instance.

The GDExtension boundary is an in-process presentation API, not a network trust boundary. It rejects non-finite spawns, invalid move coordinates, invalid frame deltas, oversized formation arrays, and undersized output buffers. Legacy network/replay interfaces are provisional and must not be exposed to untrusted peers or files; detailed findings are in `docs/SECURITY_REVIEW.md`.

## Content Flow

```
JSON Data Files (validated)
    ↓
Content Loader (C++)
    ↓
Component Database (ECS)
    ↓
Simulation Instance
```

## Testing Strategy

1. **Unit tests**: Custom C++ behavioral test runner registered through CTest
2. **Integration tests**: Godot test runner for rendering pipeline
3. **Benchmark suite**: Headless simulation with automated metrics
4. **Determinism tests**: Run same input twice, verify identical output

## Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| 10k visible units | 59.62 FPS in current simple moving scene on recorded host | Real-window 300-frame Godot profile |
| 25k active simulation units | 8.983 ms cache-hit average for one shared formation route | Isolated Release headless benchmark |
| 50k active simulation units | Stretch target pending evidence | Isolated Release headless benchmark |
| Tick latency | <50ms | Simulation timer |
| Memory/unit | <100KB | valgrind massif |

## Next Steps

1. Preserve the verified Goal 02 scale and presentation baseline while auditing Goal 03.
2. Replace the legacy combat diagnostic with an assertion-backed active-combat benchmark.
3. Complete and accept ADR-007 before using portable snapshot bytes as a trusted compatibility claim.
4. Schedule independent cold-route generation without breaking deterministic publication.
