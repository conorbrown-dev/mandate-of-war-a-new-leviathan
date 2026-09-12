#pragma once

#include <deque>
#include <limits>
#include <map>
#include <tuple>

#include "ecs/entity.hpp"
#include "pathfinding/pathfinding.hpp"
#include "ecs/components/airbase.hpp"
#include "ecs/components/aircraft.hpp"
#include "ecs/components/carrier.hpp"
#include "ecs/components/naval_vessel.hpp"
#include "ecs/components/intelligence.hpp"
#include "ecs/components/recovery_facility.hpp"

namespace rts {

class ComponentManager;

struct SafeReturnEstimate {
    EntityId facility_id = INVALID_ENTITY;
    float distance = std::numeric_limits<float>::infinity();
    float energy_cost = std::numeric_limits<float>::infinity();
    float material_cost = std::numeric_limits<float>::infinity();
    float flight_time_ms = std::numeric_limits<float>::infinity();
    bool safe = false;
};

class LogisticsManager {
public:
    LogisticsManager() = default;
    void set_component_manager(ComponentManager* cm) { component_manager_ = cm; }

    // Safe-return calculation
    bool is_safe_return(EntityId aircraft_id, const Pathfinding& pathfinding);
    float estimate_return_cost(EntityId aircraft_id, const Pathfinding& pathfinding);
    SafeReturnEstimate estimate_safe_return(EntityId aircraft_id, const Pathfinding& pathfinding);

    // Recovery facility lookup
    EntityId find_nearest_recovery_facility(float x, float y, RecoveryFacility::Type type, EntityId observer = INVALID_ENTITY);
    EntityId find_nearest_aircraft_recovery_facility(float x, float y, EntityId observer = INVALID_ENTITY);
    bool compatible_facility(EntityId observer, EntityId facility) const;
    bool update_recovery_facility_position(EntityId facility_id, float x, float y);
    bool set_airbase_runway_usable(EntityId airbase_id, bool usable);

    // Intelligence management
    void update_intelligence(EntityId entity_id, float x, float y, uint32_t tick);
    Intelligence* get_intelligence(EntityId entity_id);
    std::vector<EntityId> get_stale_intelligence(uint32_t current_tick, uint32_t stale_threshold);

    // Endurance updates
    void update_aircraft_endurance(EntityId aircraft_id, float delta_ms);
    void update_naval_endurance(EntityId vessel_id, float delta_ms);

    // Resupply
    void resupply_naval_vessel(EntityId vessel_id, float amount);

    // Facility management
    void add_airbase(EntityId airbase_id, const Airbase& airbase);
    void add_carrier(EntityId carrier_id, const Carrier& carrier);
    void add_naval_base(EntityId base_id, float x, float y, float max_recovery_distance);

    // Conventional fixed-wing runway/flight-deck lifecycle
    bool embark_aircraft(EntityId aircraft_id, EntityId carrier_id);
    bool queue_aircraft_for_takeoff(EntityId aircraft_id, EntityId facility_id);
    bool order_aircraft_return(EntityId aircraft_id, EntityId facility_id);
    bool queue_aircraft_for_landing(EntityId aircraft_id, EntityId facility_id);
    bool launch_vtol(EntityId aircraft_id);
    bool land_vtol(EntityId aircraft_id);
    size_t takeoff_queue_size(EntityId facility_id) const;
    size_t landing_queue_size(EntityId facility_id) const;
    size_t active_runway_operations(EntityId facility_id) const;
    size_t carrier_deck_occupancy(EntityId carrier_id) const;
    size_t carrier_recovery_reservations(EntityId carrier_id) const;
    void remove_aircraft(EntityId aircraft_id);

    size_t safe_return_cache_hits() const { return safe_return_cache_hits_; }
    size_t safe_return_cache_misses() const { return safe_return_cache_misses_; }
    size_t facility_lookup_cache_hits() const { return facility_lookup_cache_hits_; }
    size_t facility_lookup_cache_misses() const { return facility_lookup_cache_misses_; }

    // ECS convenience methods for simulation.cpp
    void add_aircraft(EntityId airbase_id, float x, float y, float fuel);
    void add_naval_vessel(EntityId vessel_id, float x, float y, float fuel);

    // Batch recovery for performance
    void batch_safe_return_check(const std::vector<EntityId>& aircraft_ids, const Pathfinding& pathfinding);
    void update_all(float delta_ms);
    void reset();

    // Counters for state verification
    size_t crashed_aircraft_count() const { return crashed_aircraft_count_; }
    size_t stranded_naval_count() const { return stranded_naval_count_; }
    void reset_counters() { crashed_aircraft_count_ = 0; stranded_naval_count_ = 0; }

