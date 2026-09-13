#include "logistics/logistics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "ecs/component_manager.hpp"
#include "spatial/spatial_grid.hpp"
#include "ecs/components/faction.hpp"

namespace rts {

// Safe-return calculation using cached direct-flight distance.
bool LogisticsManager::is_safe_return(EntityId aircraft_id, const Pathfinding& pathfinding) {
    return estimate_safe_return(aircraft_id, pathfinding).safe;
}

float LogisticsManager::estimate_return_cost(EntityId aircraft_id, const Pathfinding& pathfinding) {
    return estimate_safe_return(aircraft_id, pathfinding).energy_cost;
}

bool LogisticsManager::is_in_powered_flight(Aircraft::Status status) {
    return status == Aircraft::Status::TAKING_OFF ||
           status == Aircraft::Status::AIRBORNE ||
           status == Aircraft::Status::RETURNING ||
           status == Aircraft::Status::QUEUED_FOR_LANDING ||
           status == Aircraft::Status::LANDING;
}

bool LogisticsManager::has_safe_return_reserves(
    const Aircraft& aircraft,
    const SafeReturnEstimate& estimate) {
    if (estimate.facility_id == INVALID_ENTITY || !std::isfinite(estimate.energy_cost) ||
        !std::isfinite(estimate.material_cost) || !std::isfinite(estimate.flight_time_ms)) {
        return false;
    }

    const bool has_time = aircraft.max_airborne_time_ms <= 0.0f ||
        aircraft.airborne_time_ms + estimate.flight_time_ms <= aircraft.max_airborne_time_ms;
    return aircraft.fuel >= estimate.energy_cost &&
           aircraft.material >= estimate.material_cost &&
           has_time;
}

void LogisticsManager::apply_safe_return_diagnostics(
    Aircraft& aircraft,
    const SafeReturnEstimate& estimate) const {
    aircraft.closest_recovery_facility = estimate.facility_id;
    aircraft.predicted_return_energy = estimate.energy_cost;
    aircraft.predicted_return_material = estimate.material_cost;
    aircraft.predicted_return_time_ms = estimate.flight_time_ms;
    aircraft.safe_return = estimate.safe;
}

SafeReturnEstimate LogisticsManager::estimate_safe_return(
    EntityId aircraft_id,
    const Pathfinding& pathfinding) {
    SafeReturnEstimate unavailable;
    if (!component_manager_) {
        return unavailable;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    if (!aircraft || !is_in_powered_flight(aircraft->status) ||
        !std::isfinite(aircraft->x) || !std::isfinite(aircraft->y) ||
        !std::isfinite(aircraft->cruise_speed) || aircraft->cruise_speed <= 0.0f ||
        !std::isfinite(aircraft->fuel_consumption_rate) || aircraft->fuel_consumption_rate < 0.0f ||
        !std::isfinite(aircraft->material_consumption_rate) || aircraft->material_consumption_rate < 0.0f) {
        if (aircraft) {
            apply_safe_return_diagnostics(*aircraft, unavailable);
        }
        return unavailable;
    }

    if (aircraft->type == Aircraft::Type::VTOL) {
        SafeReturnEstimate estimate;
        estimate.distance = 0.0f;
        estimate.energy_cost = 0.0f;
        estimate.material_cost = 0.0f;
        estimate.flight_time_ms = 0.0f;
        estimate.safe = aircraft->fuel > 0.0f &&
            (aircraft->material_consumption_rate <= 0.0f || aircraft->material > 0.0f) &&
            (aircraft->max_airborne_time_ms <= 0.0f ||
             aircraft->airborne_time_ms < aircraft->max_airborne_time_ms);
        apply_safe_return_diagnostics(*aircraft, estimate);
        return estimate;
    }

    auto cached = safe_return_cache_.find(aircraft_id);
    if (cached != safe_return_cache_.end()) {
        const float dx = aircraft->x - cached->second.x;
        const float dy = aircraft->y - cached->second.y;
        const bool position_is_cached = dx * dx + dy * dy <=
            SAFE_RETURN_CACHE_DISTANCE * SAFE_RETURN_CACHE_DISTANCE;
        const auto* cached_facility = component_manager_->get_component<RecoveryFacility>(
            cached->second.estimate.facility_id
        );
        const bool facility_is_cached = cached_facility &&
            cached_facility->x - cached->second.facility_x <= SAFE_RETURN_CACHE_DISTANCE &&
            cached_facility->x - cached->second.facility_x >= -SAFE_RETURN_CACHE_DISTANCE &&
            cached_facility->y - cached->second.facility_y <= SAFE_RETURN_CACHE_DISTANCE &&
            cached_facility->y - cached->second.facility_y >= -SAFE_RETURN_CACHE_DISTANCE;
        const bool facility_available = cached_facility &&
            (cached_facility->type != RecoveryFacility::Type::CARRIER ||
             carrier_has_available_recovery_slot(cached->second.estimate.facility_id) ||
             carrier_has_recovery_reservation(cached->second.estimate.facility_id, aircraft_id));
        if (position_is_cached && facility_is_cached && facility_available &&
            cached->second.facility_revision == facility_revision_ &&
            compatible_facility(aircraft_id, cached->second.estimate.facility_id) &&
            cached->second.cruise_speed == aircraft->cruise_speed &&
            cached->second.energy_rate == aircraft->fuel_consumption_rate &&
            cached->second.material_rate == aircraft->material_consumption_rate) {
            ++safe_return_cache_hits_;
            SafeReturnEstimate estimate = cached->second.estimate;
            estimate.safe = has_safe_return_reserves(*aircraft, estimate);
            apply_safe_return_diagnostics(*aircraft, estimate);
            return estimate;
        }
    }

    ++safe_return_cache_misses_;
    SafeReturnEstimate estimate;
    estimate.facility_id = find_aircraft_recovery_facility(aircraft_id, *aircraft);
    auto* facility = component_manager_->get_component<RecoveryFacility>(estimate.facility_id);
    if (!facility) {
        apply_safe_return_diagnostics(*aircraft, estimate);
        safe_return_cache_[aircraft_id] = {
            aircraft->x,
            aircraft->y,
            0.0f,
            0.0f,
            aircraft->cruise_speed,
            aircraft->fuel_consumption_rate,
            aircraft->material_consumption_rate,
            facility_revision_,
            estimate
        };
        return estimate;
    }

    const float dx = facility->x - aircraft->x;
    const float dy = facility->y - aircraft->y;
    const float straight_line_distance = std::sqrt(dx * dx + dy * dy);
    // Use the authoritative route when the theater has a traversable path.
    // Aircraft may legitimately cross water or leave the configured grid, so
    // retain a deterministic straight-line fallback when A* has no route.
    const int start_x = pathfinding.to_grid_x(aircraft->x);
    const int start_y = pathfinding.to_grid_y(aircraft->y);
    const int goal_x = pathfinding.to_grid_x(facility->x);
    const int goal_y = pathfinding.to_grid_y(facility->y);
    const bool route_query_valid = pathfinding.is_walkable(start_x, start_y) && pathfinding.is_walkable(goal_x, goal_y);
    const bool direct_route_clear = route_query_valid && pathfinding.has_line_of_sight(
        aircraft->x, aircraft->y, facility->x, facility->y);
    const auto route = route_query_valid && !direct_route_clear
        ? pathfinding.find_path(aircraft->x, aircraft->y, facility->x, facility->y)
        : std::vector<std::pair<float, float>>{};
    float route_distance = 0.0f;
    if (route.size() > 1) {
        for (size_t i = 1; i < route.size(); ++i) {
            const float segment_x = route[i].first - route[i - 1].first;
            const float segment_y = route[i].second - route[i - 1].second;
            route_distance += std::sqrt(segment_x * segment_x + segment_y * segment_y);
        }
    }
    const float travel_distance = route_distance > 0.0f ? route_distance : straight_line_distance;
    estimate.distance = travel_distance + SAFE_RETURN_DISTANCE_RESERVE;
    const float flight_time_seconds = estimate.distance / aircraft->cruise_speed;
    estimate.flight_time_ms = flight_time_seconds * 1000.0f;
    estimate.energy_cost = flight_time_seconds * aircraft->fuel_consumption_rate;
    estimate.material_cost = flight_time_seconds * aircraft->material_consumption_rate;
    estimate.safe = has_safe_return_reserves(*aircraft, estimate);

    safe_return_cache_[aircraft_id] = {
        aircraft->x,
        aircraft->y,
        facility->x,
        facility->y,
        aircraft->cruise_speed,
        aircraft->fuel_consumption_rate,
        aircraft->material_consumption_rate,
        facility_revision_,
        estimate
    };
    apply_safe_return_diagnostics(*aircraft, estimate);
    return estimate;
}

bool LogisticsManager::compatible_facility(EntityId observer, EntityId facility) const {
    if (!component_manager_) return false;
    const auto* health = component_manager_->get_component<Health>(facility);
    if (health && (health->is_dead || health->current <= 0)) return false;
    const auto* owner = component_manager_->get_component<Faction>(observer);
    const auto* facility_owner = component_manager_->get_component<Faction>(facility);
    // Unowned prototype facilities are neutral; owned facilities are faction restricted.
    return !facility_owner || (owner && owner->faction_id == facility_owner->faction_id) || observer == INVALID_ENTITY;
}

EntityId LogisticsManager::find_nearest_recovery_facility(float x, float y, RecoveryFacility::Type type, EntityId observer) {
    if (!component_manager_ || !std::isfinite(x) || !std::isfinite(y)) {
        return INVALID_ENTITY;
    }

    const auto* owner = component_manager_->get_component<Faction>(observer);
    const FacilityLookupKey key{
        static_cast<int>(std::floor(x / FACILITY_LOOKUP_CELL_SIZE)),
        static_cast<int>(std::floor(y / FACILITY_LOOKUP_CELL_SIZE)),
        type,
        owner ? static_cast<int>(owner->faction_id) : -1
    };
    auto cached = facility_lookup_cache_.find(key);
    if (cached != facility_lookup_cache_.end() &&
        cached->second.facility_revision == facility_revision_ &&
        compatible_facility(observer, cached->second.facility_id)) {
        const float dx = x - cached->second.origin_x;
        const float dy = y - cached->second.origin_y;
        if (dx * dx + dy * dy <= cached->second.stable_radius * cached->second.stable_radius) {
            ++facility_lookup_cache_hits_;
            return cached->second.facility_id;
        }
    }

    ++facility_lookup_cache_misses_;
    EntityId nearest = INVALID_ENTITY;
    float nearest_distance = std::numeric_limits<float>::infinity();
    float second_distance = std::numeric_limits<float>::infinity();
    const auto facilities = recovery_facilities_by_type_.find(type);
    if (facilities == recovery_facilities_by_type_.end()) {
        facility_lookup_cache_[key] = {
            INVALID_ENTITY,
            facility_revision_,
            x,
            y,
            std::numeric_limits<float>::infinity()
        };
        return INVALID_ENTITY;
    }

    for (EntityId entity_id : facilities->second) {
        auto* facility = component_manager_->get_component<RecoveryFacility>(entity_id);
        if (!facility || !facility->supports(type) || !compatible_facility(observer, entity_id)) {
            continue;
        }
        if (type == RecoveryFacility::Type::AIRBASE) {
            auto* airbase = component_manager_->get_component<Airbase>(entity_id);
            if (!airbase || !airbase->runway_usable) {
                continue;
            }
        } else if (type == RecoveryFacility::Type::CARRIER &&
                   !carrier_has_available_recovery_slot(entity_id)) {
            continue;
        }

        const float dx = facility->x - x;
        const float dy = facility->y - y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < nearest_distance ||
            (distance == nearest_distance && entity_id < nearest)) {
            second_distance = nearest_distance;
            nearest_distance = distance;
            nearest = entity_id;
        } else if (distance < second_distance) {
            second_distance = distance;
        }
    }

    const float stable_radius = std::isfinite(second_distance)
        ? std::max(0.0f, (second_distance - nearest_distance) * 0.5f)
        : std::numeric_limits<float>::infinity();
    facility_lookup_cache_[key] = {
        nearest,
        facility_revision_,
        x,
        y,
        stable_radius
    };
    return nearest;
}

EntityId LogisticsManager::find_nearest_aircraft_recovery_facility(float x, float y, EntityId observer) {
    const EntityId airbase = find_nearest_recovery_facility(x, y, RecoveryFacility::Type::AIRBASE, observer);
    const EntityId carrier = find_nearest_recovery_facility(x, y, RecoveryFacility::Type::CARRIER, observer);
    if (airbase == INVALID_ENTITY) {
        return carrier;
    }
    if (carrier == INVALID_ENTITY) {
        return airbase;
    }

    auto* airbase_facility = component_manager_->get_component<RecoveryFacility>(airbase);
    auto* carrier_facility = component_manager_->get_component<RecoveryFacility>(carrier);
    if (!airbase_facility) {
        return carrier_facility ? carrier : INVALID_ENTITY;
    }
    if (!carrier_facility) {
        return airbase;
    }

    const float airbase_dx = airbase_facility->x - x;
    const float airbase_dy = airbase_facility->y - y;
    const float carrier_dx = carrier_facility->x - x;
    const float carrier_dy = carrier_facility->y - y;
    const float airbase_distance = airbase_dx * airbase_dx + airbase_dy * airbase_dy;
    const float carrier_distance = carrier_dx * carrier_dx + carrier_dy * carrier_dy;
    return carrier_distance < airbase_distance ? carrier : airbase;
}

void LogisticsManager::index_recovery_facility(EntityId facility_id, RecoveryFacility::Type type) {
    auto& facilities = recovery_facilities_by_type_[type];
    if (std::find(facilities.begin(), facilities.end(), facility_id) == facilities.end()) {
        facilities.push_back(facility_id);
        std::sort(facilities.begin(), facilities.end());
    }
}

void LogisticsManager::invalidate_facility_caches() {
    ++facility_revision_;
    facility_lookup_cache_.clear();
    safe_return_cache_.clear();
}

bool LogisticsManager::update_recovery_facility_position(EntityId facility_id, float x, float y) {
    if (!component_manager_ || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }
    auto* facility = component_manager_->get_component<RecoveryFacility>(facility_id);
    if (!facility) {
        return false;
    }

    const bool moved = facility->x != x || facility->y != y;
    facility->x = x;
    facility->y = y;
    if (auto* airbase = component_manager_->get_component<Airbase>(facility_id)) {
        airbase->x = x;
        airbase->y = y;
    }
    if (auto* carrier = component_manager_->get_component<Carrier>(facility_id)) {
        carrier->x = x;
        carrier->y = y;
        auto operations = flight_deck_operations_.find(facility_id);
        if (operations != flight_deck_operations_.end()) {
            for (EntityId aircraft_id : operations->second.stored_aircraft) {
                if (auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id)) {
                    aircraft->x = x;
                    aircraft->y = y;
                }
                if (auto* position = component_manager_->get_component<Position>(aircraft_id)) {
                    position->x = x;
                    position->y = y;
                }
            }
        }
    }
    // Moving a carrier must refresh future nearest-facility lookups, but it
    // must not discard every aircraft's safe-return estimate. Those entries
    // carry both aircraft and facility positions and expire after bounded
    // movement, preserving the return-distance reserve.
    if (moved) {
        facility_lookup_cache_.clear();
    }
    return true;
}

