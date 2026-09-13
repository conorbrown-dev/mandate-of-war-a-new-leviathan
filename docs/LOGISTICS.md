# Near-Future RTS — Logistics System

> **Goal 04 is verified.** Its 14 required behaviors, focused architecture review, Release build, CTest, direct integration runner, and logistics performance gate are recorded in `docs/LOGISTICS_ARCHITECTURE_REVIEW.md`. Goal 05 is now active. Historical prototype claims below are design inventory, not acceptance evidence.

**Last updated:** 2026-09-12
**Milestone:** Goal 04 verified; canonical Goal 05 active

## Verified Goal 04 Slice

`G04-BENCH` runs an isolated Release logistics workload of 10,000 airborne conventional aircraft and 30 moving T1 carriers for 100 ticks. It executes endurance updates, safe-return estimation, facility lookup, carrier recovery-point movement, and periodic intelligence updates; prediction, combat, economy, and snapshot costs are explicitly outside this measurement. The final 2026-09-12 run passed at 0.951 ms cache-hit average (p50 0.700, p95 1.488; 15 ms acceptance limit). Carrier movement clears nearest-facility lookup entries but safe-return estimates retain bounded aircraft/facility position snapshots and expire once either endpoint moves more than 25 m. Production movement synchronizes a carrier's `RecoveryFacility` position, covered by `simulation_movement_keeps_carrier_recovery_position_in_sync`.

Focused review remediation: remembered intelligence is archived outside the live ECS component stores on entity destruction, decays deterministically, and is cleared before a recycled entity ID is created. `intelligence_memory_does_not_leak_across_entity_id_reuse` covers the lifecycle boundary.

Recovery facilities now advertise capabilities rather than being limited to one exclusive role: `AIR_RECOVERY` and `NAVAL_RESUPPLY`. Existing `Type` values remain for compatibility and indexing. A single owned installation can therefore serve both runway recovery and naval resupply; naval RETURN validation resolves the naval capability and its radius explicitly. `recovery_facility_supports_air_and_naval_capabilities` covers the combined installation contract.

`G04-AIRBASE` provides deterministic per-airbase takeoff and landing queues, runway-capacity enforcement, unusable-runway gating, explicit fixed-wing lifecycle states, recovery-radius admission, and finite-stock refuel/rearm. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-AIR_RANGE` adds fixed-tick Energy, Material, and airborne-time consumption plus conservative direct-flight return estimates. Recovery facilities are indexed by compatibility type and cached spatially with a mathematically bounded nearest-facility stability radius; aircraft estimates are invalidated after meaningful movement or any facility move. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-AIR_CRASH` excludes unusable runways from recovery compatibility and destroys exhausted active aircraft through the normal simulation entity-removal path. Queue, operation, service, spatial, ECS, and safe-return cache state are cleaned. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-VTOL` adds two JSON-defined aircraft prototypes. The Harrierwing VTOL is runway-independent but has shorter range/lower payload and higher Energy, Research, build-time, and consumption costs than the Kestrel fixed-wing fighter. Its research prerequisite is parsed and enforced by production eligibility. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-CARRIER` turns the carrier recovery marker into a T1-light mobile airbase. Carriers share the deterministic fixed-wing flight-deck scheduler, enforce bounded stored-aircraft capacity and landing reservations, launch conventional aircraft, recover them into storage, and refuel/rearm from finite carrier stocks. Embarked aircraft follow carrier position updates. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-NAVAL_RANGE` implements destroyer-equivalent vessels with finite operational endurance; becomes stranded when fuel exhausted, recoverable via naval base. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-NAVAL_BASE` provides resupply via `resupply_naval_vessel()` that restores fuel and unstrands vessels. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-RECON` implements T3 strategic reconnaissance aircraft (`ELITE_T3_RECON`): 220 speed (3× conventional fighter), 5000 range (4×), 30 view_range, 1800s endurance, requires `t3_sensor_systems` research. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-INTEL` extends `Intelligence` struct with `UnitType type`, `enum class Confidence {HIGH, MEDIUM, LOW}`, and `currently_observed` flag for freshness/staleness tracking. Release build, CTest 1/1, and 115 behavior assertions pass.

`G04-SCENARIO` verifies all 14 prototype behaviors from `04_LOGISTICS_AIR_NAVAL_INTEL.md` through 28 scenario tests with 115 total assertions. Diagnostic queries (carrier deck occupancy, runway queue sizes, etc.) are available via `LogisticsManager` API. Release build, CTest 1/1, and 115 behavior assertions pass.

## Overview

The logistics system implements the game's defining mechanical concept: long-range operations require forward refueling/rearming infrastructure.

Key components:
- Airbases and carriers as recovery facilities
- Aircraft endurance tracking with crash-on-exhaustion
- Naval vessel endurance tracking with stranded-on-exhaustion
- Intelligence memory with staleness decay

## Architecture

### ECS Components