    // Verification/assertion methods for testing
    bool verify_safe_return_calculation(EntityId aircraft_id, const Pathfinding& pathfinding, float tolerance = 0.1f);
    bool verify_crashed_state(EntityId aircraft_id, float expected_fuel_threshold);
    bool verify_stranded_state(EntityId vessel_id, float expected_fuel_threshold);
    bool verify_intelligence_staleness(EntityId intel_id, uint32_t current_tick, uint32_t ticks_since_update, float expected_decay_rate = 0.1f);
    int count_crashed_aircraft();
    int count_stranded_naval();

private:
    enum class RunwayOperationType {
        TAKEOFF,
        LANDING
    };

    struct RunwayOperation {
        EntityId aircraft_id = INVALID_ENTITY;
        RunwayOperationType type = RunwayOperationType::TAKEOFF;
        float remaining_ms = 0.0f;
    };

    struct FlightDeckOperations {
        std::deque<EntityId> takeoff_queue;
        std::deque<EntityId> landing_queue;
        std::vector<RunwayOperation> active;
        std::vector<EntityId> recovering;
        std::vector<EntityId> stored_aircraft;
    };

    struct FacilityLookupKey {
        int x = 0;
        int y = 0;
        RecoveryFacility::Type type = RecoveryFacility::Type::AIRBASE;
        int faction = -1;

        bool operator<(const FacilityLookupKey& other) const {
            return std::tie(type, faction, x, y) < std::tie(other.type, other.faction, other.x, other.y);
        }
    };

    struct FacilityLookupCacheEntry {
        EntityId facility_id = INVALID_ENTITY;
        uint64_t facility_revision = 0;
        float origin_x = 0.0f;
        float origin_y = 0.0f;
        float stable_radius = 0.0f;
    };

    struct SafeReturnCacheEntry {
        float x = 0.0f;
        float y = 0.0f;
        float facility_x = 0.0f;
        float facility_y = 0.0f;
        float cruise_speed = 0.0f;
        float energy_rate = 0.0f;
        float material_rate = 0.0f;
        uint64_t facility_revision = 0;
        SafeReturnEstimate estimate;
    };

    ComponentManager* component_manager_ = nullptr;
    std::map<EntityId, FlightDeckOperations> flight_deck_operations_;
    std::map<RecoveryFacility::Type, std::vector<EntityId>> recovery_facilities_by_type_;
    std::map<FacilityLookupKey, FacilityLookupCacheEntry> facility_lookup_cache_;
    std::map<EntityId, SafeReturnCacheEntry> safe_return_cache_;
    uint64_t facility_revision_ = 0;
    size_t safe_return_cache_hits_ = 0;
    size_t safe_return_cache_misses_ = 0;
    size_t facility_lookup_cache_hits_ = 0;
    size_t facility_lookup_cache_misses_ = 0;

    void update_flight_deck_operations(float delta_ms);
    void update_recovering_aircraft(EntityId facility_id, FlightDeckOperations& operations, float delta_ms);
    void update_queue_slots(FlightDeckOperations& operations);
    Airbase* get_flight_deck(EntityId facility_id) const;
    bool carrier_has_available_recovery_slot(EntityId carrier_id) const;
    bool carrier_has_recovery_reservation(EntityId carrier_id, EntityId aircraft_id) const;
    EntityId find_aircraft_recovery_facility(EntityId aircraft_id, const Aircraft& aircraft);
    void sync_carrier_diagnostics(EntityId carrier_id, const FlightDeckOperations& operations);
    bool is_aircraft_near_facility(const Aircraft& aircraft, EntityId facility_id) const;
    void index_recovery_facility(EntityId facility_id, RecoveryFacility::Type type);
    void invalidate_facility_caches();
    static bool is_in_powered_flight(Aircraft::Status status);
    static bool has_safe_return_reserves(const Aircraft& aircraft, const SafeReturnEstimate& estimate);
    void apply_safe_return_diagnostics(Aircraft& aircraft, const SafeReturnEstimate& estimate) const;

    static constexpr float RUNWAY_OPERATION_MS = 1000.0f;
    static constexpr float FACILITY_LOOKUP_CELL_SIZE = 128.0f;
    static constexpr float SAFE_RETURN_CACHE_DISTANCE = 25.0f;
    static constexpr float SAFE_RETURN_DISTANCE_RESERVE = 25.0f;

    // Cached indices for performance
    std::vector<std::pair<float, EntityId>> nearby_facilities_cache_;
    static constexpr int CACHE_SIZE = 100;

    // Counter for verification
    size_t crashed_aircraft_count_ = 0;
    size_t stranded_naval_count_ = 0;
};

} // namespace rts
