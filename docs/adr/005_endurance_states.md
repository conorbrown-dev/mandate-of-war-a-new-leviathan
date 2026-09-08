# ADR-005: Endurance State Machine

**Date:** 2026-08-27  
**Status:** Accepted

## Decision

Implement aircraft and naval vessel endurance management with three-state status machine:
- **Aircraft:** `OPERATIONAL` → `LOW_FUEL` → `CRASHED`
- **Naval vessels:** `OPERATIONAL` → `LOW_FUEL` → `STRANDED`

## Rationale

### Problem: Binary Fuel Depletion

Current implementation only tracks fuel amount with direct exhaustion:

```cpp
// OLD (logistics.cpp:126-141)
void LogisticsManager::update_aircraft_endurance(EntityId aircraft_id, float delta_ms) {
    // ...
    if (aircraft_it->second.fuel <= 0) {
        aircraft_it->second.status = Aircraft::Status::CRASHED;
    }
}
```

**Issues:**
- No warning before fuel exhaustion
- Aircraft crash immediately when fuel reaches zero
- No opportunity for safe return planning
- No distinction between "low fuel" and "critical"

### Solution: Three-State Machine with Thresholds

```
Aircraft State Machine:
┌─────────────┐
│ OPERATIONAL │  fuel > (safety_margin * max_fuel)
└──────┬──────┘
       │ fuel ≤ (safety_margin * max_fuel)
       ▼
┌───────────┐
│  LOW_FUEL │  fuel > 0, safe return feasible
└──────┬────┘
       │ fuel ≤ 0
       ▼
┌─────────┐
│ CRASHED │  fuel = 0, impossible to recover
└─────────┘

Naval Vessel State Machine:
┌─────────────┐
│ OPERATIONAL │  fuel > (safety_margin * max_fuel)
└──────┬──────┘
       │ fuel ≤ (safety_margin * max_fuel)
       ▼
┌───────────┐
│  LOW_FUEL │  fuel > 0, can reach nearest carrier
└──────┬────┘
       │ fuel ≤ 0
       ▼
┌──────────┐
│ STRANDED │  fuel = 0, immobile
└──────────┘
```

## Implementation

### State Definitions

```cpp
// logistics.hpp
struct Aircraft {
    enum class Status {
        ON_GROUND,     // Aircraft not airborne
        AIRBORNE,      // In flight, OPERATIONAL state
        RECOVERING,    // Returning to base
        LOW_FUEL,      // ⭐ NEW: Fuel at safety threshold
        CRASHED        // Fuel exhausted, destroyed
    } status;
};

struct NavalVessel {
    // Status not explicitly stored, use is_stranded boolean
    // LOW_FUEL state inferred when fuel ≤ safety_margin * max_fuel
};
```

### Configuration Constants

```cpp
// logistics.hpp
namespace rts {
constexpr float FUEL_SAFETY_MARGIN = 0.25f;  // 25% fuel remaining triggers LOW_FUEL
}
```

### State Transition Logic

```cpp
// logistics.cpp
void LogisticsManager::update_aircraft_endurance(EntityId aircraft_id, float delta_ms) {
    auto aircraft_it = aircraft_.find(aircraft_id);
    if (aircraft_it == aircraft_.end()) {
        return;
    }

    float tick_consumption = aircraft_it->second.fuel_consumption_rate * (delta_ms / 1000.0f);
    aircraft_it->second.fuel -= tick_consumption;

    if (aircraft_it->second.fuel < 0.0f) {
        aircraft_it->second.fuel = 0.0f;
        aircraft_it->second.status = Aircraft::Status::CRASHED;
        return;
    }

    // ⭐ NEW: LOW_FUEL transition
    float safety_threshold = FUEL_SAFETY_MARGIN * aircraft_it->second.max_fuel;
    if (aircraft_it->second.fuel <= safety_threshold) {
        if (aircraft_it->second.status == Aircraft::Status::AIRBORNE) {
            aircraft_it->second.status = Aircraft::Status::LOW_FUEL;
        }
    }
}

void LogisticsManager::update_naval_endurance(EntityId vessel_id, float delta_ms) {
    auto vessel_it = naval_vessels_.find(vessel_id);
    if (vessel_it == naval_vessels_.end()) {
        return;
    }

    float tick_consumption = vessel_it->second.fuel_consumption_rate * (delta_ms / 1000.0f);
    vessel_it->second.fuel -= tick_consumption;

    if (vessel_it->second.fuel < 0.0f) {
        vessel_it->second.fuel = 0.0f;
        vessel_it->second.is_stranded = true;
    }

    // ⭐ NEW: LOW_FUEL inference
    float safety_threshold = FUEL_SAFETY_MARGIN * vessel_it->second.max_fuel;
    vessel_it->second.is_low_fuel = (vessel_it->second.fuel <= safety_threshold);
}
```

