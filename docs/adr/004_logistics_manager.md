# ADR-004: Logistics Manager Singleton Pattern

**Date:** 2026-08-27  
**Status:** Accepted

## Decision

Implement `LogisticsManager` as a singleton accessible via `get_simulation()->logistics_manager()` for C API consistency. All `extern "C"` wrappers must reference the simulation's manager instance.

## Rationale

### Problem: Duplicate Static Instances

Initial implementation used static `LogisticsManager` instance within the logistics module:

```cpp
// logistics.cpp (ORIGINAL)
static LogisticsManager g_logistics_manager;  // ❌ Problematic

extern "C" {
void rts_logistics_add_airbase(EntityId airbase_id, const Airbase* airbase) {
    g_logistics_manager.add_airbase(airbase_id, *airbase);  // Works in module
}
}
```

**Issue:** When called from GDExtension, the C++ runtime creates separate static instances:
- One in the simulation shared library
- One in the GDExtension module

Result: External wrappers reference empty `std::map` in their static instance.

### Solution: Single Source of Truth

```
Simulation (single instance)
  └── LogisticsManager logistics_manager_
          ↑
          └── Referenced by all modules
```

**Access pattern:**
```cpp
// Get simulation's LogisticsManager
auto& manager = get_simulation()->logistics_manager();

// Use via extern "C" wrapper
extern "C" {
void rts_logistics_add_airbase(EntityId airbase_id, const Airbase* airbase) {
    auto& manager = get_simulation()->logistics_manager();
    manager.add_airbase(airbase_id, *airbase);
}
}
```

### Why Not Static in Each Module?

| Approach | Pros | Cons |
|----------|------|------|
| Static in logistics module | Simple access | ❌ Multiple instances across DSOs |
| Static in simulation module | Single instance | ❌ Coupled to simulation, hard to test |
| Single instance inSimulation | ✅ One source, no duplication | Requires passing reference |

## Implementation

### Simulation Ownership

```cpp
// simulation.hpp
class Simulation {
    LogisticsManager logistics_manager_;
    
public:
    LogisticsManager& logistics_manager() { return logistics_manager_; }
};
```

### Extern "C" Wrappers

```cpp
// logistics_c_api.cpp
#include "simulation/simulation.hpp"

extern "C" {

void rts_logistics_add_airbase(EntityId airbase_id, const Airbase* airbase) {
    auto& manager = get_simulation()->logistics_manager();
    manager.add_airbase(airbase_id, *airbase);
}

void rts_logistics_update_all(float delta_ms) {
    auto& manager = get_simulation()->logistics_manager();
    manager.update_all(delta_ms);
}

}  // extern "C"
```

### GDExtension Binding

```cpp
// gd_extension.cpp
#include "simulation/simulation.hpp"
#include "logistics/logistics.hpp"

class RtsLogistics {
    Simulation& simulation_;

public:
    RtsLogistics() : simulation_(get_simulation()) {}

    void add_airbase(EntityId airbase_id, const Airbase& airbase) {
        simulation_.logistics_manager().add_airbase(airbase_id, airbase);
    }

    void update_all(float delta_ms) {
        simulation_.logistics_manager().update_all(delta_ms);
    }
};
```

## Benefits

1. **Single source of truth**:No duplicate state across DSO boundaries
2. **C API consistency**: All modules reference same instance
3. **Testability**: Can inject simulation instance in unit tests
4. **Memory efficiency**: No redundant manager instances

## Tradeoffs

### Singleton Pattern (Current)

**Pros:**
- Simple access via `get_simulation()->logistics_manager()`
- Guaranteed single instance
- Easy to integrate with existing simulation architecture

**Cons:**
- Hidden dependency (not explicit in constructor signatures)
- Harder to mock in isolation
- Potential for tight coupling

### Dependency Injection (Alternative)

```cpp
// Alternative: Pass manager to each component
class AircraftSystem {
    LogisticsManager& logistics_manager_;  // Explicit dependency
    
public:
    AircraftSystem(LogisticsManager& manager) 
        : logistics_manager_(manager) {}
};
```

**Tradeoff analysis:**

| Factor | Singleton | Dependency Injection |
|--------|-----------|---------------------|
| Test complexity | Low (global access) | Medium (mock injection) |
| Runtime overhead | None | Minimal (reference) |
| Coupling | Tight (global) | Loose (explicit) |
| Code clarity | Access anywhere | Dependencies visible |

**Decision: Singleton is acceptable because:**
- LogisticsManager is owned by Simulation (not globally free-standing)
- GDExtension modules need consistent access to single instance
- Unit tests can instantiate full Simulation for integration tests
- Performance-critical hot paths avoid virtual dispatch overhead

## Validation

1. **Integration test**: 
   - Add airbase in simulation module
   - Query from GDExtension module
   - Verify same instance returns data

2. ** Leak check**: 
   - Verify `logistics_manager_` lifetime matches `Simulation`

3. **Benchmark**: 
   - Compare access time: `manager.method()` vs static function

## Migration Path

Existing static instance code:
```cpp
// OLD
static LogisticsManager g_manager;
g_manager.add_airbase(...);

// NEW
auto& manager = get_simulation()->logistics_manager();
manager.add_airbase(...);
```

## References

- GDExtension documentation: module instance isolation
- C++ linking: static variables in shared libraries
- Simulation pattern: single owner for subsystem managers