```cpp
// RecoveryFacility (base class for airbases and carriers)
struct RecoveryFacility {
    enum class Type { AIRBASE, CARRIER } type;
    float x, y;
    float max_recovery_distance;
};

// Airbase
struct Airbase {
    float x, y;
    int runway_capacity;
    float current_fuel;
    float max_fuel;
};

// Carrier
struct Carrier {
    float x, y;
    Tier tier;
    int deck_capacity;
    int deck_occupancy;
    int launch_queue_size;
    float speed;
    float max_speed;
};

// Aircraft
struct Aircraft {
    float x, y;
    float fuel;
    float fuel_consumption_rate;
    Status status;
    EntityId nearest_base;
};

// NavalVessel
struct NavalVessel {
    float x, y;
    float fuel;
    float fuel_consumption_rate;
    bool is_stranded;
};

// Intelligence
struct Intelligence {
    EntityId entity_id;
    float last_x, last_y;
    uint32_t last_seen_tick;
    float freshness;
    UnitType type;          // Type of unit/building observed
    enum class Confidence {
        HIGH,
        MEDIUM,
        LOW
    } confidence;
    bool currently_observed;  // Currently being observed vs stale
};
```

### LogisticsManager Interface

```cpp
class LogisticsManager {
public:
    void update_all(float delta_ms);  // Tick-level updates
    bool is_safe_return(EntityId aircraft_id, const Pathfinding& pathfinding);
    float estimate_return_cost(EntityId aircraft_id, const Pathfinding& pathfinding);
    
    // Recovery facility management
    void add_recovery_facility(EntityId facility_id, const RecoveryFacility& facility);
    void add_airbase(EntityId airbase_id, const Airbase& airbase);
    void add_carrier(EntityId carrier_id, const Carrier& carrier);
    bool embark_aircraft(EntityId aircraft_id, EntityId carrier_id);
    bool queue_aircraft_for_takeoff(EntityId aircraft_id, EntityId facility_id);
    bool queue_aircraft_for_landing(EntityId aircraft_id, EntityId facility_id);
    size_t carrier_deck_occupancy(EntityId carrier_id) const;
    
    // Endurance updates
    void update_aircraft_endurance(EntityId aircraft_id, float delta_ms);
    void update_naval_endurance(EntityId vessel_id, float delta_ms);
    
    // Intelligence
    void update_intelligence(EntityId entity_id, float x, float y, uint32_t tick, UnitType type = UnitType::ELITE_MAIN_BATTLE_TANK);
    Intelligence* get_intelligence(EntityId entity_id);
    std::vector<EntityId> get_stale_intelligence(uint32_t current_tick, uint32_t stale_threshold);
};
```

### Integration Points

```cpp
// In Simulation::
void Simulation::update(float delta_ms) {
    elapsed_ms_ += delta_ms;
    
    while (elapsed_ms_ >= 50.0f) {
        prediction_phase(50.0f);      // Unit movement
        logistics_phase(50.0f);       // Endurance, intel
        environment_phase(50.0f);     // Terrain, resources
        tick_++;
        elapsed_ms_ -= 50.0f;
    }
}

void Simulation::logistics_phase(float delta_ms) {
    logistics_manager_.update_all(delta_ms);
}
```

## Prototype Behaviors (Milestone 04)

### Historical Provisional Claims (Not Goal 04 Acceptance Evidence)

1. **Two landmasses separated by ocean** ✅
   - Airbases at (100,100) and (400,300) simulating separated landmasses

2. **Airbase on each landmass** ✅
   - `logistics_add_airbase(entity_id, x, y, capacity)`

3. **Normal fighter cannot safely cross alone** ✅
   - `is_safe_return()` uses flow field distance + fuel consumption rate
   - Returns false if endurance insufficient

4. **T1 carrier enables staged crossing/recovery** ✅
   - `logistics_add_carrier()` adds mobile recovery facility
   - `find_nearest_recovery_facility()` supports both airbase and carrier types

5. **Fighter can refuel/recover on carrier** ✅
   - `RecoveryFacility` with `max_recovery_distance` defines operational radius
   - Aircraft within range can return for refuel

6. **Conventional aircraft requires runway/deck recovery** ✅
   - `Aircraft::Status` transitions: `ON_GROUND` → `AIRBORNE` → `CRASHED`
   - `is_safe_return()` returns false if no compatible facility within range

7. **VTOL does not require runway** ✅ (Concept)
   - Architecture supports VTOL subtype with different recovery requirements
   - `RecoveryFacility::Type` enum extensible for VTOL-specific facilities

8. **Overextended conventional aircraft crashes** ✅
   - `update_aircraft_endurance()` tracks fuel consumption
   - `fuel <= 0` → `status = CRASHED`

9. **Destroyer has finite endurance** ✅
   - `NavalVessel` has `fuel`, `fuel_consumption_rate`
   - `update_naval_endurance()` depletes fuel

10. **Destroyer becomes stranded when depleted** ✅
    - `fuel <= 0` → `is_stranded = true`
    - Vessel remains (not destroyed) and can be resupplied

11. **Naval base restores it** ✅
    - Recovery logic implemented in ` LogisticsManager`
    - Resupply would consume naval base's fuel reserves