### Safe-Return Calculation

```cpp
// Compute safe return using flow field to nearest recovery facility
bool LogisticsManager::is_safe_return(EntityId aircraft_id, const Pathfinding& pathfinding) {
    auto aircraft_it = aircraft_.find(aircraft_id);
    if (aircraft_it == aircraft_.end()) {
        return false;
    }

    // Only calculate for AIRBORNE or LOW_FUEL aircraft
    if (aircraft_it->second.status != Aircraft::Status::AIRBORNE &&
        aircraft_it->second.status != Aircraft::Status::LOW_FUEL) {
        return false;
    }

    float current_fuel = aircraft_it->second.fuel;
    float est_cost = estimate_return_cost(aircraft_id, pathfinding);

    // Safe if fuel >= estimated cost
    return current_fuel >= est_cost;
}

// Flow field-based cost estimation
float LogisticsManager::estimate_return_cost(EntityId aircraft_id, const Pathfinding& pathfinding) {
    auto aircraft_it = aircraft_.find(aircraft_id);
    if (aircraft_it == aircraft_.end()) {
        return std::numeric_limits<float>::infinity();
    }

    // Find nearest recovery facility
    EntityId facility_id = find_nearest_recovery_facility(
        aircraft_it->second.x, 
        aircraft_it->second.y,
        RecoveryFacility::Type::AIRBASE
    );

    if (facility_id == INVALID_ENTITY) {
        return std::numeric_limits<float>::infinity();
    }

    const auto& facility = recovery_facility_.at(facility_id);
    
    // Generate flow field to facility
    auto flow_field = pathfinding.generate_flow_field(facility.x, facility.y);
    
    // Count grid cells from aircraft to facility
    int aircraft_grid_x = pathfinding.to_grid_x(aircraft_it->second.x);
    int aircraft_grid_y = pathfinding.to_grid_y(aircraft_it->second.y);
    
    int distance = 0;
    int current_x = aircraft_grid_x;
    int current_y = aircraft_grid_y;

    while (true) {
        auto it = flow_field.find({current_x, current_y});
        if (it == flow_field.end()) {
            break;  // No path
        }

        const auto& step = it->second;
        if (step.empty()) {
            break;  // Reached facility
        }

        // Move to next cell (simplified: use first step)
        current_x = pathfinding.to_grid_x(step.first);
        current_y = pathfinding.to_grid_y(step.second);
        distance++;

        if (distance > 1000) {
            break;  // Safety limit
        }
    }

    // Convert grid distance to fuel cost (1 grid cell = 1 fuel unit)
    // Add safety buffer (10%)
    return distance * 1.1f;
}
```

### Integration with logistics_phase()

```cpp
// simulation.cpp
void Simulation::logistics_phase() {
    // Update all endurance states
    logistics_manager_.update_all(tick_delta_ms_);
    
    // Identify aircraft in LOW_FUEL state for routing
    for (auto& [id, aircraft] : logistics_manager_.aircraft()) {
        if (aircraft.status == Aircraft::Status::LOW_FUEL) {
            // Set mission to REFUEL
            aircraft.mission = Aircraft::Mission::REFUEL;
            
            // Generate safe return path via flow field
            if (logistics_manager_.is_safe_return(id, *pathfinding_)) {
                aircraft.target_base = logistics_manager_.find_nearest_recovery_facility(
                    aircraft.x, aircraft.y, RecoveryFacility::Type::AIRBASE
                );
            }
        }
    }
    
    // Identify naval vessels with is_low_fuel flag
    for (auto& [id, vessel] : logistics_manager_.naval_vessels()) {
        if (vessel.is_low_fuel && !vessel.is_stranded) {
            // Route to nearest carrier
            // (implementation in logistics phase routing)
        }
    }
}
```

### Tick Update Flow

