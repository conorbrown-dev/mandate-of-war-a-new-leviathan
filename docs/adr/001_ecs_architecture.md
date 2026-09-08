# ADR-001: Entity-Component-System Architecture

**Date:** 2026-08-24  
**Status:** Accepted

## Decision

Implement ECS for simulation layer to enable massive unit counts with cache-friendly data access.

## Rationale

Traditional object-oriented designs with per-unit scripts/objects have O(n) overhead per unit for:
- Virtual function calls
- Memory indirection (heap allocations)
- Cache misses (pointer chasing)

ECS provides:
- Contiguous memory layout (cache lines)
- Batch operations on component arrays
- Data-oriented optimization opportunities

## Implementation

### Core Types

```cpp
// Component: POD data only
struct Position {
    float x, y, z;
};

struct Velocity {
    float x, y, z;
};

struct Health {
    float current;
    float max;
};

// Entity: Simple ID
using EntityId = uint32_t;

// Archetype: Component combination + entity IDs
struct Archetype {
    std::vector<EntityId> entities;
    ComponentMask mask;
};
```

### Queries

```cpp
// Range-based iteration over positions
for (auto [entity, pos] : world.query<Position>()) {
    pos.x += velocity[entity].x;
}

// Spatial query (quadtree-backed)
auto nearby = world.query_in_region<Position, Velocity>(center, radius);
```

### Memory Layout

```
Archetype 0: (Position, Velocity, Health)
 entities: [0, 1, 2, 3, ...]
  positions: [pos0, pos1, pos2, pos3, ...]  // contiguous
  velocities: [vel0, vel1, vel2, vel3, ...]  // contiguous
  health: [hp0, hp1, hp2, hp3, ...]  // contiguous

Archetype 1: (Position, Health)
  entities: [4, 5, 6, ...]
  positions: [pos4, pos5, pos6, ...]
  health: [hp4, hp5, hp6, ...]
```

## Benefits

1. **Cache efficiency**: Iterate consecutive memory for component updates
2. **Vectorization**: SIMD-friendly for batch operations
3. **Flexibility**: Dynamic component composition
4. **Serialization**: Easy to save/load entire component arrays

## Tradeoffs

1. **Complexity**: ECS framework vs simple classes
2. **Query overhead**: Runtime bitmask checks
3. **Moving between archetypes**: Expensive when components change

## Migration Path

- Phase 1: Simple array of structs (no full ECS)
- Phase 2: Add archetype splitting for hot paths
- Phase 3: Parallel queries with job system

## Validation

Benchmarks will compare:
- ECS query performance vs OOP iterating std::vector<GameObject*>
- Memory usage per unit
- Frame time at scale

## References

- [ECS Wikipedia](https://en.wikipedia.org/wiki/Entity_component_system)
- [Data-Oriented Design](https://dataorienteddesign.com/dodbook/)