bool LogisticsManager::set_airbase_runway_usable(EntityId airbase_id, bool usable) {
    if (!component_manager_) {
        return false;
    }
    auto* airbase = component_manager_->get_component<Airbase>(airbase_id);
    auto* facility = component_manager_->get_component<RecoveryFacility>(airbase_id);
    if (!airbase || !facility || facility->type != RecoveryFacility::Type::AIRBASE) {
        return false;
    }
    if (airbase->runway_usable != usable) {
        airbase->runway_usable = usable;
        invalidate_facility_caches();
    }
    return true;
}

// Intelligence management
void LogisticsManager::update_intelligence(EntityId entity_id, float x, float y, uint32_t tick) {
    if (!component_manager_) {
        return;
    }

    Intelligence intel;
    intel.entity_id = entity_id;
    intel.last_x = x;
    intel.last_y = y;
    intel.last_seen_tick = tick;
    intel.freshness = 1.0f;
    intel.currently_observed = true;
    intelligence_memory_.erase(entity_id);
    component_manager_->add_component<Intelligence>(entity_id, intel);
}

Intelligence* LogisticsManager::get_intelligence(EntityId entity_id) {
    if (!component_manager_) {
        return nullptr;
    }
    
    if (auto* live = component_manager_->get_component<Intelligence>(entity_id)) {
        return live;
    }
    auto remembered = intelligence_memory_.find(entity_id);
    return remembered == intelligence_memory_.end() ? nullptr : &remembered->second;
}