```
Per-Tick Update Sequence:

1. logistics_phase() called (between prediction and environment)
   ├─ update_aircraft_endurance() for each aircraft
   │  ├─ Consume fuel: fuel -= consumption_rate * delta_ms
   │  ├─ Check for CRASHED: fuel ≤ 0
   │  └─ ⭐ Check for LOW_FUEL: fuel ≤ safety_margin * max_fuel
   ├─ update_naval_endurance() for each vessel
   │  ├─ Consume fuel: fuel -= consumption_rate * delta_ms
   │  ├─ Check for STRANDED: fuel ≤ 0
   │  └─ ⭐ Set is_low_fuel: fuel ≤ safety_margin * max_fuel
   └─ batch_safe_return_check() for all AIRBORNE/LOW_FUEL aircraft
      └─ Use Pathfinding::generate_flow_field() to nearest facility
2. routing_phase() [future milestone]
   ├─ LOW_FUEL aircraft → REFUEL mission
   └─ LOW_FUEL naval → return to nearest carrier
```

## Validation

1. **State transition test:**
   ```cpp
   // Create aircraft with 100 fuel, 10/sec consumption
   // safety_margin = 0.25 → LOW_FUEL at 25 fuel
   // Verify status transitions: AIRBORNE → LOW_FUEL → CRASHED
   ```

2. **Safe-return integration test:**
   ```cpp
   // Place aircraft at (1000, 1000)
   // Place airbase at (0, 0)
   // Flow field calculates 500-cell path
   // Aircraft with 600 fuel → is_safe_return() = true
   // Aircraft with 400 fuel → is_safe_return() = false
   ```

3. **Performance test:**
   ```bash
   # Verify flow field cache reduces repeated calculations
   # 10k aircraft, 5% in LOW_FUEL: < 2ms additional overhead
   ```

4. **Crash prevention test:**
   ```cpp
   // 10 aircraft with 5 sec fuel remaining
   // After 5 seconds: all transition to LOW_FUEL
   # After 10 seconds: all transition to CRASHED
   ```

## Tradeoffs

### State Machine Precision (Current) vs. Simpler Fuel Thresholds (Alternative)

| Factor | Current Design | Simpler Threshold |
|--------|---------------|-------------------|
| Memory overhead | +1 byte per aircraft (enum) | 0 bytes |
| Logic complexity | Medium (3 states) | Low (2 states) |
| Player awareness | High (explicit LOW_FUEL) | Low (only CRASHED visible) |
| Routing accuracy | High (flow field to facility) | Medium (distance to nearest) |

**Decision: Three-state machine accepted because:**
- Player AI can make informed decisions with LOW_FUEL status
- Flow field integration enables precise return planning
- Different states required for visual indicators (fuel warning icons)

### Safety Margin Choice: 25%

**Rationale:**
- 25% fuel reserve allows for:
  - 25% additional travel beyond estimated return cost
  - Altitude changes and evasive maneuvers
  - Pathfinding inaccuracies and grid approximation

**Alternative values considered:**

| Margin | Pros | Cons |
|--------|------|------|
| 10% | Aggressive, More fuel used | High crash risk if pathfinding fails |
| 25% | Balanced, ✓ Current choice | Some fuel wasted on buffer |
| 50% | Very safe, Emergency reserve | Inefficient, underutilize range |

## Migration Path

**Current code (without LOW_FUEL):**
```cpp
// No explicit LOW_FUEL transition
if (fuel <= 0) {
    status = CRASHED;
}
```

**New code:**
```cpp
// LOW_FUEL transition added
if (fuel <= FUEL_SAFETY_MARGIN * max_fuel) {
    status = LOW_FUEL;  // if currently AIRBORNE
}
if (fuel <= 0) {
    status = CRASHED;
}
```

**Impact on existing systems:**
- Physics system: No change (handles position regardless of status)
- Rendering system: Add visual indicator for LOW_FUEL status
- Command system: Add "return to base" command for LOW_FUEL aircraft

## Future Enhancements

1. **Variable safety margin based on threat level:**
   ```cpp
   float safety_margin = (threat_level == HIGH) ? 0.5f : 0.25f;
   ```

2. **Fuel consumption variation:**
   - cruise: 1.0x
   - combat: 1.5x
   - emergency: 2.0x

3. **Aerial refueling integration:**
   - LOW_FUEL aircraft can request tanker support
   - Tanker routes updated to intercept refueling candidates

4. **Partial recovery:**
   - Aircraft can land at any airbase (not just nearest)
   - Re-fuel and return to mission

## References

- Milestone 04 Logistics implementation: `/src/logistics/`
- Pathfinding flow fields: `/src/pathfinding/pathfinding.cpp:generate_flow_field()`
- Tick integration: `/src/simulation/simulation.cpp:logistics_phase()`