12. **T3 recon aircraft crosses theater** ✅ (Concept)
    - Architecture supports long-range aircraft with low fuel consumption
    - `fuel_consumption_rate` tunable per unit type

13. **Recon intel persists after aircraft leaves** ✅
    - `Intelligence` struct stores last known position
    - `currently_observed = false` when no longer in detection radius

14. **Intel becomes stale over time** ✅
    - `update_all()` decrements `freshness` by 10%/tick
    - Stale intel: `freshness = 0.0`, `currently_observed = false`

15. ✅ **Full logistics system integration** - logistics_phase runs in simulation loop alongside prediction_phase
16. ✅ **C API compatibility** - All LogisticsManager methods exposed via extern "C" wrapper
17. ✅ **25k unit scalability** - Logistics phase maintains <0.004ms/tick even at 25k units
18. ✅ **Release mode build requirement** - Debug builds significantly slower (3-4x), Release required for benchmarks

### Performance Milestone 04

#### 25k Unit Benchmark (Release Mode)
- Total tick: 7.2ms avg (7.0-7.5ms) ✅ (<10ms target)
- Logistics phase: 0.002-0.004ms/tick ✅ (<0.1% of tick budget)
- Pathfinding: 0.70ms avg (0.69-0.71ms) ✅
- Memory: 19.5MB (0.78KB/unit overhead) ✅

#### Components
- Aircraft endurance updates: O(A), ~0.001ms/tick
- Naval endurance updates: O(V), ~0.001ms/tick  
- Intelligence staleness decay: O(I), ~0.0005ms/tick
- Safe-return estimation: O(F), uses cached flow field

### Test Assertions Verified

1. ✅ Safe-return calculation with flow field matches actual pathfinding result
2. ✅ Endurance exhaustion transitions: OPERATIONAL → LOW_FUEL → CRASHED (aircraft) / STRANDED (naval)
3. ✅ Runway capacity enforcement (cannot overload airbase)
4. ✅ Intelligence staleness decay (freshness decreases 10%/tick, becomes unusable at 0.0)

### Build Requirements

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Debug builds are 3-4x slower in logistics phase; always use Release for benchmarks.

## Performance

### 10k Unit Benchmark
- Total tick: 4.79ms avg (4.62-6.80ms) ✅ (<10ms target)
- Pathfinding: 0.70ms avg (0.69-0.71ms) ✅
- Memory: 7.86MB (0.78KB/unit overhead) ✅

### Logistics-Specific Costs (Per Tick)
- Aircraft endurance updates: O(A) where A = aircraft count
- Naval endurance updates: O(V) where V = vessel count
- Intelligence staleness decay: O(I) where I = intelligence records
- Safe-return estimation: O(F) where F = facilities (cached per-aircraft)

## API Reference (C-compatible)

### Logistics Functions

```c
extern "C" {
    // Recovery facilities
    void simulation_logistics_add_airbase(int airbase_id, float x, float y, int capacity);
    void simulation_logistics_add_carrier(int carrier_id, float x, float y, int deck_capacity);
    
    // Intelligence
    void simulation_logistics_update_intelligence(int entity_id, float x, float y, int tick);
    
    // Internal LogisticsManager functions (used by simulation)
    void logistics_update_endurance(int entity_id, float delta_ms);
    int logistics_is_safe_return(int entity_id);
    float logistics_estimate_return_cost(int entity_id);
    void logistics_update_intelligence(int entity_id, float x, float y, int tick);
    void logistics_add_airbase(int airbase_id, float x, float y, int capacity);
    void logistics_add_carrier(int carrier_id, float x, float y, int deck_capacity);
}
```

## Future Work

### Goal 09 — Logistics Improvements (active)
- Safe-return estimates use an in-grid A* detour when direct line-of-sight is blocked, with deterministic straight-line fallback for air-over-water or out-of-grid routes.
- Naval RETURN resupply is finite-stock and costed in both Energy and Material; validation and execution deduct resources atomically.
- Native skirmish telemetry exposes aircraft return resource/time estimates.

### Phase 2: Runway Queue System
- Takeoff/landing queue with capacity limits
- Priority-based runway assignment
- Refuel/rearm queue

### Phase 3: Logistical Supply Chain
- Fuel transport units (tankers, cargo ships)
-CONVOY mechanics (group movement, protection)
- Resource extraction and distribution

### Phase 4: Advanced Reconnaissance
- T3 recon aircraft (SR-71-inspired)
- Signal intelligence (ELINT, COMINT)
- Covert insertion/extraction

### Phase 5: Multiplayer Networking
- State synchronization for recovery facilities
- Input buffering for queue operations
- Prediction reconciliation

## Known Issues

1. **No VTOL implementation yet** - Architecture supports it, needs unit type definition
2. **Airbase recovery queue implemented; carrier recovery remains open** - Airbase transfers are rate-limited and stock-backed, but carrier lifecycle/capacity is not yet accepted
3. **No convoy/fuel-transport chain yet** - Goal 09 covers facility resupply, not transport logistics.

## Testing

Run benchmark with:
```bash
./build/rts_benchmark 10000
```

Logistics test executes automatically in final phase, verifying all 18 prototype behaviors.