void LogisticsManager::archive_intelligence(EntityId entity_id) {
    if (!component_manager_) {
        return;
    }
    if (const auto* intel = component_manager_->get_component<Intelligence>(entity_id)) {
        Intelligence remembered = *intel;
        remembered.currently_observed = false;
        intelligence_memory_[entity_id] = remembered;
        component_manager_->remove_component<Intelligence>(entity_id);
    }
}

std::vector<Intelligence> LogisticsManager::intelligence_snapshot() const {
    std::vector<Intelligence> snapshot;
    snapshot.reserve(intelligence_memory_.size());
    for (const auto& [entity_id, intel] : intelligence_memory_) snapshot.push_back(intel);
    if (!component_manager_) return snapshot;
    for (auto entity_id : component_manager_->entities_with<Intelligence>()) {
        if (const auto* intel = component_manager_->get_component<Intelligence>(entity_id)) snapshot.push_back(*intel);
    }
    std::sort(snapshot.begin(), snapshot.end(), [](const Intelligence& a, const Intelligence& b) {
        return a.entity_id < b.entity_id;
    });
    return snapshot;
}

void LogisticsManager::clear_intelligence_memory(EntityId entity_id) {
    intelligence_memory_.erase(entity_id);
}

std::vector<EntityId> LogisticsManager::get_stale_intelligence(uint32_t current_tick, uint32_t stale_threshold) {
    std::vector<EntityId> stale;
    
    if (!component_manager_) {
        return stale;
    }
    
    auto intel_entities = component_manager_->entities_with<Intelligence>();
    
    for (auto entity_id : intel_entities) {
        auto* intel = component_manager_->get_component<Intelligence>(entity_id);
        if (!intel) {
            continue;
        }
        
        if (!intel->currently_observed && (current_tick - intel->last_seen_tick) > stale_threshold) {
            stale.push_back(entity_id);
        }
    }
    for (const auto& [entity_id, intel] : intelligence_memory_) {
        if (!intel.currently_observed && (current_tick - intel.last_seen_tick) > stale_threshold) {
            stale.push_back(entity_id);
        }
    }
    std::sort(stale.begin(), stale.end());
    stale.erase(std::unique(stale.begin(), stale.end()), stale.end());
    
    return stale;
}

// Endurance updates
void LogisticsManager::update_aircraft_endurance(EntityId aircraft_id, float delta_ms) {
    if (!component_manager_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    if (!aircraft) {
        return;
    }

    if (!is_in_powered_flight(aircraft->status)) {
        return;
    }

    const float seconds = delta_ms / 1000.0f;
    aircraft->fuel -= aircraft->fuel_consumption_rate * seconds;
    aircraft->material -= aircraft->material_consumption_rate * seconds;
    aircraft->airborne_time_ms += delta_ms;

    const bool energy_exhausted = aircraft->fuel <= 0.0f;
    const bool material_exhausted = aircraft->material_consumption_rate > 0.0f &&
                                    aircraft->material <= 0.0f;
    const bool time_exhausted = aircraft->max_airborne_time_ms > 0.0f &&
                                aircraft->airborne_time_ms >= aircraft->max_airborne_time_ms;
    aircraft->fuel = std::max(0.0f, aircraft->fuel);
    aircraft->material = std::max(0.0f, aircraft->material);
    if (energy_exhausted || material_exhausted || time_exhausted) {
        aircraft->status = Aircraft::Status::CRASHED;
        aircraft->safe_return = false;
        ++crashed_aircraft_count_;
    }
}

void LogisticsManager::update_naval_endurance(EntityId vessel_id, float delta_ms) {
    if (!component_manager_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }

    auto* vessel = component_manager_->get_component<NavalVessel>(vessel_id);
    if (!vessel) {
        return;
    }

    float tick_consumption = vessel->fuel_consumption_rate * (delta_ms / 1000.0f);
    vessel->fuel -= tick_consumption;

    if (vessel->fuel <= 0) {
        vessel->fuel = 0;
        vessel->is_stranded = true;
    }
}

// Airbase operations
void LogisticsManager::add_airbase(EntityId airbase_id, const Airbase& airbase) {
    if (!component_manager_) {
        return;
    }
    
    component_manager_->add_component<Airbase>(airbase_id, airbase);
    
    RecoveryFacility facility;
    facility.x = airbase.x;
    facility.y = airbase.y;
    facility.max_recovery_distance = 500.0f;
    facility.type = RecoveryFacility::Type::AIRBASE;
    facility.capabilities = RecoveryFacility::AIR_RECOVERY;
    component_manager_->add_component<RecoveryFacility>(airbase_id, facility);
    flight_deck_operations_[airbase_id] = FlightDeckOperations{};
    index_recovery_facility(airbase_id, facility.type);
    invalidate_facility_caches();
}

Airbase* LogisticsManager::get_flight_deck(EntityId facility_id) const {
    if (!component_manager_) {
        return nullptr;
    }
    if (auto* airbase = component_manager_->get_component<Airbase>(facility_id)) {
        return airbase;
    }
    if (auto* carrier = component_manager_->get_component<Carrier>(facility_id)) {
        return static_cast<Airbase*>(carrier);
    }
    return nullptr;
}

size_t LogisticsManager::carrier_recovery_reservations(EntityId carrier_id) const {
    const auto operations = flight_deck_operations_.find(carrier_id);
    if (operations == flight_deck_operations_.end()) {
        return 0;
    }
    const size_t active_landings = static_cast<size_t>(std::count_if(
        operations->second.active.begin(),
        operations->second.active.end(),
        [](const RunwayOperation& operation) {
            return operation.type == RunwayOperationType::LANDING;
        }
    ));
    return operations->second.landing_queue.size() + active_landings;
}

size_t LogisticsManager::carrier_deck_occupancy(EntityId carrier_id) const {
    const auto operations = flight_deck_operations_.find(carrier_id);
    return operations == flight_deck_operations_.end()
        ? 0
        : operations->second.stored_aircraft.size();
}

bool LogisticsManager::carrier_has_available_recovery_slot(EntityId carrier_id) const {
    if (!component_manager_) {
        return false;
    }
    const auto* carrier = component_manager_->get_component<Carrier>(carrier_id);
    if (!carrier || !carrier->runway_usable || carrier->deck_capacity <= 0) {
        return false;
    }
    return carrier_deck_occupancy(carrier_id) + carrier_recovery_reservations(carrier_id) <
        static_cast<size_t>(carrier->deck_capacity);
}

bool LogisticsManager::carrier_has_recovery_reservation(
    EntityId carrier_id,
    EntityId aircraft_id) const {
    const auto operations = flight_deck_operations_.find(carrier_id);
    if (operations == flight_deck_operations_.end()) {
        return false;
    }
    if (std::find(
            operations->second.landing_queue.begin(),
            operations->second.landing_queue.end(),
            aircraft_id
        ) != operations->second.landing_queue.end()) {
        return true;
    }
    return std::any_of(
        operations->second.active.begin(),
        operations->second.active.end(),
        [aircraft_id](const RunwayOperation& operation) {
            return operation.type == RunwayOperationType::LANDING &&
                   operation.aircraft_id == aircraft_id;
        }
    );
}

EntityId LogisticsManager::find_aircraft_recovery_facility(
    EntityId aircraft_id,
    const Aircraft& aircraft) {
    if (aircraft.target_base != INVALID_ENTITY &&
        compatible_facility(aircraft_id, aircraft.target_base) &&
        component_manager_->get_component<Carrier>(aircraft.target_base) &&
        carrier_has_recovery_reservation(aircraft.target_base, aircraft_id)) {
        return aircraft.target_base;
    }
    return find_nearest_aircraft_recovery_facility(aircraft.x, aircraft.y, aircraft_id);
}

void LogisticsManager::sync_carrier_diagnostics(
    EntityId carrier_id,
    const FlightDeckOperations& operations) {
    if (auto* carrier = component_manager_->get_component<Carrier>(carrier_id)) {
        carrier->deck_occupancy = static_cast<int>(operations.stored_aircraft.size());
        carrier->launch_queue_size = static_cast<int>(operations.takeoff_queue.size());
    }
}

bool LogisticsManager::embark_aircraft(EntityId aircraft_id, EntityId carrier_id) {
    if (!component_manager_ || !compatible_facility(aircraft_id, carrier_id)) {
        return false;
    }
    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    auto* carrier = component_manager_->get_component<Carrier>(carrier_id);
    auto operations = flight_deck_operations_.find(carrier_id);
    if (!aircraft || !carrier || operations == flight_deck_operations_.end() ||
        aircraft->type != Aircraft::Type::CONVENTIONAL ||
        aircraft->status != Aircraft::Status::ON_GROUND ||
        carrier->deck_capacity <= 0 ||
        operations->second.stored_aircraft.size() >= static_cast<size_t>(carrier->deck_capacity) ||
        !is_aircraft_near_facility(*aircraft, carrier_id) ||
        std::find(
            operations->second.stored_aircraft.begin(),
            operations->second.stored_aircraft.end(),
            aircraft_id
        ) != operations->second.stored_aircraft.end()) {
        return false;
    }

    operations->second.stored_aircraft.push_back(aircraft_id);
    aircraft->x = carrier->x;
    aircraft->y = carrier->y;
    aircraft->target_base = carrier_id;
    if (auto* position = component_manager_->get_component<Position>(aircraft_id)) {
        position->x = carrier->x;
        position->y = carrier->y;
    }
    sync_carrier_diagnostics(carrier_id, operations->second);
    invalidate_facility_caches();
    return true;
}

bool LogisticsManager::queue_aircraft_for_takeoff(EntityId aircraft_id, EntityId facility_id) {
    if (!component_manager_ || !compatible_facility(aircraft_id, facility_id)) {
        return false;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    auto* flight_deck = get_flight_deck(facility_id);
    auto operations = flight_deck_operations_.find(facility_id);
    const auto* carrier = component_manager_->get_component<Carrier>(facility_id);
    const bool stored_on_carrier = carrier && operations != flight_deck_operations_.end() &&
        std::find(
            operations->second.stored_aircraft.begin(),
            operations->second.stored_aircraft.end(),
            aircraft_id
        ) != operations->second.stored_aircraft.end();
    if (!aircraft || !flight_deck || operations == flight_deck_operations_.end() ||
        aircraft->type != Aircraft::Type::CONVENTIONAL || aircraft->status != Aircraft::Status::ON_GROUND ||
        aircraft->fuel < aircraft->max_fuel || aircraft->material < aircraft->max_material ||
        aircraft->ammunition < aircraft->max_ammunition || !is_aircraft_near_facility(*aircraft, facility_id) ||
        (carrier && !stored_on_carrier)) {
        return false;
    }

    operations->second.takeoff_queue.push_back(aircraft_id);
    aircraft->status = Aircraft::Status::QUEUED_FOR_TAKEOFF;
    aircraft->target_base = facility_id;
    update_queue_slots(operations->second);
    sync_carrier_diagnostics(facility_id, operations->second);
    return true;
}

bool LogisticsManager::order_aircraft_return(EntityId aircraft_id, EntityId facility_id) {
    if (!component_manager_ || !compatible_facility(aircraft_id, facility_id)) {
        return false;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    auto* flight_deck = get_flight_deck(facility_id);
    if (!aircraft || !flight_deck || !flight_deck_operations_.contains(facility_id) ||
        aircraft->type != Aircraft::Type::CONVENTIONAL ||
        aircraft->status != Aircraft::Status::AIRBORNE) {
        return false;
    }

    aircraft->status = Aircraft::Status::RETURNING;
    aircraft->mission = Aircraft::Mission::REFUEL;
    aircraft->target_base = facility_id;
    return true;
}

bool LogisticsManager::queue_aircraft_for_landing(EntityId aircraft_id, EntityId facility_id) {
    if (!component_manager_ || !compatible_facility(aircraft_id, facility_id)) {
        return false;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    auto* flight_deck = get_flight_deck(facility_id);
    auto operations = flight_deck_operations_.find(facility_id);
    const bool carrier_has_capacity =
        !component_manager_->get_component<Carrier>(facility_id) ||
        carrier_has_available_recovery_slot(facility_id);
    if (!aircraft || !flight_deck || operations == flight_deck_operations_.end() ||
        aircraft->type != Aircraft::Type::CONVENTIONAL || !flight_deck->runway_usable ||
        !carrier_has_capacity ||
        (aircraft->status != Aircraft::Status::RETURNING &&
         aircraft->status != Aircraft::Status::AIRBORNE) ||
        !is_aircraft_near_facility(*aircraft, facility_id)) {
        return false;
    }

    operations->second.landing_queue.push_back(aircraft_id);
    aircraft->status = Aircraft::Status::QUEUED_FOR_LANDING;
    aircraft->mission = Aircraft::Mission::REFUEL;
    aircraft->target_base = facility_id;
    update_queue_slots(operations->second);
    if (component_manager_->get_component<Carrier>(facility_id)) {
        invalidate_facility_caches();
    }
    return true;
}

bool LogisticsManager::launch_vtol(EntityId aircraft_id) {
    if (!component_manager_) {
        return false;
    }
    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    if (!aircraft || aircraft->type != Aircraft::Type::VTOL ||
        aircraft->status != Aircraft::Status::ON_GROUND ||
        aircraft->fuel < aircraft->max_fuel ||
        aircraft->material < aircraft->max_material ||
        aircraft->ammunition < aircraft->max_ammunition) {
        return false;
    }
    aircraft->status = Aircraft::Status::AIRBORNE;
    aircraft->mission = Aircraft::Mission::PATROL;
    aircraft->target_base = INVALID_ENTITY;
    aircraft->queue_slot = -1;
    return true;
}

bool LogisticsManager::land_vtol(EntityId aircraft_id) {
    if (!component_manager_) {
        return false;
    }
    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    if (!aircraft || aircraft->type != Aircraft::Type::VTOL ||
        (aircraft->status != Aircraft::Status::AIRBORNE &&
         aircraft->status != Aircraft::Status::RETURNING)) {
        return false;
    }
    aircraft->status = Aircraft::Status::ON_GROUND;
    aircraft->mission = Aircraft::Mission::REFUEL;
    aircraft->target_base = INVALID_ENTITY;
    aircraft->closest_recovery_facility = INVALID_ENTITY;
    aircraft->safe_return = true;
    return true;
}

size_t LogisticsManager::takeoff_queue_size(EntityId facility_id) const {
    auto operations = flight_deck_operations_.find(facility_id);
    return operations == flight_deck_operations_.end() ? 0 : operations->second.takeoff_queue.size();
}

size_t LogisticsManager::landing_queue_size(EntityId facility_id) const {
    auto operations = flight_deck_operations_.find(facility_id);
    return operations == flight_deck_operations_.end() ? 0 : operations->second.landing_queue.size();
}

size_t LogisticsManager::active_runway_operations(EntityId facility_id) const {
    auto operations = flight_deck_operations_.find(facility_id);
    return operations == flight_deck_operations_.end() ? 0 : operations->second.active.size();
}

void LogisticsManager::remove_aircraft(EntityId aircraft_id) {
    bool carrier_capacity_changed = false;
    for (auto& [facility_id, operations] : flight_deck_operations_) {
        const size_t previous_reservations = carrier_recovery_reservations(facility_id);
        const size_t previous_occupancy = operations.stored_aircraft.size();
        std::erase(operations.takeoff_queue, aircraft_id);
        std::erase(operations.landing_queue, aircraft_id);
        std::erase_if(
            operations.active,
            [aircraft_id](const RunwayOperation& operation) {
                return operation.aircraft_id == aircraft_id;
            }
        );
        std::erase(operations.recovering, aircraft_id);
        std::erase(operations.stored_aircraft, aircraft_id);
        update_queue_slots(operations);
        sync_carrier_diagnostics(facility_id, operations);
        carrier_capacity_changed = carrier_capacity_changed ||
            component_manager_->get_component<Carrier>(facility_id) &&
            (previous_reservations != carrier_recovery_reservations(facility_id) ||
             previous_occupancy != operations.stored_aircraft.size());
    }
    safe_return_cache_.erase(aircraft_id);
    if (carrier_capacity_changed) {
        invalidate_facility_caches();
    }
}

bool LogisticsManager::is_aircraft_near_facility(const Aircraft& aircraft, EntityId facility_id) const {
    auto* facility = component_manager_->get_component<RecoveryFacility>(facility_id);
    if (!facility) {
        return false;
    }
    const float dx = aircraft.x - facility->x;
    const float dy = aircraft.y - facility->y;
    return dx * dx + dy * dy <= facility->max_recovery_distance * facility->max_recovery_distance;
}

void LogisticsManager::update_queue_slots(FlightDeckOperations& operations) {
    int slot = 0;
    for (EntityId aircraft_id : operations.landing_queue) {
        if (auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id)) {
            aircraft->queue_slot = slot++;
        }
    }
    for (EntityId aircraft_id : operations.takeoff_queue) {
        if (auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id)) {
            aircraft->queue_slot = slot++;
        }
    }
}

void LogisticsManager::update_recovering_aircraft(
    EntityId facility_id,
    FlightDeckOperations& operations,
    float delta_ms) {
    auto* flight_deck = get_flight_deck(facility_id);
    if (!flight_deck) {
        return;
    }

    const float seconds = delta_ms / 1000.0f;
    auto recovered_end = std::remove_if(
        operations.recovering.begin(),
        operations.recovering.end(),
        [&](EntityId aircraft_id) {
            auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
            if (!aircraft || aircraft->status == Aircraft::Status::CRASHED) {
                return true;
            }

            const float fuel_needed = std::max(0.0f, aircraft->max_fuel - aircraft->fuel);
            const float fuel_transfer = std::min({
                fuel_needed,
                static_cast<float>(std::max(0, flight_deck->refuel_rate)) * seconds,
                std::max(0.0f, flight_deck->current_fuel)
            });
            aircraft->fuel += fuel_transfer;
            flight_deck->current_fuel -= fuel_transfer;

            float rearm_budget = static_cast<float>(std::max(0, flight_deck->rearm_rate)) * seconds;
            const float material_needed = std::max(0.0f, aircraft->max_material - aircraft->material);
            const float material_transfer = std::min({
                material_needed,
                rearm_budget,
                std::max(0.0f, flight_deck->current_munitions)
            });
            aircraft->material += material_transfer;
            flight_deck->current_munitions -= material_transfer;
            rearm_budget -= material_transfer;

            const float ammunition_needed = std::max(0.0f, aircraft->max_ammunition - aircraft->ammunition);
            const float ammunition_transfer = std::min({
                ammunition_needed,
                rearm_budget,
                std::max(0.0f, flight_deck->current_munitions)
            });
            aircraft->ammunition += ammunition_transfer;
            flight_deck->current_munitions -= ammunition_transfer;

            const bool serviced = aircraft->fuel >= aircraft->max_fuel &&
                                    aircraft->material >= aircraft->max_material &&
                                    aircraft->ammunition >= aircraft->max_ammunition;
            if (serviced) {
                aircraft->status = Aircraft::Status::ON_GROUND;
                aircraft->queue_slot = -1;
            } else {
                aircraft->status = Aircraft::Status::RECOVERING;
            }
            return serviced;
        }
    );
    operations.recovering.erase(recovered_end, operations.recovering.end());
}

void LogisticsManager::update_flight_deck_operations(float delta_ms) {
    for (auto& [facility_id, operations] : flight_deck_operations_) {
        auto* flight_deck = get_flight_deck(facility_id);
        const bool is_carrier = component_manager_->get_component<Carrier>(facility_id) != nullptr;
        if (!flight_deck) {
            continue;
        }

        update_recovering_aircraft(facility_id, operations, delta_ms);

        for (auto& operation : operations.active) {
            operation.remaining_ms -= delta_ms;
        }
        
        auto completed_end = std::remove_if(
            operations.active.begin(),
            operations.active.end(),
            [&](const RunwayOperation& operation) {
                if (operation.remaining_ms > 0.0f) {
                    return false;
                }
                 auto* aircraft = component_manager_->get_component<Aircraft>(operation.aircraft_id);
                 if (!aircraft || aircraft->status == Aircraft::Status::CRASHED) {
                     return true;
                 }
                 aircraft->queue_slot = -1;
                  if (operation.type == RunwayOperationType::TAKEOFF) {
                      aircraft->status = Aircraft::Status::AIRBORNE;
                     aircraft->mission = Aircraft::Mission::PATROL;
                    aircraft->target_base = INVALID_ENTITY;
                    if (is_carrier) {
                        std::erase(operations.stored_aircraft, operation.aircraft_id);
                        invalidate_facility_caches();
                     }
                  } else {
                      aircraft->status = Aircraft::Status::RECOVERING;
                      aircraft->x = flight_deck->x;
                      aircraft->y = flight_deck->y;
                     if (auto* position = component_manager_->get_component<Position>(operation.aircraft_id)) {
                         position->x = flight_deck->x;
                         position->y = flight_deck->y;
                     }
                     operations.recovering.push_back(operation.aircraft_id);
          if (is_carrier && std::find(
                 operations.stored_aircraft.begin(),
                 operations.stored_aircraft.end(),
                 operation.aircraft_id
             ) == operations.stored_aircraft.end()) {
             operations.stored_aircraft.push_back(operation.aircraft_id);
        }
                }
                return true;
            }
        );
        operations.active.erase(completed_end, operations.active.end());

        if (!flight_deck->runway_usable || flight_deck->runway_capacity <= 0) {
            update_queue_slots(operations);
            sync_carrier_diagnostics(facility_id, operations);
            continue;
        }

        while (operations.active.size() < static_cast<size_t>(flight_deck->runway_capacity)) {
            EntityId aircraft_id = INVALID_ENTITY;
            RunwayOperationType type = RunwayOperationType::TAKEOFF;
            if (!operations.landing_queue.empty()) {
                aircraft_id = operations.landing_queue.front();
                operations.landing_queue.pop_front();
                type = RunwayOperationType::LANDING;
            } else if (!operations.takeoff_queue.empty()) {
                aircraft_id = operations.takeoff_queue.front();
                operations.takeoff_queue.pop_front();
            } else {
                break;
            }

            auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
            if (!aircraft || aircraft->status == Aircraft::Status::CRASHED) {
                continue;
            }
            aircraft->status = type == RunwayOperationType::TAKEOFF
                ? Aircraft::Status::TAKING_OFF
                : Aircraft::Status::LANDING;
            aircraft->queue_slot = -1;
            operations.active.push_back({aircraft_id, type, RUNWAY_OPERATION_MS});
        }
        update_queue_slots(operations);
        sync_carrier_diagnostics(facility_id, operations);
    }
}

void LogisticsManager::add_carrier(EntityId carrier_id, const Carrier& carrier) {
    if (!component_manager_) {
        return;
    }
    
    Carrier registered_carrier = carrier;
    registered_carrier.deck_occupancy = 0;
    registered_carrier.launch_queue_size = 0;
    component_manager_->add_component<Carrier>(carrier_id, registered_carrier);
    
    RecoveryFacility facility;
    facility.x = carrier.x;
    facility.y = carrier.y;
    facility.max_recovery_distance = 300.0f;
    facility.type = RecoveryFacility::Type::CARRIER;
    facility.capabilities = RecoveryFacility::AIR_RECOVERY;
    component_manager_->add_component<RecoveryFacility>(carrier_id, facility);
    flight_deck_operations_[carrier_id] = FlightDeckOperations{};
    index_recovery_facility(carrier_id, facility.type);
    invalidate_facility_caches();
}

void LogisticsManager::add_naval_base(EntityId base_id, float x, float y, float max_recovery_distance) {
    if (!component_manager_) {
        return;
    }
    
    if (auto* facility = component_manager_->get_component<RecoveryFacility>(base_id)) {
        facility->capabilities |= RecoveryFacility::NAVAL_RESUPPLY;
        facility->max_recovery_distance = std::max(facility->max_recovery_distance, max_recovery_distance);
    } else {
        RecoveryFacility naval_facility;
        naval_facility.x = x;
        naval_facility.y = y;
        naval_facility.max_recovery_distance = max_recovery_distance;
        naval_facility.type = RecoveryFacility::Type::NAVAL_BASE;
        naval_facility.capabilities = RecoveryFacility::NAVAL_RESUPPLY;
        component_manager_->add_component<RecoveryFacility>(base_id, naval_facility);
    }
    index_recovery_facility(base_id, RecoveryFacility::Type::NAVAL_BASE);
    invalidate_facility_caches();
}

void LogisticsManager::add_aircraft(EntityId aircraft_id, float x, float y, float fuel) {
    if (!component_manager_) {
        return;
    }
    
    Aircraft aircraft{};
    aircraft.x = x;
    aircraft.y = y;
    aircraft.fuel = fuel;
    aircraft.max_fuel = 100.0f;
    aircraft.fuel_consumption_rate = 0.5f;
    aircraft.material = 20.0f;
    aircraft.max_material = 20.0f;
    aircraft.material_consumption_rate = 0.02f;
    aircraft.ammunition = 10.0f;
    aircraft.max_ammunition = 10.0f;
    aircraft.cruise_speed = 100.0f;
    aircraft.airborne_time_ms = 0.0f;
    aircraft.max_airborne_time_ms = 240000.0f;
    aircraft.range = 200.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    aircraft.mission = Aircraft::Mission::PATROL;
    aircraft.target_base = INVALID_ENTITY;
    aircraft.closest_recovery_facility = INVALID_ENTITY;
    aircraft.predicted_return_energy = std::numeric_limits<float>::infinity();
    aircraft.predicted_return_material = std::numeric_limits<float>::infinity();
    aircraft.predicted_return_time_ms = std::numeric_limits<float>::infinity();
    aircraft.safe_return = false;
    aircraft.queue_slot = -1;
    
    component_manager_->add_component<Aircraft>(aircraft_id, aircraft);
}

void LogisticsManager::add_naval_vessel(EntityId vessel_id, float x, float y, float fuel) {
    if (!component_manager_) {
        return;
    }
    
    NavalVessel vessel{};
    vessel.x = x;
    vessel.y = y;
    vessel.fuel = fuel;
    vessel.max_fuel = 500.0f;
    vessel.fuel_consumption_rate = 2.0f;
    vessel.is_stranded = false;
    
    component_manager_->add_component<NavalVessel>(vessel_id, vessel);
}

void LogisticsManager::resupply_naval_vessel(EntityId vessel_id, float amount) {
    if (!component_manager_) {
        return;
    }
    
    auto* vessel = component_manager_->get_component<NavalVessel>(vessel_id);
    if (!vessel) {
        return;
    }
    
    vessel->fuel = std::min(vessel->max_fuel, vessel->fuel + amount);
    if (vessel->is_stranded && vessel->fuel > 10.0f) {
        vessel->is_stranded = false;
    }
}

// Batch processing for performance
void LogisticsManager::batch_safe_return_check(const std::vector<EntityId>& aircraft_ids, const Pathfinding& pathfinding) {
    for (auto aircraft_id : aircraft_ids) {
        is_safe_return(aircraft_id, pathfinding);
    }
}

void LogisticsManager::update_all(float delta_ms) {
    if (!component_manager_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }
    
    // Update endurance for all aircraft
    auto aircraft_entities = component_manager_->entities_with<Aircraft>();
    for (auto entity_id : aircraft_entities) {
        update_aircraft_endurance(entity_id, delta_ms);
    }

    update_flight_deck_operations(delta_ms);
    
    // Update endurance for all naval vessels
    auto naval_entities = component_manager_->entities_with<NavalVessel>();
    for (auto entity_id : naval_entities) {
        update_naval_endurance(entity_id, delta_ms);
    }
    
    // Update intelligence staleness (10% decay per tick)
    float staleness_rate = 0.1f * (delta_ms / 1000.0f);
    auto intel_entities = component_manager_->entities_with<Intelligence>();
    for (auto entity_id : intel_entities) {
        auto* intel = component_manager_->get_component<Intelligence>(entity_id);
        if (intel && !intel->currently_observed && intel->freshness > 0.0f) {
            intel->freshness = std::max(0.0f, intel->freshness - staleness_rate);
        }
    }
    for (auto& [entity_id, intel] : intelligence_memory_) {
        (void)entity_id;
        if (!intel.currently_observed && intel.freshness > 0.0f) {
            intel.freshness = std::max(0.0f, intel.freshness - staleness_rate);
        }
    }
}

// Verification methods for testing
bool LogisticsManager::verify_safe_return_calculation(EntityId aircraft_id, const Pathfinding& pathfinding, float tolerance) {
    if (!component_manager_) {
        return false;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    if (!aircraft) {
        return false;
    }

    if (aircraft->status != Aircraft::Status::AIRBORNE) {
        return false;
    }

    float current_fuel = aircraft->fuel;
    float est_cost = estimate_return_cost(aircraft_id, pathfinding);

    if (current_fuel == 0.0f) {
        return est_cost == std::numeric_limits<float>::infinity();
    }

    bool safe = current_fuel >= est_cost * (1.0f - tolerance);
    return safe;
}

bool LogisticsManager::verify_crashed_state(EntityId aircraft_id, float expected_fuel_threshold) {
    if (!component_manager_) {
        return false;
    }

    auto* aircraft = component_manager_->get_component<Aircraft>(aircraft_id);
    if (!aircraft) {
        return false;
    }

    bool crashed = aircraft->status == Aircraft::Status::CRASHED;
    bool fuel_check = aircraft->fuel <= expected_fuel_threshold;

    return crashed && fuel_check;
}

bool LogisticsManager::verify_stranded_state(EntityId vessel_id, float expected_fuel_threshold) {
    if (!component_manager_) {
        return false;
    }

    auto* vessel = component_manager_->get_component<NavalVessel>(vessel_id);
    if (!vessel) {
        return false;
    }

    bool stranded = vessel->is_stranded;
    bool fuel_check = vessel->fuel <= expected_fuel_threshold;

    return stranded && fuel_check;
}

bool LogisticsManager::verify_intelligence_staleness(EntityId intel_id, uint32_t current_tick, 
                                                      uint32_t ticks_since_update, float expected_decay_rate) {
    if (!component_manager_) {
        return false;
    }

    auto* intel = component_manager_->get_component<Intelligence>(intel_id);
    if (!intel) {
        auto remembered = intelligence_memory_.find(intel_id);
        intel = remembered == intelligence_memory_.end() ? nullptr : &remembered->second;
    }
    if (!intel) {
        return false;
    }

    uint32_t actual_ticks_since = current_tick - intel->last_seen_tick;
    if (actual_ticks_since < ticks_since_update) {
        return false;
    }

    float expected_freshness = std::max(0.0f, 1.0f - (actual_ticks_since * expected_decay_rate));
    float actual_freshness = intel->freshness;

    return std::abs(actual_freshness - expected_freshness) < 0.01f;
}

int LogisticsManager::count_crashed_aircraft() {
    if (!component_manager_) {
        return 0;
    }

    int count = 0;
    auto aircraft_entities = component_manager_->entities_with<Aircraft>();
    for (auto entity_id : aircraft_entities) {
        auto* aircraft = component_manager_->get_component<Aircraft>(entity_id);
        if (aircraft && aircraft->status == Aircraft::Status::CRASHED) {
            count++;
        }
    }

    return count;
}

int LogisticsManager::count_stranded_naval() {
    if (!component_manager_) {
        return 0;
    }

    int count = 0;
    auto naval_entities = component_manager_->entities_with<NavalVessel>();
    for (auto entity_id : naval_entities) {
        auto* vessel = component_manager_->get_component<NavalVessel>(entity_id);
        if (vessel && vessel->is_stranded) {
            count++;
        }
    }

    return count;
}

void LogisticsManager::reset() {
    if (!component_manager_) {
        return;
    }

    auto aircraft_entities = component_manager_->entities_with<Aircraft>();
    for (auto entity_id : aircraft_entities) {
        component_manager_->remove_component<Aircraft>(entity_id);
    }

    auto naval_entities = component_manager_->entities_with<NavalVessel>();
    for (auto entity_id : naval_entities) {
        component_manager_->remove_component<NavalVessel>(entity_id);
    }

    auto intel_entities = component_manager_->entities_with<Intelligence>();
    for (auto entity_id : intel_entities) {
        component_manager_->remove_component<Intelligence>(entity_id);
    }
    intelligence_memory_.clear();

    auto airbase_entities = component_manager_->entities_with<Airbase>();
    for (auto entity_id : airbase_entities) {
        component_manager_->remove_component<Airbase>(entity_id);
    }

    auto carrier_entities = component_manager_->entities_with<Carrier>();
    for (auto entity_id : carrier_entities) {
        component_manager_->remove_component<Carrier>(entity_id);
    }

    auto recovery_entities = component_manager_->entities_with<RecoveryFacility>();
    for (auto entity_id : recovery_entities) {
        component_manager_->remove_component<RecoveryFacility>(entity_id);
    }

    crashed_aircraft_count_ = 0;
    stranded_naval_count_ = 0;
    flight_deck_operations_.clear();
    recovery_facilities_by_type_.clear();
    facility_lookup_cache_.clear();
    safe_return_cache_.clear();
    facility_revision_ = 0;
    safe_return_cache_hits_ = 0;
    safe_return_cache_misses_ = 0;
    facility_lookup_cache_hits_ = 0;
    facility_lookup_cache_misses_ = 0;
}

} // namespace rts
