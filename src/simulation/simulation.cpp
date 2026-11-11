#include <set>
#include "combat/air_domain.hpp"
#include <algorithm>
#include <chrono>
#include <numbers>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <iostream>

#include "simulation/simulation.hpp"
#include "simulation/movement_tuning.hpp"
#include "network/types.hpp"
#include "network/buffer.hpp"
#include "network/network_manager.hpp"
#include "network/serializer.hpp"
#include "economy_api.h"
#include "ecs/components/factions.hpp"
#include "ecs/components/faction.hpp"
#include "ecs/components/weapon.hpp"
#include "ecs/components/resources.hpp"

namespace rts {

namespace {

constexpr float MAX_CATCH_UP_MS = 250.0f;

} // namespace

static Simulation* get_simulation() {
    static Simulation sim;
    return &sim;
}

Simulation* runtime_simulation() { return get_simulation(); }

Simulation::Simulation() {
    combat_manager_.set_event_sink([this](CombatEventKind kind, EntityId entity, float vx, float vy) {
        record_combat_event(kind, entity, vx, vy);
    });
    logistics_manager_.set_component_manager(&component_manager_);
    network_manager_.set_simulation(this);
    ai_manager_ = std::make_unique<AIManager>();
    ai_manager_->set_simulation(this);
}

void Simulation::start() {
    // Reset simulation state for fresh world
    clear_entities();
    combat_events_.clear();
    for (auto& events : radio_events_) events.clear();
    air_threats_ = {};
    running_ = true;
    tick_ = 0;
    elapsed_ms_ = 0.0f;
    game_speed_ = 1.0f;
    last_tick_ms_ = 0.0f;
    
    // Reset subsystems
    pathfinding_.clear_cache();
    logistics_manager_.reset();
    combat_manager_.reset();
    production_manager_.reset();
    network_manager_.reset();
    ai_manager_->reset();
    command_manager_.clear();
    command_log_.clear();
    ai_enabled_ = true;
    ai_manager_->restore_default_doctrine();
    match_record_.clear();
    playback_.clear();
    playback_cursor_ = 0;
    territorial_control_.reset();
    control_point_battle_.reset();
    reinforcement_delivery_.reset();
    forces_by_transport_ = false;
    ground_fuel_burn_ = false;
    fuel_starved_.fill(0);
    has_landing_zone_.fill(false);
    inbound_forces_.clear();
    transport_routes_ = {};
    next_transport_order_id_ = 1;
    paradrop_sequence_ = 0;
    paradrops_lost_.fill(0);
    road_network_.reset();
    power_grid_.reset();
    power_tasks_.clear();
    power_notices_.fill({});
    site_claims_.clear();
    fuel_captures_.clear();
    site_captures_.clear();
    fuel_shipments_.clear();
    fuel_supply_reasons_.fill({});
    match_player_post_ = match_enemy_post_ = INVALID_ENTITY;
    match_result_ = -1;
    match_victory_reason_ = 0;
    key_site_victory_sites_.clear();
    key_sites_required_ = 0;
    key_site_hold_ticks_required_ = 0;
    key_sites_held_.fill(0);
    key_site_hold_ticks_.fill(0);
    vision_sources_.clear();
    vision_active_ = false;
    intel_map_.clear();
    for (auto& contacts : radar_contacts_) contacts.clear();
    recon_flights_.clear();
    for (auto& log : sightings_) log.clear();
    pathfinding_.clear_traversal_costs();
    naval_pathfinding_.clear_traversal_costs();
}

void Simulation::stop() {
    running_ = false;
}

void Simulation::reset() {
    // Reset everything for fresh world
    if (running_) {
        stop();
    }
    start();
}

void Simulation::clear_entities() {
    move_targets_.clear();
    queued_moves_.clear();
    patrol_orders_.clear();
    ferry_flights_.clear();
    air_orbits_.clear();
    component_manager_.cleanup_all();
    spatial_grid_.clear();
    entity_manager_.clear();
    unit_counts_.fill(0);
}

void Simulation::refresh_unit_counts() {
    unit_counts_.fill(0);
    for (const auto entity : component_manager_.entities_with<Faction, UnitData>(entity_manager_.get_entities())) {
        const auto index = static_cast<size_t>(component_manager_.get_component<Faction>(entity)->faction_id);
        const auto* health = component_manager_.get_component<Health>(entity);
        if (index < FACTION_COUNT && (!health || !health->is_dead)) ++unit_counts_[index];
    }
}

int Simulation::unit_count(FactionId faction_id) const {
    const auto index = static_cast<size_t>(faction_id);
    return index < FACTION_COUNT ? unit_counts_[index] : 0;
}

bool Simulation::set_game_speed(float scale) {
    if (network_manager_.is_multiplayer_session()) {
        game_speed_ = 1.0f;
        return false;
    }
    if (!std::isfinite(scale) || scale < 0.25f || scale > 1.75f) return false;
    game_speed_ = scale;
    return true;
}

void Simulation::update(float delta_ms) {
    if (!running_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }

    if (network_manager_.is_multiplayer_session()) game_speed_ = 1.0f;
    // Clamp incoming wall time, then scale it. Preserve the fractional tick:
    // capping the scaled accumulator discards time at 25/175% on long frames.
    elapsed_ms_ += std::min(delta_ms, MAX_CATCH_UP_MS) * game_speed_;
    
    while (elapsed_ms_ >= 50.0f) {
        const auto tick_start = std::chrono::steady_clock::now();
        tick_++;
        network_manager_.receive_packets();
        feed_playback();
        process_network_commands();
        process_commands();
        prediction_phase(50.0f);
        combat_phase(50.0f);
        environment_phase(50.0f);
        logistics_phase(50.0f);
        economy_phase(50.0f);
        if (ai_enabled_) ai_manager_->update(50.0f);
        record_match_tick();
        network_manager_.update(50.0f);
        last_tick_ms_ = static_cast<float>(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tick_start).count()
        );
        elapsed_ms_ -= 50.0f;
        
        write_replay_portable_snapshot();
    }
}

SimulationState Simulation::get_state() const {
    SimulationState state;
    state.tick_number = tick_;
    state.timestamp_ms = elapsed_ms_;
    
    const auto& entities = entity_manager_.get_entities();
    
    std::vector<EntityId> temp_entities;
    std::vector<float> temp_pos_x, temp_pos_y;
    std::vector<float> temp_vel_x, temp_vel_y;
    std::vector<float> temp_health_curr, temp_health_max;
    std::vector<uint8_t> temp_is_dead;
    
    temp_entities.reserve(entities.size());
    temp_pos_x.reserve(entities.size());
    temp_pos_y.reserve(entities.size());
    temp_vel_x.reserve(entities.size());
    temp_vel_y.reserve(entities.size());
    temp_health_curr.reserve(entities.size());
    temp_health_max.reserve(entities.size());
    temp_is_dead.reserve(entities.size());
    
    for (EntityId eid : entities) {
        auto* pos = component_manager_.get_component<Position>(eid);
        auto* vel = component_manager_.get_component<Velocity>(eid);
        auto* hp = component_manager_.get_component<Health>(eid);
        
        if (pos) {
            temp_entities.push_back(eid);
            temp_pos_x.push_back(pos->x);
            temp_pos_y.push_back(pos->y);
            
            if (vel) {
                temp_vel_x.push_back(vel->x);
                temp_vel_y.push_back(vel->y);
            } else {
                temp_vel_x.push_back(0.0f);
                temp_vel_y.push_back(0.0f);
            }
            
            if (hp) {
                temp_health_curr.push_back(hp->current);
                temp_health_max.push_back(hp->max);
                temp_is_dead.push_back(hp->is_dead ? 1 : 0);
            } else {
                temp_health_curr.push_back(0.0f);
                temp_health_max.push_back(0.0f);
                temp_is_dead.push_back(0);
            }
        }
    }
    
    std::vector<size_t> indices(temp_entities.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }
    
    std::sort(indices.begin(), indices.end(), 
        [&temp_entities](size_t a, size_t b) {
            return temp_entities[a] < temp_entities[b];
        });
    
    state.entity_ids.reserve(indices.size());
    state.positions_x.reserve(indices.size());
    state.positions_y.reserve(indices.size());
    state.velocities_x.reserve(indices.size());
    state.velocities_y.reserve(indices.size());
    state.health_current.reserve(indices.size());
    state.health_max.reserve(indices.size());
    state.is_dead.reserve(indices.size());
    
    for (size_t idx : indices) {
        state.entity_ids.push_back(temp_entities[idx]);
        state.positions_x.push_back(temp_pos_x[idx]);
        state.positions_y.push_back(temp_pos_y[idx]);
        state.velocities_x.push_back(temp_vel_x[idx]);
        state.velocities_y.push_back(temp_vel_y[idx]);
        state.health_current.push_back(temp_health_curr[idx]);
        state.health_max.push_back(temp_health_max[idx]);
        state.is_dead.push_back(temp_is_dead[idx]);
    }
    
    return state;
}

void Simulation::write_replay_portable_snapshot() {
    if (replay_writer_.is_open()) {
        auto state = get_state();
        
        if (state.entity_ids.empty()) {
            return;
        }
        
        std::vector<uint8_t> buffer(4000032);
        size_t buffer_size = buffer.size();
        size_t encoded_size = 0;
        
        SnapshotError error = encode_portable_snapshot(
            state.entity_ids.data(),
            state.positions_x.data(),
            state.positions_y.data(),
            nullptr,
            state.velocities_x.data(),
            state.velocities_y.data(),
            nullptr,
            state.health_current.data(),
            state.health_max.data(),
            state.is_dead.data(),
            static_cast<uint32_t>(state.entity_ids.size()),
            tick_,
            buffer.data(),
            buffer_size,
            &encoded_size
        );
        
        if (error == SnapshotError::OK) {
            std::cerr << "write_replay_portable_snapshot: encoded_size = " << encoded_size << "\n";
            replay_writer_.write_snapshot(buffer.data(), encoded_size);
        } else {
            std::cerr << "write_replay_portable_snapshot: encode failed with error " << static_cast<int>(error) << "\n";
        }
    }
}

Entity Simulation::create_unit(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return Entity{};
    }
    if (theater_water_rules_enabled_ && !is_land_position(x, y)) return Entity{};

    Entity entity = entity_manager_.create_entity();
    logistics_manager_.clear_intelligence_memory(entity.id);
    
    Position pos = {x, y, 0.0f};
    Velocity vel = {0.0f, 0.0f, 0.0f};
    Health hp = {100.0f, 100.0f};
    
    Material mat = {100.0f, 1000.0f, 0.0f};
    
    Faction faction = {FactionId::ELITE_PRECISION};
    
    Weapon wep = {10.0f, 100.0f, 1.0f, 0.0f, 200.0f, 0.5f, 0.0f, 0.0f};
    
    component_manager_.add_component(entity.id, pos);
    component_manager_.add_component(entity.id, vel);
    component_manager_.add_component(entity.id, hp);
    component_manager_.add_component(entity.id, mat);
    component_manager_.add_component(entity.id, faction);
    component_manager_.add_component(entity.id, wep);
    
    spatial_grid_.insert(entity.id, x, y);
    
    return entity;
}

const Pathfinding& Simulation::navigation_for(EntityId entity) const {
    if (component_manager_.get_component<Aircraft>(entity)) return air_pathfinding_;
    if (component_manager_.get_component<NavalVessel>(entity)) return naval_pathfinding_;
    return pathfinding_;
}
Pathfinding& Simulation::navigation_for(EntityId entity) {
    return const_cast<Pathfinding&>(std::as_const(*this).navigation_for(entity));
}

void Simulation::move_unit(EntityId entity, float x, float y) {
    move_unit_with_route(entity, x, y, x, y);
}

void Simulation::move_unit_with_route(EntityId entity, float x, float y, float route_x, float route_y) {
    queued_moves_.erase(entity);
    assign_move_target(entity, x, y, route_x, route_y);
}

bool Simulation::queue_unit_move_with_route(EntityId entity, float x, float y, float route_x, float route_y) {
    if (move_targets_.find(entity) == move_targets_.end()) {
        queued_moves_.erase(entity);
        return assign_move_target(entity, x, y, route_x, route_y);
    }
    const auto& navigation = navigation_for(entity);
    const auto* position = component_manager_.get_component<Position>(entity);
    auto& queue = queued_moves_[entity];
    if (!position || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(route_x) || !std::isfinite(route_y) ||
        queue.size() + 1 >= MAX_MOVE_WAYPOINTS ||
        !navigation.is_walkable(navigation.to_grid_x(x), navigation.to_grid_y(y)) ||
        !navigation.is_walkable(navigation.to_grid_x(route_x), navigation.to_grid_y(route_y))) {
        if (queue.empty()) queued_moves_.erase(entity);
        return false;
    }
    queue.push_back({{x, y, position->z}, {route_x, route_y, position->z}});
    return true;
}

std::vector<std::pair<float, float>> Simulation::get_unit_waypoints(EntityId entity) const {
    std::vector<std::pair<float, float>> waypoints;
    const auto target = move_targets_.find(entity);
    if (target == move_targets_.end()) return waypoints;
    waypoints.emplace_back(target->second.arrival.x, target->second.arrival.y);
    if (const auto queue = queued_moves_.find(entity); queue != queued_moves_.end()) {
        for (const auto& waypoint : queue->second) waypoints.emplace_back(waypoint.arrival.x, waypoint.arrival.y);
    }
    return waypoints;
}

std::size_t Simulation::queued_waypoint_count(EntityId entity) const {
    const auto queue = queued_moves_.find(entity);
    return queue == queued_moves_.end() ? 0 : queue->second.size();
}

bool Simulation::assign_move_target(EntityId entity, float x, float y, float route_x, float route_y) {
    auto& navigation = navigation_for(entity);
    auto* position = component_manager_.get_component<Position>(entity);
    auto* velocity = component_manager_.get_component<Velocity>(entity);
    if (!position || !velocity || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(route_x) || !std::isfinite(route_y) ||
        !navigation.is_walkable(navigation.to_grid_x(x), navigation.to_grid_y(y)) ||
        !navigation.is_walkable(navigation.to_grid_x(route_x), navigation.to_grid_y(route_y))) {
        return false;
    }

    if(auto* aircraft=component_manager_.get_component<Aircraft>(entity);
       aircraft && aircraft->status==Aircraft::Status::ON_GROUND) {
        if(aircraft->type==Aircraft::Type::VTOL) logistics_manager_.launch_vtol(entity);
        else logistics_manager_.queue_aircraft_for_takeoff(entity,
            logistics_manager_.find_nearest_aircraft_recovery_facility(aircraft->x,aircraft->y,entity));
    }
    if (auto* aircraft = component_manager_.get_component<Aircraft>(entity);
        aircraft && aircraft->status == Aircraft::Status::ON_GROUND && aircraft->type == Aircraft::Type::CONVENTIONAL) {
        // Parked on a strategic airfield: take off straight away.
        const auto* owner = component_manager_.get_component<Faction>(entity);
        float ax = 0.0f, ay = 0.0f;
        if (owner && nearest_friendly_airfield(owner->faction_id, position->x, position->y, ax, ay) &&
            std::hypot(ax - position->x, ay - position->y) <= 300.0f &&
            aircraft->fuel > 0.0f) {
            aircraft->status = Aircraft::Status::AIRBORNE;
            aircraft->mission = Aircraft::Mission::PATROL;
        }
    }
    if (const auto ferry = ferry_flights_.find(entity); ferry != ferry_flights_.end() && ferry->second.landed)
        ferry_flights_.erase(ferry);
    air_orbits_.erase(entity);
    move_targets_[entity] = {
        {x, y, position->z},
        {route_x, route_y, position->z}
    };
    // Any ordinary order ends a yield; the yield itself sets the flag after.
    if (auto* steering = component_manager_.get_component<GroundSteering>(entity)) steering->yielding = false;
    return true;
}

void Simulation::move_units_formation(
    const std::vector<EntityId>& entities,
    float center_x,
    float center_y,
    float spacing,
    bool queued) {
    if (entities.empty() || !std::isfinite(center_x) || !std::isfinite(center_y) ||
        !std::isfinite(spacing) || spacing < movement::kMinimumFormationSpacingMeters) {
        return;
    }

    const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(entities.size()))));
    const int rows = static_cast<int>((entities.size() + static_cast<std::size_t>(columns) - 1) /
                                      static_cast<std::size_t>(columns));
    float effective_spacing = spacing;
    // Slots closer than a hull length would only be fought over by unit
    // separation, so ground formation slots leave a gap between hulls.
    if (entities.size() > 1) {
        for (const auto entity : entities) {
            if (const auto* steering = component_manager_.get_component<GroundSteering>(entity)) {
                effective_spacing = std::max(effective_spacing,
                    2.0f * steering->footprint_radius * movement::kFormationHullSpacing);
            }
        }
    }
    if (columns > 1) {
        effective_spacing = std::min(
            effective_spacing,
            (pathfinding_.max_world_x_center() - pathfinding_.min_world_x_center()) /
                static_cast<float>(columns - 1)
        );
    }
    if (rows > 1) {
        effective_spacing = std::min(
            effective_spacing,
            (pathfinding_.max_world_y_center() - pathfinding_.min_world_y_center()) /
                static_cast<float>(rows - 1)
        );
    }

    const float half_width = static_cast<float>(columns - 1) * effective_spacing * 0.5f;
    const float half_height = static_cast<float>(rows - 1) * effective_spacing * 0.5f;
    const float route_x = std::clamp(
        center_x,
        pathfinding_.min_world_x_center() + half_width,
        pathfinding_.max_world_x_center() - half_width
    );
    const float route_y = std::clamp(
        center_y,
        pathfinding_.min_world_y_center() + half_height,
        pathfinding_.max_world_y_center() - half_height
    );

    for (std::size_t index = 0; index < entities.size(); ++index) {
        const int row = static_cast<int>(index) / columns;
        const int column = static_cast<int>(index) % columns;
        const float arrival_x = route_x + static_cast<float>(column) * effective_spacing - half_width;
        const float arrival_y = route_y + static_cast<float>(row) * effective_spacing - half_height;
        if (queued) queue_unit_move_with_route(entities[index], arrival_x, arrival_y, route_x, route_y);
        else move_unit_with_route(entities[index], arrival_x, arrival_y, route_x, route_y);
    }
}

void Simulation::stop_unit(EntityId entity) {
    patrol_orders_.erase(entity);
    queued_moves_.erase(entity);
    combat_manager_.unregister_entity(entity);
    auto* vel = component_manager_.get_component<Velocity>(entity);
    if (vel) {
        vel->x = 0.0f;
        vel->y = 0.0f;
        vel->z = 0.0f;
    }
    move_targets_.erase(entity);
}

void Simulation::attack_unit(EntityId entity, EntityId target_id) {
    combat_manager_.start_attack(entity, target_id, component_manager_);
}

void Simulation::patrol_unit(EntityId entity, float x, float y) {
    auto* position = component_manager_.get_component<Position>(entity);
    if (!position) return;
    stop_unit(entity);
    patrol_orders_[entity] = {*position, {x, y, position->z}, false};
    move_unit(entity, x, y);
}

void Simulation::return_unit(EntityId entity) {
    const auto* owner = component_manager_.get_component<Faction>(entity);
    if (!owner) return;
    const auto base = production_manager_.faction_line(owner->faction_id);
    if (base == INVALID_ENTITY) return;
    
    const auto* base_pos = component_manager_.get_component<Position>(base);
    if (!base_pos) return;
    
    if (auto* vessel = component_manager_.get_component<NavalVessel>(entity)) {
        const EntityId naval_base = logistics_manager_.find_nearest_recovery_facility(
            vessel->x, vessel->y, RecoveryFacility::Type::NAVAL_BASE, entity
        );
        const auto* facility = component_manager_.get_component<RecoveryFacility>(naval_base);
        if (!facility) return;
        const float dx = vessel->x - facility->x;
        const float dy = vessel->y - facility->y;
        if (dx * dx + dy * dy > facility->max_recovery_distance * facility->max_recovery_distance) return;
        auto funds = production_manager_.storages().find(base);
        const float needed = std::max(0.0f, vessel->max_fuel - vessel->fuel);
        const float material_cost = needed * 0.1f;
        if (funds != production_manager_.storages().end() && funds->second.energy_storage >= needed &&
            funds->second.metal_storage >= material_cost) {
            funds->second.energy_storage -= needed;
            funds->second.metal_storage -= material_cost;
            // TEMPORARY Phase-2 bridge: naval recovery still consumes its
            // legacy logistics budget, so preserve the canonical Materials
            // snapshot until the logistics fuel migration removes this path.
            funds->second.tactical.materials.current = funds->second.metal_storage;
            logistics_manager_.resupply_naval_vessel(entity, needed);
        }
        stop_unit(entity);
        return;
    }

    if (auto* aircraft = component_manager_.get_component<Aircraft>(entity)) {
        auto estimate = logistics_manager_.estimate_safe_return(entity, pathfinding_);
        if (estimate.facility_id != INVALID_ENTITY) {
            stop_unit(entity);
            logistics_manager_.order_aircraft_return(entity, estimate.facility_id);
        } else {
            begin_airfield_return(entity);
        }
        return;
    }
    
    const float return_radius = 80.0f;
    const float dx = get_unit_x(entity) - base_pos->x;
    const float dy = get_unit_y(entity) - base_pos->y;
    const float dist_sq = dx * dx + dy * dy;
    
    if (dist_sq <= return_radius * return_radius) {
        stop_unit(entity);
    } else {
        move_unit(entity, base_pos->x, base_pos->y);
    }
}

void Simulation::build_structure(EntityId entity, float x, float y, UnitType unit_type) {
    auto* faction = component_manager_.get_component<Faction>(entity);
    if (faction) order_unit(faction->faction_id, unit_type, x, y);
}

bool Simulation::order_unit(FactionId faction, UnitType type, float x, float y) {
    if (needs_delivery(type)) return order_delivery(faction, type);
    const EntityId line = production_manager_.faction_line(faction);
    return line != INVALID_ENTITY && production_manager_.queue_unit(line, faction, type, x, y);
}

bool Simulation::needs_delivery(UnitType type) const {
    if (!forces_by_transport_) return false;
    const auto proto = get_unit_prototypes().find(type);
    return proto != get_unit_prototypes().end() && !proto->second.is_aircraft && !proto->second.is_naval;
}

int Simulation::inbound_count(FactionId faction) const {
    int count = 0;
    for (const auto& force : inbound_forces_) count += force.faction == faction ? 1 : 0;
    return count;
}

bool Simulation::set_landing_zone(FactionId faction, float x, float y) {
    const auto index = static_cast<size_t>(faction);
    if (index >= FACTION_COUNT || !std::isfinite(x) || !std::isfinite(y) ||
        !pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y)) || !is_land_position(x, y)) {
        return false;
    }
    has_landing_zone_[index] = true;
    landing_zones_[index] = {x, y};
    return true;
}

bool Simulation::landing_site(FactionId faction, float& x, float& y) const {
    float airfield_x = 0.0f;
    float airfield_y = 0.0f;
    if (territorial_control_.get_active_installation_position(faction, InstallationType::AIRFIELD, airfield_x, airfield_y)) {
        // Land just clear of the airfield's runway: beyond either end of its
        // 750 m site first, then beside it, taking the nearest ground that a
        // unit can actually stand on.
        const float distances[] = {460.0f, 520.0f, 600.0f, 700.0f, 850.0f, 1000.0f};
        for (const float distance : distances) {
            for (const float direction : {1.0f, -1.0f}) {
                const float site_y = airfield_y + direction * distance;
                if (pathfinding_.is_walkable(pathfinding_.to_grid_x(airfield_x), pathfinding_.to_grid_y(site_y)) &&
                    is_land_position(airfield_x, site_y)) {
                    x = airfield_x;
                    y = site_y;
                    return true;
                }
            }
        }
        for (const float distance : {210.0f, 300.0f, 420.0f}) {
            for (const float direction : {1.0f, -1.0f}) {
                const float site_x = airfield_x + direction * distance;
                if (pathfinding_.is_walkable(pathfinding_.to_grid_x(site_x), pathfinding_.to_grid_y(airfield_y)) &&
                    is_land_position(site_x, airfield_y)) {
                    x = site_x;
                    y = airfield_y;
                    return true;
                }
            }
        }
        // An airfield with no open ground around it cannot receive forces.
        return false;
    }
    const auto index = static_cast<size_t>(faction);
    if (index < FACTION_COUNT && has_landing_zone_[index]) {
        x = landing_zones_[index][0];
        y = landing_zones_[index][1];
        return true;
    }
    return false;
}

std::string Simulation::delivery_refusal(FactionId faction, UnitType type) const {
    const auto proto = get_unit_prototypes().find(type);
    if (proto == get_unit_prototypes().end() || proto->second.faction != faction) return "UNKNOWN UNIT";
    float x = 0.0f;
    float y = 0.0f;
    if (!landing_site(faction, x, y)) return "AIRFIELD REQUIRED FOR REINFORCEMENTS";
    if (inbound_count(faction) >= kMaxInboundForces) return "TRANSPORT QUEUE FULL";
    if (unit_count(faction) + inbound_count(faction) >= unit_cap_per_player_) return "UNIT CAP REACHED";
    const EntityId line = production_manager_.faction_line(faction);
    const auto line_it = production_manager_.production_lines().find(line);
    if (line_it == production_manager_.production_lines().end()) return "NO PRODUCTION LINE";
    const auto storage = production_manager_.storages().find(line_it->second.storage_id);
    const float cost = production_manager_.get_unit_metal_cost(faction, type);
    if (storage == production_manager_.storages().end() || storage->second.tactical.materials.current < cost) {
        return "INSUFFICIENT MATERIALS";
    }
    return {};
}

bool Simulation::order_delivery(FactionId faction, UnitType type) {
    if (!delivery_refusal(faction, type).empty()) return false;
    const auto& proto = get_unit_prototypes().at(type);
    const float cost = production_manager_.get_unit_metal_cost(faction, type);
    if (!production_manager_.deduct_faction_resources(faction, cost, 0.0f)) return false;
    const float seconds = proto.build_time_seconds + kTransitSeconds;
    inbound_forces_.push_back({faction, type, tick_ + static_cast<uint32_t>(std::ceil(seconds * 20.0f))});
    return true;
}

bool Simulation::configure_transport_route(FactionId faction, GroundTransport mode,
                                            float entry_x, float entry_y, float terminal_x, float terminal_y) {
    const size_t side = static_cast<size_t>(faction);
    const int route = static_cast<int>(mode) - 1;
    if (side >= FACTION_COUNT || route < 0 || route >= 2 ||
        !std::isfinite(entry_x) || !std::isfinite(entry_y) ||
        !std::isfinite(terminal_x) || !std::isfinite(terminal_y)) return false;
    const float length = std::hypot(terminal_x - entry_x, terminal_y - entry_y);
    if (length < 100.0f || length > 20000.0f) return false;
    for (int sample = 0, count = std::max(1, static_cast<int>(std::ceil(length / 100.0f))); sample <= count; ++sample) {
        const float fraction = static_cast<float>(sample) / static_cast<float>(count);
        if (!is_land_position(entry_x + (terminal_x - entry_x) * fraction,
                              entry_y + (terminal_y - entry_y) * fraction)) return false;
    }
    transport_routes_[side][static_cast<size_t>(route)] = {true, entry_x, entry_y, terminal_x, terminal_y};
    if (mode == GroundTransport::ROAD) {
        RoadSegment paved;
        paved.owner = faction;
        paved.start_x = entry_x;
        paved.start_y = entry_y;
        paved.end_x = terminal_x;
        paved.end_y = terminal_y;
        paved.width = 12.0f;
        paved.completed = true;
        apply_completed_road(paved);
    }
    return true;
}

Simulation::TransportRoute Simulation::transport_route(FactionId faction, GroundTransport mode) const {
    const size_t side = static_cast<size_t>(faction);
    const int route = static_cast<int>(mode) - 1;
    if (side >= FACTION_COUNT || route < 0 || route >= 2) return {};
    return transport_routes_[side][static_cast<size_t>(route)];
}

std::string Simulation::delivery_refusal_via(FactionId faction, UnitType type, GroundTransport mode) const {
    if (mode == GroundTransport::AIR) return delivery_refusal(faction, type);
    const auto proto = get_unit_prototypes().find(type);
    if (!forces_by_transport_ || proto == get_unit_prototypes().end() || proto->second.faction != faction ||
        !needs_delivery(type)) return "UNKNOWN GROUND FORCE";
    if (!transport_route(faction, mode).enabled) return mode == GroundTransport::RAIL ? "RAIL ROUTE UNAVAILABLE" : "ROAD ROUTE UNAVAILABLE";
    if (inbound_count(faction) >= kMaxInboundForces) return "TRANSPORT QUEUE FULL";
    if (unit_count(faction) + inbound_count(faction) >= unit_cap_per_player_) return "UNIT CAP REACHED";
    const EntityId line = production_manager_.faction_line(faction);
    const auto line_it = production_manager_.production_lines().find(line);
    if (line_it == production_manager_.production_lines().end()) return "NO PRODUCTION LINE";
    const auto storage = production_manager_.storages().find(line_it->second.storage_id);
    const float cost = production_manager_.get_unit_metal_cost(faction, type);
    if (storage == production_manager_.storages().end() || storage->second.tactical.materials.current < cost)
        return "INSUFFICIENT MATERIALS";
    return {};
}

bool Simulation::order_delivery_via(FactionId faction, UnitType type, GroundTransport mode) {
    if (mode == GroundTransport::AIR) return order_delivery(faction, type);
    if (!delivery_refusal_via(faction, type, mode).empty()) return false;
    const float cost = production_manager_.get_unit_metal_cost(faction, type);
    if (!production_manager_.deduct_faction_resources(faction, cost, 0.0f)) return false;
    const auto route = transport_route(faction, mode);
    const float length = std::hypot(route.terminal_x - route.entry_x, route.terminal_y - route.entry_y);
    const float speed = mode == GroundTransport::RAIL ? 45.0f : 25.0f;
    int queued_on_route = 0;
    for (const auto& inbound : inbound_forces_)
        if (inbound.faction == faction && inbound.route_mode == static_cast<uint8_t>(mode)) ++queued_on_route;
    const float departure_seconds = get_unit_prototypes().at(type).build_time_seconds + queued_on_route * 8.0f;
    const float seconds = departure_seconds + length / speed;
    InboundForce force{faction, type, tick_ + static_cast<uint32_t>(std::ceil(seconds * 20.0f))};
    force.route_mode = static_cast<uint8_t>(mode);
    force.order_id = next_transport_order_id_++;
    force.launch_tick = tick_ + static_cast<uint32_t>(std::ceil(departure_seconds * 20.0f));
    inbound_forces_.push_back(force);
    return true;
}

size_t Simulation::issue_ground_transport_command(FactionId faction, UnitType type, GroundTransport mode) {
    if (mode != GroundTransport::ROAD && mode != GroundTransport::RAIL) return 0;
    if (!delivery_refusal_via(faction, type, mode).empty()) return 0;
    const EntityId post = production_manager_.faction_line(faction);
    if (post == INVALID_ENTITY) return 0;
    const uint32_t flag = mode == GroundTransport::ROAD ? ROAD_DELIVERY_ORDER_FLAG : RAIL_DELIVERY_ORDER_FLAG;
    return issue_commands({post}, faction, CommandType::BUILD, 0.0f, 0.0f, flag | static_cast<uint32_t>(type));
}

std::string Simulation::paradrop_refusal(FactionId faction, UnitType type, float x, float y) const {
    const auto proto = get_unit_prototypes().find(type);
    if (proto == get_unit_prototypes().end() || proto->second.faction != faction) return "UNKNOWN UNIT";
    if (!forces_by_transport_ || !needs_delivery(type) || !proto->second.airdrop) return "CANNOT BE AIRDROPPED";
    if (!cargo_aircraft_type(faction)) return "NO CARGO AIRCRAFT";
    if (!std::isfinite(x) || !std::isfinite(y) || !is_land_position(x, y) ||
        !pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y))) return "NO DROP ZONE THERE";
    // Nobody jumps blind: the drop zone must be watched when the drop is ordered.
    if (!can_see(faction, x, y)) return "DROP ZONE NOT SCOUTED";
    if (inbound_count(faction) >= kMaxInboundForces) return "TRANSPORT QUEUE FULL";
    if (unit_count(faction) + inbound_count(faction) >= unit_cap_per_player_) return "UNIT CAP REACHED";
    const EntityId line = production_manager_.faction_line(faction);
    const auto line_it = production_manager_.production_lines().find(line);
    if (line_it == production_manager_.production_lines().end()) return "NO PRODUCTION LINE";
    const auto storage = production_manager_.storages().find(line_it->second.storage_id);
    const float cost = production_manager_.get_unit_metal_cost(faction, type);
    if (storage == production_manager_.storages().end() || storage->second.tactical.materials.current < cost) {
        return "INSUFFICIENT MATERIALS";
    }
    return {};
}

float Simulation::paradrop_seconds(UnitType type, float x) const {
    const auto proto = get_unit_prototypes().find(type);
    if (proto == get_unit_prototypes().end()) return 0.0f;
    const auto cargo = cargo_aircraft_type(proto->second.faction);
    const float speed = cargo ? get_unit_prototypes().at(*cargo).speed : 90.0f;
    const float entry_x = pathfinding_.min_world_x_center() - pathfinding_.cell_size();
    const float flight = std::max(0.0f, x - entry_x - kParadropReleaseRadius) / speed;
    return proto->second.build_time_seconds + flight + kParachuteSeconds;
}

std::optional<UnitType> Simulation::cargo_aircraft_type(FactionId faction) const {
    std::optional<UnitType> cargo;
    for (const auto& [type, prototype] : get_unit_prototypes()) {
        if (prototype.faction == faction && prototype.cargo_transport && (!cargo || type < *cargo)) cargo = type;
    }
    return cargo;
}

EntityId Simulation::launch_cargo_aircraft(FactionId faction, float x, float y) {
    const auto cargo = cargo_aircraft_type(faction);
    if (!cargo) return INVALID_ENTITY;
    const float entry_x = pathfinding_.min_world_x_center() - pathfinding_.cell_size();
    const int spawned = create_unit_with_type(entry_x, y, *cargo, faction);
    if (spawned < 0) return INVALID_ENTITY;
    const auto transport = static_cast<EntityId>(spawned);
    if (auto* aircraft = component_manager_.get_component<Aircraft>(transport)) {
        aircraft->status = Aircraft::Status::AIRBORNE;
        aircraft->mission = Aircraft::Mission::RECON;
        aircraft->max_airborne_time_ms = 0.0f;
        aircraft->fuel_consumption_rate = 0.0f;
        aircraft->material_consumption_rate = 0.0f;
    }
    move_unit(transport, x, y);
    return transport;
}

bool Simulation::order_paradrop(FactionId faction, UnitType type, float x, float y) {
    if (!paradrop_refusal(faction, type, x, y).empty()) return false;
    const float cost = production_manager_.get_unit_metal_cost(faction, type);
    if (!production_manager_.deduct_faction_resources(faction, cost, 0.0f)) return false;
    InboundForce force{faction, type, tick_ + static_cast<uint32_t>(std::ceil(paradrop_seconds(type, x) * 20.0f))};
    force.airdrop = true;
    force.launch_tick = tick_ + static_cast<uint32_t>(std::ceil(get_unit_prototypes().at(type).build_time_seconds * 20.0f));
    force.drop_x = x;
    force.drop_y = y;
    force.drop_sequence = paradrop_sequence_++;
    inbound_forces_.push_back(force);
    return true;
}

size_t Simulation::issue_fuel_shipment_command(FactionId faction) {
    const EntityId post = production_manager_.faction_line(faction);
    if (post == INVALID_ENTITY) return 0;
    return issue_commands({post}, faction, CommandType::BUILD, 0.0f, 0.0f, FUEL_SHIPMENT_ORDER_FLAG);
}

size_t Simulation::issue_paradrop_command(FactionId faction, UnitType type, float x, float y) {
    const EntityId post = production_manager_.faction_line(faction);
    if (post == INVALID_ENTITY) return 0;
    return issue_commands({post}, faction, CommandType::BUILD, x, y, PARADROP_ORDER_FLAG | static_cast<uint32_t>(type));
}

void Simulation::deliver_inbound_forces() {
    if (inbound_forces_.empty()) return;
    std::array<int, FACTION_COUNT> landed{};
    for (size_t index = 0; index < inbound_forces_.size();) {
        const auto force = inbound_forces_[index];
        float site_x = 0.0f;
        float site_y = 0.0f;
        if (force.airdrop && !force.released) {
            auto& flight = inbound_forces_[index];
            if (flight.transport == INVALID_ENTITY) {
                // Ready: the aircraft takes off from beyond the west map edge.
                if (tick_ >= flight.launch_tick) flight.transport = launch_cargo_aircraft(flight.faction, flight.drop_x, flight.drop_y);
                ++index;
                continue;
            }
            const auto* position = component_manager_.get_component<Position>(flight.transport);
            if (!position || get_unit_is_dead(flight.transport)) {
                ++paradrops_lost_[static_cast<size_t>(flight.faction)];
                inbound_forces_.erase(inbound_forces_.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            const float distance = std::hypot(position->x - flight.drop_x, position->y - flight.drop_y);
            const auto* data = component_manager_.get_component<UnitData>(flight.transport);
            const float speed = std::max(data ? data->speed : 90.0f, 1.0f);
            if (distance <= kParadropReleaseRadius) {
                // Released over the zone: the unit comes down under canopy and
                // the aircraft, unloaded, leaves the theatre.
                flight.released = true;
                flight.arrive_tick = tick_ + static_cast<uint32_t>(std::ceil(kParachuteSeconds * 20.0f));
                destroy_unit(flight.transport);
                flight.transport = INVALID_ENTITY;
            } else {
                flight.arrive_tick = tick_ + static_cast<uint32_t>(std::ceil(
                    ((distance - kParadropReleaseRadius) / speed + kParachuteSeconds) * 20.0f));
            }
            ++index;
            continue;
        }
        if (force.airdrop) {
            if (force.arrive_tick > tick_ || unit_count(force.faction) >= unit_cap_per_player_) {
                ++index;
                continue;
            }
            if (land_paradrop(force)) {
                inbound_forces_.erase(inbound_forces_.begin() + static_cast<std::ptrdiff_t>(index));
            } else {
                ++index;
            }
            continue;
        }
        // A force waits for an intact terminal if its route becomes unavailable.
        bool terminal_ready = false;
        if (force.route_mode == static_cast<uint8_t>(GroundTransport::AIR)) {
            terminal_ready = landing_site(force.faction, site_x, site_y);
        } else if (force.route_mode <= static_cast<uint8_t>(GroundTransport::RAIL)) {
            const auto route = transport_route(force.faction, static_cast<GroundTransport>(force.route_mode));
            terminal_ready = route.enabled && is_land_position(route.terminal_x, route.terminal_y);
            site_x = route.terminal_x;
            site_y = route.terminal_y;
        }
        if (force.arrive_tick > tick_ || !terminal_ready ||
            unit_count(force.faction) >= unit_cap_per_player_) {
            ++index;
            continue;
        }
        const int slot = landed[static_cast<size_t>(force.faction)]++;
        // Land in a loose block, spread so arrivals on one tick do not overlap.
        const float spread_x = (static_cast<float>(slot % 6) - 2.5f) * 14.0f;
        const float spread_y = static_cast<float>(slot / 6) * 14.0f;
        // A force is only taken off the list once it is on the ground: if its
        // spot is refused, it tries the site itself, then circles and retries.
        const float attempts[][2] = {{spread_x, spread_y}, {0.0f, 0.0f}, {spread_x, -spread_y}, {-30.0f, 30.0f}};
        bool landed_ok = false;
        for (const auto& offset : attempts) {
            if (create_unit_with_type(site_x + offset[0], site_y + offset[1], force.type, force.faction) >= 0) {
                landed_ok = true;
                break;
            }
        }
        if (landed_ok) {
            inbound_forces_.erase(inbound_forces_.begin() + static_cast<std::ptrdiff_t>(index));
        } else {
            --landed[static_cast<size_t>(force.faction)];
            ++index;
        }
    }
}

// Parachutes drift, so each unit comes down on its own spot around the drop
// point: a sunflower spiral by order, with the radius varied by a fixed hash so
// the drop is scattered but the same in every playback. A spot a unit cannot
// stand on is passed over for the next one out, then the point itself.
bool Simulation::land_paradrop(const InboundForce& force) {
    constexpr float kGoldenAngle = 2.39996323f;
    uint32_t hash = force.drop_sequence * 2654435761u;
    hash ^= hash >> 15;
    for (int attempt = 0; attempt < 12; ++attempt) {
        const float turn = static_cast<float>(force.drop_sequence + static_cast<uint32_t>(attempt) * 7u);
        const float spread = 0.35f + 0.65f * static_cast<float>((hash >> 8) % 1000u) / 1000.0f;
        const float radius = kParadropScatterMeters * spread * std::min(1.0f, 0.4f + 0.1f * static_cast<float>(attempt));
        const float x = force.drop_x + std::cos(turn * kGoldenAngle) * radius;
        const float y = force.drop_y + std::sin(turn * kGoldenAngle) * radius;
        if (!is_land_position(x, y) || !pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y))) continue;
        if (create_unit_with_type(x, y, force.type, force.faction) >= 0) return true;
    }
    return create_unit_with_type(force.drop_x, force.drop_y, force.type, force.faction) >= 0;
}

void Simulation::install_fob(EntityId entity, float x, float y, InstallationType installation_type) {
    auto* faction = component_manager_.get_component<Faction>(entity);
    if (!faction) return;
    
    static constexpr float FOB_CONSTRUCTION_COST_METAL = 500.0f;
    static constexpr float FOB_CONSTRUCTION_COST_ENERGY = 250.0f;
    
    // Deduct resources from faction's economy
    if (!production_manager_.deduct_faction_resources(faction->faction_id, FOB_CONSTRUCTION_COST_METAL, FOB_CONSTRUCTION_COST_ENERGY)) {
        return;
    }
    
    // Add installation with construction tracking
    territorial_control_.add_installation(x, y, installation_type, faction->faction_id);
}

void Simulation::requisition_unit(EntityId entity, float x, float y, UnitType unit_type) {
    auto* faction = component_manager_.get_component<Faction>(entity);
    if (!faction) return;
    
    auto line_id = production_manager_.faction_line(faction->faction_id);
    if (line_id == INVALID_ENTITY) return;
    
    production_manager_.queue_requisition(line_id, faction->faction_id, unit_type, x, y);
}

void Simulation::harvest_resource(EntityId entity, float x, float y) {
    if (const EntityId depot = production_manager_.fuel_depot_at(x, y); depot != INVALID_ENTITY) {
        // A depot changes hands when a unit reaches it, not when one is ordered to.
        std::erase_if(fuel_captures_, [entity](const FuelCapture& capture) { return capture.unit == entity; });
        std::erase_if(site_captures_, [entity](const SiteCapture& capture) { return capture.unit == entity; });
        if (component_manager_.get_component<Faction>(entity)) {
            const auto& node = production_manager_.resource_nodes().at(depot);
            fuel_captures_.push_back({entity, depot});
            move_unit(entity, node.x, node.y);
        }
        return;
    }
    EntityId extractor_id, node_id;
    if (!production_manager_.find_or_create_extractor(x, y, extractor_id, node_id)) {
        return;
    }
    const auto* faction = component_manager_.get_component<Faction>(entity);
    if (!faction || production_manager_.faction_line(faction->faction_id) == INVALID_ENTITY) return;
    if (production_manager_.extractors().find(extractor_id) == production_manager_.extractors().end()) return;
    // A field facility is a territorial objective. It changes hands when the
    // unit reaches it (update_site_captures), and a redirected unit gives up.
    std::erase_if(fuel_captures_, [entity](const FuelCapture& capture) { return capture.unit == entity; });
    std::erase_if(site_captures_, [entity](const SiteCapture& capture) { return capture.unit == entity; });
    site_captures_.push_back({entity, node_id, extractor_id});
    move_unit(entity, x, y);
}

// Claiming a site reroutes its single shared Materials output to the
// claimant's command storage.
void Simulation::update_site_captures() {
    std::erase_if(site_captures_, [this](const SiteCapture& capture) {
        const auto node = production_manager_.resource_nodes().find(capture.node);
        const auto extractor = production_manager_.extractors().find(capture.extractor);
        const auto* position = component_manager_.get_component<Position>(capture.unit);
        const auto* faction = component_manager_.get_component<Faction>(capture.unit);
        const auto* health = component_manager_.get_component<Health>(capture.unit);
        if (node == production_manager_.resource_nodes().end() || extractor == production_manager_.extractors().end() ||
            !position || !faction || (health && health->is_dead)) return true;
        if (std::hypot(position->x - node->second.x, position->y - node->second.y) <= kSiteCaptureRadius) {
            extractor->second.storage_id = production_manager_.faction_line(faction->faction_id);
            component_manager_.add_component(capture.unit, Harvester{capture.node, 0.0f, 0.0f});
            return true;
        }
        const auto target = move_targets_.find(capture.unit);
        return target == move_targets_.end() ||
            std::hypot(target->second.arrival.x - node->second.x, target->second.arrival.y - node->second.y) > kSiteCaptureRadius;
    });
}

std::vector<MaterialSiteStatus> Simulation::material_sites() const {
    std::vector<MaterialSiteStatus> sites;
    for (const auto& [id, node] : production_manager_.resource_nodes()) {
        if (node.type == ResourceNode::Type::ENERGY) continue;
        MaterialSiteStatus status{id, node.x, node.y};
        for (const auto& [extractor_id, extractor] : production_manager_.extractors()) {
            (void)extractor_id;
            if (extractor.resource_node_id != id || extractor.storage_id == INVALID_ENTITY) continue;
            for (size_t faction = 0; faction < FACTION_COUNT; ++faction) {
                if (production_manager_.faction_line(static_cast<FactionId>(faction)) == extractor.storage_id) status.owner = static_cast<int>(faction);
            }
        }
        for (const auto& capture : site_captures_) {
            const auto* faction = component_manager_.get_component<Faction>(capture.unit);
            if (capture.node != id || !faction) continue;
            status.capturing = true;
            status.capturing_faction = static_cast<int>(faction->faction_id);
        }
        sites.push_back(status);
    }
    std::sort(sites.begin(), sites.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return sites;
}

bool Simulation::destroy_resource_site(EntityId entity, float x, float y) {
    if (!component_manager_.get_component<Faction>(entity)) return false;
    if (const EntityId depot = production_manager_.fuel_depot_at(x, y); depot != INVALID_ENTITY) {
        const auto* position = component_manager_.get_component<Position>(entity);
        const auto& node = production_manager_.resource_nodes().at(depot);
        if (!position || std::hypot(position->x - node.x, position->y - node.y) > kFuelDepotCaptureRadius) {
            return false;
        }
    }
    return production_manager_.destroy_resource_site(x, y);
}

void Simulation::defend_area(EntityId entity, float x, float y) {
    stop_unit(entity);
    FactionId faction = FactionId::ELITE_PRECISION;
    auto* faction_comp = component_manager_.get_component<Faction>(entity);
    if (faction_comp) faction = faction_comp->faction_id;
    EntityId nearest_enemy = find_nearest_visible_enemy(faction, x, y, 80.0f);
    if (nearest_enemy == 0) {
        move_unit(entity, x, y);
    } else {
        const auto* enemy_pos = component_manager_.get_component<Position>(nearest_enemy);
        if (enemy_pos) {
            float dx = enemy_pos->x - x, dy = enemy_pos->y - y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 0) {
                float approach_point_x = x + (dx / dist) * 30.0f;
                float approach_point_y = y + (dy / dist) * 30.0f;
                move_unit(entity, approach_point_x, approach_point_y);
            }
        }
    }
}


bool Simulation::is_visible_to(FactionId faction, EntityId target) const {
    const auto* position = component_manager_.get_component<Position>(target);
    const auto* hp = component_manager_.get_component<Health>(target);
    if (!position || !hp || hp->is_dead || hp->current <= 0) return false;
    if (vision_active_) return is_watched(faction, target, position->x, position->y);
    return is_position_visible_to(faction, position->x, position->y);
}

EntityId Simulation::find_nearest_visible_enemy(FactionId faction, float x, float y, float max_range) const {
    EntityId nearest = 0;
    float nearest_dist_sq = max_range * max_range;
    
    for (auto enemy : spatial_grid().query_in_region(x, y, max_range)) {
        const auto* enemy_faction = component_manager_.get_component<Faction>(enemy);
        const auto* health = component_manager_.get_component<Health>(enemy);
        if (!enemy_faction || !health || health->is_dead || health->current <= 0) continue;
        if (enemy_faction->faction_id == faction) continue;
        
        const auto* position = component_manager_.get_component<Position>(enemy);
        if (!position) continue;
        
        float dx = position->x - x, dy = position->y - y;
        float dist_sq = dx*dx + dy*dy;
        if (dist_sq < nearest_dist_sq && (!vision_active_ || is_watched(faction, enemy, position->x, position->y))) {
            nearest_dist_sq = dist_sq;
            nearest = enemy;
        }
    }
    return nearest;
}

bool Simulation::is_position_visible_to(FactionId faction, float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    if (vision_active_) return vision_map_.visible(faction, x, y);
    for (const auto& source : vision_sources_) {
        const float dx = source.x - x, dy = source.y - y;
        if (source.faction == faction && dx * dx + dy * dy <= source.radius * source.radius) return true;
    }
    if (combat_observers_active_) {
        const size_t index = static_cast<size_t>(faction);
        if (index >= combat_observers_.size()) return false;
        for (const auto observer : combat_observers_[index]) {
            const auto* health = component_manager_.get_component<Health>(observer);
            const auto* sensor = component_manager_.get_component<UnitData>(observer);
            const auto* weapon = component_manager_.get_component<Weapon>(observer);
            const auto* origin = component_manager_.get_component<Position>(observer);
            if (!health || health->is_dead || health->current <= 0 || !origin) continue;
            const float range = sensor ? sensor->view_range : (weapon ? weapon->range : 0);
            const float dx = origin->x - x, dy = origin->y - y;
            if (std::isfinite(range) && range > 0 && dx * dx + dy * dy <= range * range) return true;
        }
        return false;
    }
    for (auto observer : entity_manager_.get_entities()) {
        const auto* owner = component_manager_.get_component<Faction>(observer);
        const auto* health = component_manager_.get_component<Health>(observer);
        const auto* sensor = component_manager_.get_component<UnitData>(observer);
        const auto* weapon = component_manager_.get_component<Weapon>(observer);
        const auto* origin = component_manager_.get_component<Position>(observer);
        if (!owner || owner->faction_id != faction || !health || health->is_dead || health->current <= 0 || !origin) continue;
        // Legacy untyped benchmark units use weapon range as their sensor.
        const float range = sensor ? sensor->view_range : (weapon ? weapon->range : 0);
        const float dx = origin->x - x, dy = origin->y - y;
        if (std::isfinite(range) && range > 0 && dx*dx + dy*dy <= range*range) return true;
    }
    return false;
}

bool Simulation::validate_command(const InputCommand& cmd, uint32_t execution_tick) const {
    // A flight plan is set before the plane reaches the map, and a cargo
    // aircraft flies straight to its airfield.
    if (recon_flights_.contains(static_cast<EntityId>(cmd.entity_id))) return false;
    if (const auto* cargo = component_manager_.get_component<UnitData>(cmd.entity_id)) {
        const auto prototype = get_unit_prototypes().find(static_cast<UnitType>(cargo->unit_type));
        if (prototype != get_unit_prototypes().end() && prototype->second.cargo_transport) return false;
    }
    if (cmd.tick_id != execution_tick || cmd.player_id > 2 ||
        cmd.cmd_type > static_cast<uint8_t>(CommandType::INSTALL) ||
        cmd.cmd_type == RETIRED_RESEARCH_COMMAND) {
        fprintf(stderr, "VALIDATE_FAIL: tick_id mismatch or invalid player_id/cmd_type (entity=%u player=%u tick=%u exec=%u type=%u)\n", cmd.entity_id, cmd.player_id, cmd.tick_id, execution_tick, cmd.cmd_type);
        return false;
    }
    const auto* owner = component_manager_.get_component<Faction>(cmd.entity_id);
    const auto* health = component_manager_.get_component<Health>(cmd.entity_id);
    if (!owner) {
        fprintf(stderr, "VALIDATE_FAIL: no Faction component (entity=%u)\n", cmd.entity_id);
        return false;
    }
    if (static_cast<uint8_t>(owner->faction_id) != cmd.player_id) {
        fprintf(stderr, "VALIDATE_FAIL: faction mismatch (entity=%u faction=%u player=%u)\n", cmd.entity_id, static_cast<uint8_t>(owner->faction_id), cmd.player_id);
        return false;
    }
    if (!health || health->is_dead || health->current <= 0) {
        fprintf(stderr, "VALIDATE_FAIL: no Health or dead (entity=%u current=%.1f is_dead=%d)\n", cmd.entity_id, health ? health->current : -1.0f, health ? health->is_dead : -1);
        return false;
    }
    if (cmd.cmd_type == static_cast<uint8_t>(CommandType::HARVEST)) {
        if ((cmd.extra & POWER_ORDER_FLAG) == 0) return cmd.extra == 0;
        return (cmd.extra & ~(POWER_ORDER_FLAG | 0x00FFFFFFu)) == 0 &&
            power_order_refusal(cmd.entity_id, static_cast<PowerOrder>((cmd.extra >> 16) & 0xFFu), cmd.extra & 0xFFFFu).empty();
    }
    const auto type = static_cast<CommandType>(cmd.cmd_type);
    // BUILD carries the player-selected rally/output target. It is not a
    // movement target, but it is authoritative production data and must reach
    // the queue unchanged.
    if (type != CommandType::MOVE && type != CommandType::PATROL && type != CommandType::DEFEND &&
        type != CommandType::BUILD && type != CommandType::INSTALL &&
        (cmd.target_x != 0 || cmd.target_y != 0)) {
        fprintf(stderr, "VALIDATE_FAIL: invalid target for command type (entity=%u type=%u target=(%d,%d))\n", cmd.entity_id, cmd.cmd_type, cmd.target_x, cmd.target_y);
        return false;
    }
    const auto* unit = component_manager_.get_component<UnitData>(cmd.entity_id);
    if (type == CommandType::ATTACK) {
        const auto* target = component_manager_.get_component<Faction>(cmd.extra);
        const auto* hp = component_manager_.get_component<Health>(cmd.extra);
        // A weapon is only ordered onto what it can engage: no tank guns at jets.
        const auto* weapon = component_manager_.get_component<Weapon>(cmd.entity_id);
        return target && hp && !hp->is_dead && hp->current > 0 && target->faction_id != owner->faction_id &&
               weapon && weapon_reaches(weapon->hits_ground, weapon->hits_air, is_air_target(component_manager_, cmd.extra)) &&
               is_visible_to(owner->faction_id, cmd.extra);
    }
    if (type == CommandType::STOP) {
        if (cmd.extra != 0) {
            fprintf(stderr, "VALIDATE_FAIL: STOP extra != 0 (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
            return false;
        }
        return true;
    }
    if (type == CommandType::RETURN) {
        if (cmd.extra != 0) {
            fprintf(stderr, "VALIDATE_FAIL: RETURN extra != 0 (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
            return false;
        }
        if (const auto* vessel=component_manager_.get_component<NavalVessel>(cmd.entity_id)) {
            const auto base=production_manager_.faction_line(owner->faction_id);
            const auto* base_hp=component_manager_.get_component<Health>(base);
            const auto* position=component_manager_.get_component<Position>(base);
            const auto funds=production_manager_.storages().find(base);
            if(!base_hp || base_hp->is_dead || !position || funds==production_manager_.storages().end()) {
                fprintf(stderr, "VALIDATE_FAIL: RETURN naval base check failed (entity=%u base=%u)\n", cmd.entity_id, base);
                return false;
            }
            const EntityId naval_base=const_cast<LogisticsManager&>(logistics_manager_).find_nearest_recovery_facility(
                vessel->x, vessel->y, RecoveryFacility::Type::NAVAL_BASE, cmd.entity_id
            );
            const auto* facility=component_manager_.get_component<RecoveryFacility>(naval_base);
            const float dx=facility ? vessel->x-facility->x : std::numeric_limits<float>::infinity();
            const float dy=facility ? vessel->y-facility->y : std::numeric_limits<float>::infinity();
            const float needed_fuel = std::max(0.0f, vessel->max_fuel - vessel->fuel);
            const float material_cost = needed_fuel * 0.1f;
            bool ok = facility && dx*dx+dy*dy<=facility->max_recovery_distance*facility->max_recovery_distance &&
                funds->second.energy_storage>=needed_fuel && funds->second.metal_storage>=material_cost;
            if (!ok) fprintf(stderr, "VALIDATE_FAIL: RETURN naval position/fuel check failed (entity=%u dist=%.2f fuel=%.2f)\n", cmd.entity_id, sqrt(dx*dx+dy*dy), funds->second.energy_storage - vessel->max_fuel + vessel->fuel);
            return ok;
        }

        const auto* aircraft = component_manager_.get_component<Aircraft>(cmd.entity_id);
        bool ok = aircraft && aircraft->status != Aircraft::Status::CRASHED;
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: RETURN aircraft check failed (entity=%u has_aircraft=%d status=%d)\n", cmd.entity_id, !!aircraft, aircraft ? (int)aircraft->status : -1);
        return ok;
    }
    if (type == CommandType::BUILD && (cmd.extra & STRUCTURE_ORDER_FLAG) != 0) {
        // A structure order: the low byte is the structure, and only a
        // construction unit may give it. The site is checked again when the
        // order runs, because the ground can change in between.
        const uint32_t structure = cmd.extra & ~STRUCTURE_ORDER_FLAG;
        const bool ok = structure <= 3 && is_construction_unit(cmd.entity_id);
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: structure order needs a construction unit (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
        return ok;
    }
    if (type == CommandType::BUILD && cmd.extra == FUEL_SHIPMENT_ORDER_FLAG) {
        const bool ok = production_manager_.faction_line(owner->faction_id) == static_cast<EntityId>(cmd.entity_id);
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: fuel shipment order must come from the command post (entity=%u)\n", cmd.entity_id);
        return ok;
    }
    if (type == CommandType::BUILD && (cmd.extra & PARADROP_ORDER_FLAG) != 0) {
        // A paratroop drop is ordered from the command post. Whether it can
        // fly is decided when it runs, with the refusal rules above.
        const bool ok = (cmd.extra & ~PARADROP_ORDER_FLAG) <= 255 &&
                        production_manager_.faction_line(owner->faction_id) == static_cast<EntityId>(cmd.entity_id);
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: paradrop order must come from the command post (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
        return ok;
    }
    if (type == CommandType::BUILD &&
        (cmd.extra & (ROAD_DELIVERY_ORDER_FLAG | RAIL_DELIVERY_ORDER_FLAG)) != 0) {
        const bool road = (cmd.extra & ROAD_DELIVERY_ORDER_FLAG) != 0;
        const uint32_t flag = road ? ROAD_DELIVERY_ORDER_FLAG : RAIL_DELIVERY_ORDER_FLAG;
        const auto mode = road ? GroundTransport::ROAD : GroundTransport::RAIL;
        return (cmd.extra & ~(flag | 0xFFu)) == 0 &&
            production_manager_.faction_line(owner->faction_id) == static_cast<EntityId>(cmd.entity_id) &&
            delivery_refusal_via(owner->faction_id, static_cast<UnitType>(cmd.extra & 0xFFu), mode).empty();
    }
    if (type == CommandType::BUILD) {
        bool ok = cmd.extra <= 255 && production_manager_.can_queue_unit(cmd.entity_id, owner->faction_id, static_cast<UnitType>(cmd.extra));
        const auto prototype = get_unit_prototypes().find(static_cast<UnitType>(cmd.extra));
        if (ok && prototype != get_unit_prototypes().end() && prototype->second.is_aircraft && prototype->second.requires_runway &&
            prototype->second.recon_loiter_seconds <= 0.0f)
            ok = territorial_control_.has_active_installation(owner->faction_id, InstallationType::AIRFIELD);
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: BUILD check failed (entity=%u extra=%u type=%u)\n", cmd.entity_id, cmd.extra, static_cast<unsigned>(cmd.extra));
        return ok;
    }
    if (type == CommandType::INSTALL) {
        if (cmd.extra != static_cast<uint32_t>(InstallationType::FORWARD_OPERATING_BASE) ||
            !territorial_control_.has_capability(Entity{cmd.entity_id}, SeizureCapability::CONSTRUCT_FOB)) {
            fprintf(stderr, "VALIDATE_FAIL: INSTALL requires FOB capability (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
            return false;
        }
        const float install_x = static_cast<float>(cmd.target_x) / command_position_scale_;
        const float install_y = static_cast<float>(cmd.target_y) / command_position_scale_;
        if (!is_land_position(install_x, install_y)) return false;
        return true;
    }
    if (unit && unit->speed <= 0) {
        fprintf(stderr, "VALIDATE_FAIL: unit speed <= 0 (entity=%u speed=%.1f)\n", cmd.entity_id, unit->speed);
        return false;
    }
    const auto& navigation = navigation_for(cmd.entity_id);
    const float x = static_cast<float>(cmd.target_x) / command_position_scale_;
    const float y = static_cast<float>(cmd.target_y) / command_position_scale_;
    if (type != CommandType::DEFEND && type != CommandType::HARVEST) {
        int gx = navigation.to_grid_x(x);
        int gy = navigation.to_grid_y(y);
        if (!navigation.is_walkable(gx, gy)) {
            return false;
        }
    }
    if (type == CommandType::MOVE) {
        if ((cmd.extra & MOVE_QUEUE_FLAG) != 0) {
            const auto queue = queued_moves_.find(cmd.entity_id);
            const std::size_t held = (move_targets_.count(cmd.entity_id) ? 1u : 0u) +
                (queue == queued_moves_.end() ? 0u : queue->second.size());
            if (held >= MAX_MOVE_WAYPOINTS) {
                fprintf(stderr, "VALIDATE_FAIL: MOVE queue full (entity=%u)\n", cmd.entity_id);
                return false;
            }
        }
        if ((cmd.extra & ~MOVE_QUEUE_FLAG) > 10000) {
            fprintf(stderr, "VALIDATE_FAIL: MOVE spacing > 10000 (entity=%u spacing=%u)\n", cmd.entity_id, cmd.extra);
            return false;
        }
        return true;
    }
    if (cmd.extra != 0) {
        fprintf(stderr, "VALIDATE_FAIL: non-MOVE command extra != 0 (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
        return false;
    }
    return true;
}

size_t Simulation::submit_commands(const std::vector<InputCommand>& commands) {
    if (commands.empty() || commands.size() > MAX_COMMANDS_PER_TICK) return 0;
    if (command_manager_.local_command_count() + commands.size() > MAX_COMMANDS_PER_TICK * 2) return 0;
    std::vector<EntityId> ids;
    ids.reserve(commands.size());
    for (const auto& command : commands) {
        if (!validate_command(command, tick_ + 1)) return 0;
        ids.push_back(command.entity_id);
    }
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) return 0;
    bool success = command_manager_.inject_local_commands(commands);
    return success ? commands.size() : 0;
}

size_t Simulation::issue_commands(const std::vector<EntityId>& entities, FactionId player,
                                 CommandType type, float x, float y, uint32_t extra) {
    int16_t encoded_x, encoded_y;
    const auto encode_position = [this](float value, int16_t& encoded) {
        if (!std::isfinite(value)) return false;
        const float scaled = std::round(value * command_position_scale_);
        if (scaled < static_cast<float>(std::numeric_limits<int16_t>::min()) ||
            scaled > static_cast<float>(std::numeric_limits<int16_t>::max())) return false;
        encoded = static_cast<int16_t>(scaled);
        return true;
    };
    if (!encode_position(x, encoded_x) || !encode_position(y, encoded_y) || entities.size() > MAX_COMMANDS_PER_TICK) return 0;
    std::vector<InputCommand> commands;
    commands.reserve(entities.size());
    for (auto id : entities) {
        InputCommand command{};
        command.entity_id = id;
        command.player_id = static_cast<uint8_t>(player);
        command.cmd_type = static_cast<uint8_t>(type);
        command.tick_id = tick_ + 1;
        command.target_x = encoded_x;
        command.target_y = encoded_y;
        command.extra = extra;
        commands.push_back(command);
    }
    return submit_commands(commands);
}

size_t Simulation::issue_move_commands(const std::vector<EntityId>& ids, FactionId player,
                                      float x, float y, float spacing, bool queued) {
    if (!std::isfinite(spacing) || spacing < movement::kMinimumFormationSpacingMeters ||
        spacing > movement::kMaximumFormationSpacingMeters) return 0;
    return issue_commands(ids, player, CommandType::MOVE, x, y,
                          static_cast<uint32_t>(std::round(spacing * command_position_scale_)) |
                              (queued ? MOVE_QUEUE_FLAG : 0u));
}
size_t Simulation::issue_stop_commands(const std::vector<EntityId>& ids, FactionId player) {
    return issue_commands(ids, player, CommandType::STOP);
}
size_t Simulation::issue_attack_commands(const std::vector<EntityId>& ids, FactionId player, EntityId target) {
    return issue_commands(ids, player, CommandType::ATTACK, 0, 0, target);
}
size_t Simulation::issue_patrol_commands(const std::vector<EntityId>& ids, FactionId player, float x, float y) {
    return issue_commands(ids, player, CommandType::PATROL, x, y);
}
size_t Simulation::issue_return_commands(const std::vector<EntityId>& ids, FactionId player) {
    return issue_commands(ids, player, CommandType::RETURN);
}
size_t Simulation::issue_build_commands(const std::vector<EntityId>& ids, FactionId player,
                                       float x, float y, int64_t unit_type) {
    if (unit_type < 0 || unit_type > 255) return 0;
    return issue_commands(ids, player, CommandType::BUILD, x, y, static_cast<uint32_t>(unit_type));
}
size_t Simulation::issue_structure_command(EntityId engineer, FactionId faction, uint8_t structure_type,
                                           float x, float y) {
    if (structure_type > 3) return 0;
    return issue_commands({engineer}, faction, CommandType::BUILD, x, y, STRUCTURE_ORDER_FLAG | structure_type);
}

bool Simulation::is_construction_unit(EntityId entity) const {
    const auto* data = component_manager_.get_component<UnitData>(entity);
    if (!data) return false;
    const auto prototype = get_unit_prototypes().find(static_cast<UnitType>(data->unit_type));
    return prototype != get_unit_prototypes().end() && prototype->second.construction_unit;
}

bool Simulation::structure_queued(FactionId faction, uint8_t structure_type) const {
    const auto line = production_manager_.production_lines().find(production_manager_.faction_line(faction));
    if (line == production_manager_.production_lines().end()) return false;
    auto queue = line->second.queue;
    for (; !queue.empty(); queue.pop()) {
        const auto& job = queue.front();
        if (job.type != ConstructionQueueEntry::Type::BUILDING) continue;
        if (job.structure_type == structure_type ||
            (job.structure_type == EARTHWORKS_JOB && job.graded_structure == structure_type)) return true;
    }
    return false;
}

size_t Simulation::issue_harvest_commands(const std::vector<EntityId>& ids, FactionId player, float x, float y) {
    return issue_commands(ids, player, CommandType::HARVEST, x, y);
}
size_t Simulation::issue_defend_commands(const std::vector<EntityId>& ids, FactionId player, float x, float y) {
    return issue_commands(ids, player, CommandType::DEFEND, x, y);
}
size_t Simulation::issue_install_commands(const std::vector<EntityId>& ids, FactionId player,
                                          float x, float y, InstallationType installation_type) {
    if (installation_type != InstallationType::FORWARD_OPERATING_BASE) return 0;
    return issue_commands(ids, player, CommandType::INSTALL, x, y,
                          static_cast<uint32_t>(installation_type));
}

size_t Simulation::issue_requisition_commands(const std::vector<EntityId>& ids, FactionId player,
                                             float x, float y, UnitType unit_type) {
    return issue_commands(ids, player, CommandType::REQUISITION, x, y, static_cast<uint32_t>(unit_type));
}

void Simulation::render_add_unit(float x, float y, uint32_t unit_type) {
    renderer_.add_unit_instance(x, y, unit_type);
}

void Simulation::render_update() {
    renderer_.begin_frame();
    renderer_.draw_all();
    renderer_.end_frame();
}

namespace {
constexpr uint32_t kVisionRefreshTicks = 5;
}

void Simulation::refresh_contact_grid() {
    contact_grid_.fill(0);
    for (const auto entity : component_manager_.entities_with<Position, Faction>(entity_manager_.get_entities())) {
        const auto* position = component_manager_.get_component<Position>(entity);
        const auto* faction = component_manager_.get_component<Faction>(entity);
        const auto* health = component_manager_.get_component<Health>(entity);
        if (health && health->is_dead) continue;
        const int cell_x = static_cast<int>(std::floor(position->x / movement::kContactCellMeters)) & (movement::kContactGridCells - 1);
        const int cell_y = static_cast<int>(std::floor(position->y / movement::kContactCellMeters)) & (movement::kContactGridCells - 1);
        contact_grid_[cell_y * movement::kContactGridCells + cell_x] |= static_cast<uint8_t>(1u << static_cast<int>(faction->faction_id));
    }
}

bool Simulation::near_enemy(float x, float y, FactionId faction) const {
    const int cell_x = static_cast<int>(std::floor(x / movement::kContactCellMeters));
    const int cell_y = static_cast<int>(std::floor(y / movement::kContactCellMeters));
    const uint8_t own = static_cast<uint8_t>(1u << static_cast<int>(faction));
    for (int row = cell_y - 2; row <= cell_y + 2; ++row) {
        for (int column = cell_x - 2; column <= cell_x + 2; ++column) {
            const int index = (row & (movement::kContactGridCells - 1)) * movement::kContactGridCells +
                (column & (movement::kContactGridCells - 1));
            if (contact_grid_[index] & ~own) return true;
        }
    }
    return false;
}

void Simulation::prediction_phase(float delta_ms) {
    const float dt = delta_ms / 1000.0f;
    if (tick_ % movement::kContactRefreshTicks == 1) refresh_contact_grid();
    const auto entities = component_manager_.entities_with<Position, Velocity>(entity_manager_.get_entities());
    // Each side's fuel store and each ground unit type's burn, looked up once a tick.
    std::array<Storage*, FACTION_COUNT> fuel_stores{};
    std::array<float, 256> burn_per_meter{};
    if (ground_fuel_burn_) {
        fuel_starved_.fill(0);
        for (size_t index = 0; index < FACTION_COUNT; ++index) {
            const auto store = production_manager_.storages().find(production_manager_.faction_storage(static_cast<FactionId>(index)));
            if (store != production_manager_.storages().end()) fuel_stores[index] = &store->second;
        }
        for (const auto& [type, prototype] : get_unit_prototypes()) {
            if (!prototype.is_aircraft && !prototype.is_naval) burn_per_meter[static_cast<uint8_t>(type)] = prototype.fuel_per_km / 1000.0f;
        }
    }

    for (auto entity_id : entities) {
        auto* pos = component_manager_.get_component<Position>(entity_id);
        auto* vel = component_manager_.get_component<Velocity>(entity_id);
        if (!pos || !vel) {
            continue;
        }

        auto* aircraft = component_manager_.get_component<Aircraft>(entity_id);
        auto* vessel = component_manager_.get_component<NavalVessel>(entity_id);
        if ((aircraft && aircraft->status != Aircraft::Status::AIRBORNE) || (vessel && vessel->is_stranded)) {
            vel->x = vel->y = vel->z = 0;
            continue;
        }

        // Runway approaches own movement through rollout; generic point
        // steering must not move the plane a second time or stop it in mid-air.
        if (const auto ferry = ferry_flights_.find(entity_id); ferry != ferry_flights_.end() &&
            !ferry->second.leaving && !ferry->second.landed) continue;

        auto& navigation = navigation_for(entity_id);
        bool road_marching = false;
        const float heading_x = vel->x;
        const float heading_y = vel->y;
        auto target = move_targets_.find(entity_id);
        if (target != move_targets_.end()) {
            const float dx = target->second.arrival.x - pos->x;
            const float dy = target->second.arrival.y - pos->y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const auto* data = component_manager_.get_component<UnitData>(entity_id);
            const float base_move_speed = data ? data->speed : 12.0f;
            const float road_multiplier = vessel || aircraft
                ? 1.0f : navigation.movement_speed_multiplier(pos->x, pos->y);
            const auto* off_road = component_manager_.get_component<OffRoadWear>(entity_id);
            const float wear_speed_penalty = off_road
                ? std::min(get_off_road_settings().maximum_speed_penalty,
                    off_road->accumulated * get_off_road_settings().speed_penalty_per_wear)
                : 0.0f;
            const auto* unit_faction = component_manager_.get_component<Faction>(entity_id);
            road_marching = !vessel && !aircraft && unit_faction &&
                distance > movement::kRoadMarchMinimumDistanceMeters &&
                !near_enemy(pos->x, pos->y, unit_faction->faction_id);
            const float move_speed = base_move_speed * road_multiplier * (1.0f - wear_speed_penalty) *
                (road_marching ? movement::kRoadMarchSpeedMultiplier : 1.0f);
            const float max_step = move_speed * dt;

            const auto steer_toward = [&](float desired_x, float desired_y) {
                if (auto* steering = component_manager_.get_component<GroundSteering>(entity_id)) {
                    const float forward_heading = std::atan2(desired_x, desired_y);
                    const float forward_error = std::remainder(forward_heading - steering->heading, 2.0f * std::numbers::pi_v<float>);
                    const bool reverse = !steering->can_pivot_turn && steering->max_reverse_speed > 0.0f &&
                        std::abs(forward_error) >= steering->reverse_preference_threshold;
                    steering->desired_heading = reverse
                        ? std::remainder(forward_heading + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>)
                        : forward_heading;
                    const float heading_error = std::remainder(
                        steering->desired_heading - steering->heading,
                        2.0f * std::numbers::pi_v<float>
                    );
                    const float speed_fraction = std::clamp(std::abs(steering->current_speed) / std::max(move_speed, 0.001f), 0.0f, 1.0f);
                    const float turn_rate = steering->turn_rate +
                        (steering->turn_rate_at_speed - steering->turn_rate) * speed_fraction;
                    const float remaining_error = std::abs(heading_error);
                    const float alignment = std::max(0.0f, std::cos(remaining_error));
                    const float direction = reverse ? -1.0f : 1.0f;
                    const float maximum_speed = reverse ? steering->max_reverse_speed : move_speed;
                    // Wheeled vehicles retain a crawl speed through a sharp turn,
                    // causing an arc rather than a stationary center-axis spin.
                    const float maneuver_speed = steering->can_pivot_turn ? 0.0f : maximum_speed * 0.18f;
                    // Begin braking before the arrival radius. This keeps the
                    // kinematic vehicle from teleporting onto its final slot.
                    const float stopping_speed = std::sqrt(std::max(
                        0.0f,
                        2.0f * steering->deceleration *
                            std::max(distance - movement::kArrivalRadiusMeters, 0.0f)
                    ));
                    const float travel_speed = std::min(maximum_speed * alignment, stopping_speed);
                    // The turn crawl is only needed while there is braking
                    // room. Cap it near the slot so it cannot create an
                    // endless low-speed orbit around the destination.
                    const float turn_crawl_speed = std::min(maneuver_speed, stopping_speed);
                    float chosen_speed = std::max(turn_crawl_speed, travel_speed);
                    // A vehicle that cannot pivot reaches a point at distance d
                    // and bearing error e only on a turn no wider than
                    // d / (2 sin e). Drive slowly enough that the turn it can
                    // make fits, or a fast chassis circles a nearby slot forever.
                    const float bearing = std::abs(std::sin(remaining_error));
                    if (!steering->can_pivot_turn && bearing > 0.05f) {
                        const float fitting_radius = distance / (2.0f * bearing);
                        float fitting_speed;
                        if (fitting_radius >= steering->minimum_turn_radius) {
                            fitting_speed = steering->turn_rate_at_speed * fitting_radius;
                        } else {
                            // Tighter than the chassis radius: only low-speed
                            // maneuver authority, which fades with speed, can do it.
                            const float authority = steering->maneuver_turn_rate * fitting_radius;
                            fitting_speed = authority / (1.0f + authority / std::max(maximum_speed, 0.001f));
                        }
                        chosen_speed = std::min(chosen_speed, std::max(fitting_speed, 0.1f));
                    }
                    const float desired_speed = direction * chosen_speed;
                    const float rate = desired_speed > steering->current_speed ? steering->acceleration : steering->deceleration;
                    steering->current_speed += std::clamp(
                        desired_speed - steering->current_speed,
                        -rate * steering->steering_response * dt,
                        rate * steering->steering_response * dt
                    );
                    const float radius_rate = std::abs(steering->current_speed) /
                        std::max(steering->minimum_turn_radius, 0.001f);
                    // At normal travel speeds the chassis follows its authored
                    // driving radius. A radius-only limit becomes pathological
                    // during the non-pivot turn crawl: yaw authority approaches
                    // zero precisely when the vehicle needs to recover its
                    // heading. Content can supply bounded low-speed maneuver
                    // authority; it fades out as the vehicle returns to driving
                    // speed and therefore cannot loosen normal-radius travel.
                    const float low_speed_fraction = 1.0f - std::clamp(
                        std::abs(steering->current_speed) / std::max(maximum_speed, 0.001f),
                        0.0f, 1.0f
                    );
                    const float maneuver_rate = steering->maneuver_turn_rate * low_speed_fraction;
                    const float permitted_turn_rate = steering->can_pivot_turn
                        ? turn_rate : std::min(turn_rate, std::max(radius_rate, maneuver_rate));
                    steering->heading += std::clamp(
                        heading_error, -permitted_turn_rate * dt, permitted_turn_rate * dt
                    );
                    vel->x = std::sin(steering->heading) * steering->current_speed;
                    vel->y = std::cos(steering->heading) * steering->current_speed;
                } else {
                    vel->x = desired_x * move_speed;
                    vel->y = desired_y * move_speed;
                }
            };

            // Aircraft cannot brake: count a pass within one step as arrival so
            // they never jitter around a point they overshoot each tick.
            const float arrival_radius = aircraft ? std::max(movement::kArrivalRadiusMeters, max_step) : movement::kArrivalRadiusMeters;
            if (distance <= arrival_radius) {
                complete_move(entity_id);
            } else {
                if (navigation.has_line_of_sight(
                        pos->x,
                        pos->y,
                        target->second.arrival.x,
                        target->second.arrival.y)) {
                    const float desired_x = dx / distance;
                    const float desired_y = dy / distance;
                    steer_toward(desired_x, desired_y);
                } else {
                    const auto direction = navigation.flow_direction(
                        pos->x,
                        pos->y,
                        target->second.strategic_route.x,
                        target->second.strategic_route.y
                    );
                    if (direction.first == 0.0f && direction.second == 0.0f) {
                        vel->x = vel->y = 0.0f;
                    } else {
                        steer_toward(direction.first, direction.second);
                    }
                }
            }
        }

        if (aircraft && move_targets_.find(entity_id) == move_targets_.end() && orbit_eligible(entity_id)) {
            const auto* data = component_manager_.get_component<UnitData>(entity_id);
            const float speed = std::max(data ? data->speed : aircraft->cruise_speed, 1.0f);
            auto orbit = air_orbits_.find(entity_id);
            if (orbit == air_orbits_.end()) {
                float hx = heading_x, hy = heading_y;
                const float length = std::hypot(hx, hy);
                if (length < 1e-3f) { hx = 1.0f; hy = 0.0f; } else { hx /= length; hy /= length; }
                AirOrbit created;
                created.radius = std::clamp(speed * 6.0f, 400.0f, 2500.0f);
                created.cx = pos->x - hy * created.radius;
                created.cy = pos->y + hx * created.radius;
                created.angle = std::atan2(pos->y - created.cy, pos->x - created.cx);
                orbit = air_orbits_.emplace(entity_id, created).first;
            }
            auto& circle = orbit->second;
            circle.angle = std::remainder(circle.angle + speed / circle.radius * dt, 2.0f * std::numbers::pi_v<float>);
            vel->x = (circle.cx + std::cos(circle.angle) * circle.radius - pos->x) / dt;
            vel->y = (circle.cy + std::sin(circle.angle) * circle.radius - pos->y) / dt;
            vel->z = 0.0f;
        }

        const float previous_x = pos->x;
        const float previous_y = pos->y;
        pos->x += vel->x * dt;
        pos->y += vel->y * dt;
        if (ground_fuel_burn_ && !aircraft && !vessel) {
            const float driven = std::hypot(pos->x - previous_x, pos->y - previous_y);
            const auto* data = component_manager_.get_component<UnitData>(entity_id);
            const auto* owner = component_manager_.get_component<Faction>(entity_id);
            const size_t side = owner ? static_cast<size_t>(owner->faction_id) : FACTION_COUNT;
            const float need = data ? driven * burn_per_meter[data->unit_type] : 0.0f;
            if (need > 0.0f && side < FACTION_COUNT && fuel_stores[side]) {
                auto& fuel = fuel_stores[side]->tactical.fuel;
                if (fuel.current >= need) {
                    fuel.current -= need;
                    fuel_stores[side]->energy_storage = fuel.current;
                } else {
                    // Out of fuel: the step is not taken.
                    pos->x = previous_x;
                    pos->y = previous_y;
                    vel->x = vel->y = 0.0f;
                    if (auto* steering = component_manager_.get_component<GroundSteering>(entity_id)) steering->current_speed = 0.0f;
                    ++fuel_starved_[side];
                }
            }
        }
        pos->z += vel->z * dt;
        // Ground units follow the authoritative heightfield; aircraft and
        // vessels retain their own altitude/draft semantics.
        if (!aircraft && !vessel) {
            pos->z = terrain_.height_at(pos->x, pos->y);
        }
        if (auto* off_road = component_manager_.get_component<OffRoadWear>(entity_id)) {
            const float distance_traveled = std::hypot(pos->x - previous_x, pos->y - previous_y);
            if (distance_traveled > 0.0f) {
                const bool on_road = navigation.traversal_cost(
                    navigation.to_grid_x(pos->x), navigation.to_grid_y(pos->y)) < 0.99f;
                const auto& settings = get_off_road_settings();
                const float terrain_factor = on_road ? settings.road_wear_multiplier : 1.0f;
                const float unit_factor = off_road_unit_factor(off_road->unit_type);
                off_road->distance_traveled += distance_traveled;
                off_road->accumulated += distance_traveled * settings.wear_per_meter * terrain_factor *
                    (road_marching ? 1.0f / movement::kRoadMarchSpeedMultiplier : 1.0f) * unit_factor;
                if (auto* material = component_manager_.get_component<Material>(entity_id)) {
                    material->current = std::max(0.0f, material->current -
                        distance_traveled * settings.material_per_meter * terrain_factor * unit_factor);
                }
            }
        }
        if (auto* aircraft = component_manager_.get_component<Aircraft>(entity_id)) {
            aircraft->x = pos->x;
            aircraft->y = pos->y;
        }
        spatial_grid_.update(entity_id, pos->x, pos->y);
        if (vessel) { vessel->x = pos->x; vessel->y = pos->y; }
        if (component_manager_.get_component<Carrier>(entity_id)) {
            logistics_manager_.update_recovery_facility_position(entity_id, pos->x, pos->y);
        }
        auto patrol = patrol_orders_.find(entity_id);
        if (patrol != patrol_orders_.end() && move_targets_.find(entity_id) == move_targets_.end()) {
            patrol->second.returning = !patrol->second.returning;
            const auto target_position = patrol->second.returning ? patrol->second.origin : patrol->second.destination;
            move_unit(entity_id, target_position.x, target_position.y);
        }
    }
    separate_ground_units();
}

void Simulation::complete_move(EntityId entity) {
    if (auto* vel = component_manager_.get_component<Velocity>(entity)) {
        vel->x = 0.0f;
        vel->y = 0.0f;
    }
    if (auto* steering = component_manager_.get_component<GroundSteering>(entity)) {
        steering->current_speed = 0.0f;
        if (steering->yielding) {
            // Stepping aside is not where the unit was sent: its settled goal,
            // which crowded arrival looks at, stays what it was.
            steering->yielding = false;
            steering->yield_ticks_left = 0;
            steering->yield_cooldown = movement::kYieldCooldownTicks;
        } else if (const auto target = move_targets_.find(entity); target != move_targets_.end()) {
            steering->has_settled_goal = true;
            steering->settled_goal_x = target->second.arrival.x;
            steering->settled_goal_y = target->second.arrival.y;
        }
    }
    move_targets_.erase(entity);
    // Drive the next queued leg. A waypoint that is no longer
    // walkable is skipped rather than ending the queue.
    if (auto queue = queued_moves_.find(entity); queue != queued_moves_.end()) {
        while (!queue->second.empty()) {
            const MoveTarget next = queue->second.front();
            queue->second.pop_front();
            if (assign_move_target(entity, next.arrival.x, next.arrival.y,
                                   next.strategic_route.x, next.strategic_route.y)) break;
        }
        if (queue->second.empty()) queued_moves_.erase(queue);
    }
}

void Simulation::separate_ground_units() {
    // Ground vehicles only: aircraft and vessels have no GroundSteering.
    // Overlap is looked for in a per-tick hash grid sized to the largest hull.
    // A stopped unit that is overlapped drives to a free spot, and back once
    // its old spot is clear; hulls are only pushed apart when no spot is
    // clear, or for two moving units deeply overlapped. Pushes are computed
    // from the same positions and then applied in entity order, so the result
    // does not depend on the order pairs are visited and stays deterministic.
    struct Body {
        EntityId id;
        Position* pos;
        float radius;
        const MoveTarget* target;
        GroundSteering* steering;
    };
    std::vector<Body> bodies;
    float max_radius = 0.0f;
    for (const auto id : component_manager_.entities_with<Position, GroundSteering>(entity_manager_.get_entities())) {
        const auto* health = component_manager_.get_component<Health>(id);
        if (health && (health->is_dead || health->current <= 0.0f)) continue;
        auto* pos = component_manager_.get_component<Position>(id);
        auto* steering = component_manager_.get_component<GroundSteering>(id);
        if (!pos || !steering || !std::isfinite(pos->x) || !std::isfinite(pos->y)) continue;
        const auto target = move_targets_.find(id);
        // A yield ends when its move does: it arrived, was stopped, or timed out.
        if (steering->yield_cooldown > 0) --steering->yield_cooldown;
        if (steering->yield_retry > 0) --steering->yield_retry;
        if (steering->yielding) {
            if (target == move_targets_.end()) {
                steering->yielding = false;
                steering->yield_ticks_left = 0;
                steering->yield_cooldown = movement::kYieldCooldownTicks;
            } else if (steering->yield_ticks_left == 0 || --steering->yield_ticks_left == 0) {
                complete_move(id);
                continue;
            }
        }
        bodies.push_back({id, pos, steering->footprint_radius,
                          target == move_targets_.end() ? nullptr : &target->second, steering});
        max_radius = std::max(max_radius, steering->footprint_radius);
    }
    if (bodies.size() < 2 || max_radius <= 0.0f) return;

    const float cell = 2.0f * max_radius * movement::kSeparationContactSlack;
    const auto cell_of = [cell](float v) { return static_cast<int32_t>(std::floor(v / cell)); };
    const auto key_of = [](int32_t cx, int32_t cy) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint32_t>(cy);
    };
    std::unordered_map<uint64_t, std::vector<uint32_t>> buckets;
    buckets.reserve(bodies.size());
    for (uint32_t index = 0; index < bodies.size(); ++index) {
        buckets[key_of(cell_of(bodies[index].pos->x), cell_of(bodies[index].pos->y))].push_back(index);
    }

    // Spots units are driving to, whether sent there or stepping aside, so a
    // unit making room does not park where another is about to stop. Kept in
    // the same kind of grid as the bodies and built only when a unit needs to
    // find a spot, which most ticks none does.
    struct Spot { float x, y, radius; };
    std::unordered_map<uint64_t, std::vector<Spot>> reserved;
    bool reserved_built = false;
    const auto reserve = [&](float x, float y, float radius) {
        reserved[key_of(cell_of(x), cell_of(y))].push_back({x, y, radius});
    };
    const auto ensure_reserved = [&]() {
        if (reserved_built) return;
        reserved_built = true;
        reserved.reserve(bodies.size());
        for (const auto& body : bodies) {
            if (body.target) reserve(body.target->arrival.x, body.target->arrival.y, body.radius);
        }
    };
    const auto walkable = [this](float x, float y) {
        return pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y)) && is_land_position(x, y);
    };
    // Whether a hull of this radius fits at a point: no other hull, nor spot
    // someone is driving to, within `slack` times contact. Returning home asks
    // only for contact, so a spot where the unit stood touching a neighbour is
    // still its own.
    const auto clear_of_others = [&](uint32_t self, float x, float y, float radius, float slack) {
        ensure_reserved();
        const int32_t px = cell_of(x), py = cell_of(y);
        size_t examined = 0;
        for (int32_t ox = -1; ox <= 1; ++ox) {
            for (int32_t oy = -1; oy <= 1; ++oy) {
                const auto held = reserved.find(key_of(px + ox, py + oy));
                if (held != reserved.end()) {
                    for (const auto& spot : held->second) {
                        const float gap = (radius + spot.radius) * slack;
                        if (std::hypot(x - spot.x, y - spot.y) < gap) return false;
                    }
                }
                const auto bucket = buckets.find(key_of(px + ox, py + oy));
                if (bucket == buckets.end()) continue;
                for (const uint32_t k : bucket->second) {
                    // So crowded that the spot cannot be vouched for: not clear.
                    if (++examined > movement::kSeparationMaxNeighbors) return false;
                    if (k == self) continue;
                    const Body& other = bodies[k];
                    const float gap = (radius + other.radius) * slack;
                    if (std::hypot(x - other.pos->x, y - other.pos->y) < gap) return false;
                }
            }
        }
        return true;
    };

    // Units that stepped aside drive back once their old spot is clear.
    for (uint32_t i = 0; i < bodies.size(); ++i) {
        Body& body = bodies[i];
        GroundSteering& gs = *body.steering;
        if (!gs.yield_home_valid) continue;
        if (body.target && !gs.yielding) {
            gs.yield_home_valid = false; // given another order meanwhile
            continue;
        }
        if (body.target || gs.yield_cooldown > 0) continue; // still driving, or just finished
        if (std::hypot(body.pos->x - gs.yield_home_x, body.pos->y - gs.yield_home_y) <= movement::kYieldHomeArrivalMeters) {
            gs.yield_home_valid = false;
            continue;
        }
        if (gs.yield_home_wait == 0 || --gs.yield_home_wait == 0) {
            gs.yield_home_valid = false;
            continue;
        }
        const float hx = gs.yield_home_x, hy = gs.yield_home_y;
        if (!walkable(hx, hy) || !walkable(0.5f * (hx + body.pos->x), 0.5f * (hy + body.pos->y)) ||
            !clear_of_others(i, hx, hy, body.radius, 1.0f)) continue;
        if (assign_move_target(body.id, hx, hy, hx, hy)) {
            gs.yielding = true;
            gs.yield_ticks_left = movement::kYieldMaxTicks;
            body.target = &move_targets_.find(body.id)->second;
            reserve(hx, hy, body.radius);
        }
    }

    std::vector<float> push_x(bodies.size(), 0.0f), push_y(bodies.size(), 0.0f);
    std::vector<uint8_t> arrived(bodies.size(), 0);
    // A neighbour that overlaps a stopped unit enough to ask it to make room.
    struct YieldRequest {
        uint32_t yielder;
        uint32_t other;
        float away_x, away_y; // unit vector from the other unit towards the yielder
        float overlap;        // contact - distance
        bool other_moving;    // a passer-by, so the yielder's own spot is worth returning to
    };
    // The strongest overlap each stopped unit has been asked to make room for.
    // One per unit: a crowd has thousands of overlapping pairs and a unit only
    // needs one reason to move.
    std::vector<YieldRequest> request_of(bodies.size());
    std::vector<uint8_t> has_request(bodies.size(), 0);
    std::vector<uint32_t> requested;
    const auto record_request = [&](const YieldRequest& request) {
        if (!has_request[request.yielder]) {
            has_request[request.yielder] = 1;
            requested.push_back(request.yielder);
            request_of[request.yielder] = request;
            return;
        }
        auto& held = request_of[request.yielder];
        if (request.overlap > held.overlap || (request.overlap == held.overlap && request.other < held.other)) held = request;
    };
    const auto occupies_destination = [](const Body& mover, const Body& other, float contact) {
        if (!mover.target || mover.steering->yielding) return false;
        const float tx = mover.target->arrival.x, ty = mover.target->arrival.y;
        // A unit sitting on the destination that is stopped, or is bound for
        // the same spot. A unit only driving through it does not count.
        const float other_to_goal = std::hypot(other.pos->x - tx, other.pos->y - ty);
        const float on_spot = contact * movement::kSeparationOnSpotFraction;
        if (other_to_goal < on_spot &&
            (!other.target ||
             std::hypot(other.target->arrival.x - tx, other.target->arrival.y - ty) < on_spot)) return true;
        // A stopped unit that was sent to the same spot: the crowd around
        // that spot has reached it, so join the crowd's edge.
        if (other.target || !other.steering->has_settled_goal) return false;
        if (std::hypot(other.steering->settled_goal_x - tx, other.steering->settled_goal_y - ty) >= on_spot) return false;
        return std::hypot(mover.pos->x - tx, mover.pos->y - ty) <= movement::kCrowdedArrivalMeters;
    };
    const auto can_yield = [](const Body& body) {
        return !body.target && !body.steering->yielding && body.steering->yield_cooldown == 0 &&
               body.steering->yield_retry == 0;
    };
    for (uint32_t i = 0; i < bodies.size(); ++i) {
        const Body& a = bodies[i];
        const int32_t cx = cell_of(a.pos->x), cy = cell_of(a.pos->y);
        size_t examined = 0;
        for (int32_t ox = -1; ox <= 1 && examined < movement::kSeparationMaxNeighbors; ++ox) {
            for (int32_t oy = -1; oy <= 1 && examined < movement::kSeparationMaxNeighbors; ++oy) {
                const auto bucket = buckets.find(key_of(cx + ox, cy + oy));
                if (bucket == buckets.end()) continue;
                // Bucket indices are inserted in ascending body order. Skip
                // earlier pairs without repeatedly scanning a crowded cell.
                const auto first = std::upper_bound(bucket->second.begin(), bucket->second.end(), i);
                for (auto entry = first; entry != bucket->second.end() &&
                                    examined < movement::kSeparationMaxNeighbors; ++entry) {
                    ++examined;
                    const uint32_t j = *entry;
                    const Body& b = bodies[j];
                    const float dx = b.pos->x - a.pos->x;
                    const float dy = b.pos->y - a.pos->y;
                    const float contact = a.radius + b.radius;
                    const float touching = contact * movement::kSeparationContactSlack;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 >= touching * touching) continue;
                    if (occupies_destination(a, b, contact)) arrived[i] = 1;
                    if (occupies_destination(b, a, contact)) arrived[j] = 1;
                    if (d2 >= contact * contact) continue;
                    float nx, ny;
                    const float d = std::sqrt(d2);
                    if (d < 1e-3f) {
                        // Exactly stacked: split along a direction fixed by the pair's ids.
                        const uint32_t h = (a.id * 2654435761u) ^ (b.id * 40503u + 0x9e3779b9u);
                        const float angle = static_cast<float>(h % 3600u) * (2.0f * std::numbers::pi_v<float> / 3600.0f);
                        nx = std::cos(angle);
                        ny = std::sin(angle);
                    } else {
                        nx = dx / d;
                        ny = dy / d;
                    }
                    // nx, ny point from a towards b. A unit already driving clear
                    // of the overlap needs nothing more from its neighbour.
                    if (a.steering->yielding || b.steering->yielding) continue;
                    if (d < contact * movement::kYieldOverlapFraction) {
                        // The stopped one gives way to a mover; of two stopped
                        // units, the higher id.
                        int yielder = -1;
                        if (can_yield(a) && can_yield(b)) yielder = a.id > b.id ? 0 : 1;
                        else if (can_yield(a) && b.target) yielder = 0;
                        else if (can_yield(b) && a.target) yielder = 1;
                        else if (can_yield(a) && !b.target) yielder = 0;
                        else if (can_yield(b) && !a.target) yielder = 1;
                        if (yielder >= 0) {
                            record_request(yielder == 0
                                ? YieldRequest{i, j, -nx, -ny, contact - d, b.target != nullptr}
                                : YieldRequest{j, i, nx, ny, contact - d, a.target != nullptr});
                            continue;
                        }
                        // Nobody can drive clear. A unit whose search just found
                        // no free spot is forced apart for now; one that is
                        // only waiting out a cooldown is left alone.
                        if (a.steering->yield_retry == 0 && b.steering->yield_retry == 0) continue;
                    } else if (a.steering->yield_retry == 0 && b.steering->yield_retry == 0 &&
                               d >= contact * movement::kSeparationDeepFraction) {
                        continue;
                    }
                    // Hulls are only forced apart for two units that cannot
                    // drive clear of each other and are deeply overlapped.
                    const float share_a = (a.target && !b.target) ? movement::kSeparationMoverShare
                        : (!a.target && b.target) ? 1.0f - movement::kSeparationMoverShare : 0.5f;
                    const float push = (contact - d) * movement::kSeparationResolveFraction;
                    push_x[i] -= nx * push * share_a;
                    push_y[i] -= ny * push * share_a;
                    push_x[j] += nx * push * (1.0f - share_a);
                    push_y[j] += ny * push * (1.0f - share_a);
                }
            }
        }
    }

    // Stopped units that were overlapped drive to a free spot, lowest unit
    // first, up to kYieldMaxPerTick of them a tick.
    if (!requested.empty()) {
        std::sort(requested.begin(), requested.end());
        // The directions candidate spots are tried in, as unit vectors rotated
        // from the way the neighbour pushes the unit.
        static const std::array<std::array<float, 2>, movement::kYieldDirections> kTurns = [] {
            std::array<std::array<float, 2>, movement::kYieldDirections> table{};
            for (int step = 0; step < movement::kYieldDirections; ++step) {
                const float angle = static_cast<float>(step) * (2.0f * std::numbers::pi_v<float> / movement::kYieldDirections);
                table[static_cast<size_t>(step)] = {std::cos(angle), std::sin(angle)};
            }
            return table;
        }();
        size_t started = 0;
        for (const uint32_t yielder : requested) {
            if (started >= movement::kYieldMaxPerTick) break;
            const YieldRequest& request = request_of[yielder];
            Body& mover = bodies[yielder];
            const Body& other = bodies[request.other];
            if (!can_yield(mover)) continue;
            const float forward_x = std::sin(mover.steering->heading), forward_y = std::cos(mover.steering->heading);
            const bool can_reverse = !mover.steering->can_pivot_turn && mover.steering->max_reverse_speed > 0.0f;
            const float reverse_cosine = std::cos(mover.steering->reverse_preference_threshold);
            bool found = false;
            float best_cost = std::numeric_limits<float>::max(), best_x = 0.0f, best_y = 0.0f;
            const float touch = (mover.radius + other.radius) * movement::kSeparationContactSlack + movement::kYieldMarginMeters;
            for (int ring = 0; ring < movement::kYieldRings && !found; ++ring) {
                const float reach = touch + static_cast<float>(ring) * mover.radius;
                for (const auto& turn : kTurns) {
                    const float ux = request.away_x * turn[0] - request.away_y * turn[1];
                    const float uy = request.away_x * turn[1] + request.away_y * turn[0];
                    const float x = other.pos->x + ux * reach;
                    const float y = other.pos->y + uy * reach;
                    const float dx = x - mover.pos->x, dy = y - mover.pos->y;
                    const float travel = std::sqrt(dx * dx + dy * dy);
                    if (travel < 0.5f) continue;
                    const float aligned = (dx * forward_x + dy * forward_y) / travel; // 1 ahead, -1 behind
                    float cost = travel + (1.0f - aligned) * movement::kYieldTurnPenaltyMeters;
                    if (can_reverse && aligned <= reverse_cosine) cost += movement::kYieldReversePenaltyMeters;
                    if (cost >= best_cost) continue;
                    if (!walkable(x, y) || !walkable(0.5f * (x + mover.pos->x), 0.5f * (y + mover.pos->y))) continue;
                    if (!clear_of_others(yielder, x, y, mover.radius, movement::kSeparationContactSlack)) continue;
                    best_cost = cost;
                    best_x = x;
                    best_y = y;
                    found = true;
                }
            }
            if (found) {
                if (assign_move_target(mover.id, best_x, best_y, best_x, best_y)) {
                    GroundSteering& gs = *mover.steering;
                    // Home is the spot the unit was sent to when it is standing
                    // on it, so a unit shouldered aside several times does not
                    // drift by its stopping tolerance each time.
                    const bool on_goal = gs.has_settled_goal &&
                        std::hypot(gs.settled_goal_x - mover.pos->x, gs.settled_goal_y - mover.pos->y) <= movement::kYieldGoalSnapMeters;
                    const float home_x = on_goal ? gs.settled_goal_x : mover.pos->x;
                    const float home_y = on_goal ? gs.settled_goal_y : mover.pos->y;
                    gs.yielding = true;
                    gs.yield_ticks_left = movement::kYieldMaxTicks;
                    // Only worth returning to if what overlapped it was passing
                    // by: a unit that was stopped on top of another has no
                    // spot of its own to go back to.
                    if (!gs.yield_home_valid && request.other_moving) {
                        gs.yield_home_valid = true;
                        gs.yield_home_x = home_x;
                        gs.yield_home_y = home_y;
                        gs.yield_home_wait = movement::kYieldReturnWaitTicks;
                    }
                    mover.target = &move_targets_.find(mover.id)->second;
                    reserve(best_x, best_y, mover.radius);
                    ++started;
                }
            } else {
                // Nowhere clear to drive (a packed crowd): force this pair of
                // hulls apart now, and for the next few ticks, as a last resort.
                mover.steering->yield_retry = movement::kYieldRetryTicks;
                const float push = request.overlap * movement::kSeparationResolveFraction * 0.5f;
                push_x[yielder] += request.away_x * push;
                push_y[yielder] += request.away_y * push;
                push_x[request.other] -= request.away_x * push;
                push_y[request.other] -= request.away_y * push;
            }
        }
    }

    for (uint32_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        if (arrived[i] && body.target) complete_move(body.id);
        float px = push_x[i], py = push_y[i];
        const float length = std::hypot(px, py);
        if (length < 1e-5f) continue;
        if (length > movement::kSeparationMaxPushMeters) {
            px *= movement::kSeparationMaxPushMeters / length;
            py *= movement::kSeparationMaxPushMeters / length;
        }
        // Never push a unit from open ground into a blocked or water cell;
        // slide along the obstacle when one axis is still open.
        const auto open = [this](float x, float y) {
            return pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y));
        };
        const float x = body.pos->x, y = body.pos->y;
        float nx = x + px, ny = y + py;
        if (open(x, y) && !open(nx, ny)) {
            if (open(x + px, y)) ny = y;
            else if (open(x, y + py)) nx = x;
            else continue;
        }
        body.pos->x = nx;
        body.pos->y = ny;
        body.pos->z = terrain_.height_at(nx, ny);
        spatial_grid_.update(body.id, nx, ny);
    }
}

void Simulation::record_combat_event(CombatEventKind kind, EntityId entity, float vx, float vy) {
    const auto* position = component_manager_.get_component<Position>(entity);
    const auto* owner = component_manager_.get_component<Faction>(entity);
    const auto* data = component_manager_.get_component<UnitData>(entity);
    if (!position || !owner) return;
    uint8_t audience = 0;
    std::array<uint64_t, 3> sequences{};
    for (size_t index = 0; index < FACTION_COUNT; ++index) {
        const auto viewer = static_cast<FactionId>(index);
        // The owner always receives its own event. Avoid a visibility lookup
        // for that side, especially in unconfigured benchmark matches where
        // the fallback sight test scans live sensors.
        if (owner->faction_id == viewer ||
            (vision_active_ ? is_watched(viewer, entity, position->x, position->y)
                            : is_position_visible_to(viewer, position->x, position->y))) {
            audience |= uint8_t(1u << index);
            sequences[index] = ++combat_event_viewer_sequences_[index];
        }
    }
    if (combat_events_.size() == MAX_COMBAT_EVENTS) combat_events_.pop_front();
    combat_events_.push_back({++combat_event_sequence_, tick_, kind, entity,
        data ? data->unit_type : -1, static_cast<int>(owner->faction_id),
        position->x, position->y, vx, vy, is_airborne(entity), audience, sequences});
}

std::vector<CombatPresentationEvent> Simulation::combat_events_after(uint64_t cursor, FactionId viewer) const {
    std::vector<CombatPresentationEvent> result;
    const auto faction = static_cast<unsigned>(viewer);
    if (faction >= FACTION_COUNT) return result;
    for (const auto& event : combat_events_) {
        if (event.viewer_sequences[faction] > cursor && (event.audience & (1u << faction))) {
            auto visible = event;
            visible.sequence = event.viewer_sequences[faction];
            result.push_back(visible);
        }
    }
    return result;
}

void Simulation::record_radio_event(RadioEventKind kind, FactionId listener, float x, float y,
                                    int count, EntityId subject) {
    const auto index = static_cast<size_t>(listener);
    if (index >= FACTION_COUNT) return;
    RadioEvent event{++radio_event_sequences_[index], tick_, kind, listener, x, y};
    for (const EntityId post : {match_player_post_, match_enemy_post_}) {
        const auto* owner = component_manager_.get_component<Faction>(post);
        const auto* position = component_manager_.get_component<Position>(post);
        if (!owner || !position || owner->faction_id != listener) continue;
        event.has_reference = true;
        event.reference_x = position->x;
        event.reference_y = position->y;
    }
    event.count = count;
    event.subject = subject;
    auto& events = radio_events_[index];
    if (events.size() == MAX_RADIO_EVENTS) events.pop_front();
    events.push_back(event);
}

std::vector<RadioEvent> Simulation::radio_events_after(uint64_t cursor, FactionId listener) const {
    std::vector<RadioEvent> result;
    const auto index = static_cast<size_t>(listener);
    if (index >= FACTION_COUNT) return result;
    for (const auto& event : radio_events_[index]) {
        if (event.sequence > cursor) result.push_back(event);
    }
    return result;
}

// Runs on the vision refresh, over aircraft only. A side is told once when
// hostile aircraft enter its own sight, however many there are.
void Simulation::update_air_threat_reports() {
    std::array<int, FACTION_COUNT> counts{};
    std::array<float, FACTION_COUNT> sum_x{}, sum_y{};
    for (const auto entity : component_manager_.entities_with<Aircraft, Position, Faction>(entity_manager_.get_entities())) {
        if (!is_airborne(entity) || get_unit_is_dead(entity)) continue;
        const auto* position = component_manager_.get_component<Position>(entity);
        const auto owner = component_manager_.get_component<Faction>(entity)->faction_id;
        for (size_t side = 0; side < FACTION_COUNT; ++side) {
            const auto viewer = static_cast<FactionId>(side);
            if (viewer == owner || !vision_map_.visible_in_sky(viewer, position->x, position->y)) continue;
            ++counts[side];
            sum_x[side] += position->x;
            sum_y[side] += position->y;
        }
    }
    for (size_t side = 0; side < FACTION_COUNT; ++side) {
        auto& threat = air_threats_[side];
        if (counts[side] > 0) {
            if (!threat.active) {
                record_radio_event(RadioEventKind::AIR_HOSTILE_INBOUND, static_cast<FactionId>(side),
                                   sum_x[side] / counts[side], sum_y[side] / counts[side], counts[side]);
            }
            threat.active = true;
            threat.last_seen_tick = tick_;
        } else if (threat.active && tick_ - threat.last_seen_tick >= kAirThreatClearTicks) {
            threat.active = false;
        }
    }
}

void Simulation::combat_phase(float delta_ms) {
    if (!vision_active_) {
        for (auto& observers : combat_observers_) observers.clear();
        for (const auto id : entity_manager_.get_entities()) {
            const auto* owner = component_manager_.get_component<Faction>(id);
            const auto* health = component_manager_.get_component<Health>(id);
            const auto* sensor = component_manager_.get_component<UnitData>(id);
            const auto* weapon = component_manager_.get_component<Weapon>(id);
            const auto* position = component_manager_.get_component<Position>(id);
            if (!owner || !health || health->is_dead || health->current <= 0 || !position ||
                (!sensor && !weapon)) continue;
            const size_t index = static_cast<size_t>(owner->faction_id);
            if (index < combat_observers_.size()) combat_observers_[index].push_back(id);
        }
        combat_observers_active_ = true;
    }
    combat_manager_.update(delta_ms, spatial_grid_, component_manager_);
    combat_observers_active_ = false;
    const auto entities = entity_manager_.get_entities();
    for (auto id : entities) {
        const auto* health = component_manager_.get_component<Health>(id);
        if (health && (health->is_dead || health->current <= 0)) destroy_unit(id);
    }
}

void Simulation::logistics_phase(float delta_ms) {
    const auto aircraft = component_manager_.entities_with<Aircraft>(entity_manager_.get_entities());
    // Return orders transfer motion to recovery guidance until touchdown.
    for (auto id : aircraft) {
        auto* plane=component_manager_.get_component<Aircraft>(id);
        if (!plane || plane->status != Aircraft::Status::RETURNING) continue;
        const auto* facility=component_manager_.get_component<RecoveryFacility>(plane->target_base);
        if (!facility || get_unit_is_dead(plane->target_base)) continue;
        const float dx=facility->x-plane->x, dy=facility->y-plane->y;
        const float distance=std::hypot(dx,dy);
        const float step=std::min(distance,plane->cruise_speed*delta_ms/1000.0f);
        if (distance>0 && step>0) { plane->x+=dx/distance*step; plane->y+=dy/distance*step; }
        logistics_manager_.queue_aircraft_for_landing(id,plane->target_base);
    }
    logistics_manager_.update_all(delta_ms);
    logistics_manager_.batch_safe_return_check(aircraft, pathfinding_);
    for (auto id : aircraft) {
        auto* plane = component_manager_.get_component<Aircraft>(id);
        auto* position = component_manager_.get_component<Position>(id);
        if (plane && position && plane->status != Aircraft::Status::AIRBORNE) {
            position->x = plane->x; position->y = plane->y;
            spatial_grid_.update(id, position->x, position->y);
        }
    }

    std::vector<EntityId> crashed;
    for (EntityId entity_id : aircraft) {
        auto* aircraft_component = component_manager_.get_component<Aircraft>(entity_id);
        if (aircraft_component && aircraft_component->status == Aircraft::Status::CRASHED) {
            crashed.push_back(entity_id);
        }
    }
    for (EntityId entity_id : crashed) {
        destroy_unit(entity_id);
    }
}

void Simulation::economy_phase(float delta_ms) {
     refresh_unit_counts();
     for (size_t index = 0; index < FACTION_COUNT; ++index) {
         const auto faction = static_cast<FactionId>(index);
         // An inbound delivery was paid for, so production must leave it a slot.
         const int reserved = (reinforcement_delivery_.state() == ReinforcementDeliveryState::INCOMING &&
             reinforcement_delivery_.owner() == faction ? 1 : 0) + inbound_count(faction);
         production_manager_.set_unit_slots(faction, unit_cap_per_player_ - unit_counts_[index] - reserved);
     }
     production_manager_.update_all(delta_ms);
     requisition_manager_.update(delta_ms, production_manager_, *this);
     update_harvesters(delta_ms);
    
    // Spawn units for completed constructions
    auto& completed = production_manager_.get_completed_constructions();
    for (size_t index = 0; index < completed.size(); ++index) {
        const auto& comp = completed[index];
        if (comp.is_structure && comp.structure_type == EARTHWORKS_JOB) {
            for (const auto& claim : site_claims_) {
                if (claim.x == comp.x && claim.y == comp.y) {
                    terrain_.flatten_rect(claim.x, claim.y, claim.half_x, claim.half_y, claim.pad_height);
                    break;
                }
            }
            continue;
        }
        if (comp.is_structure) {
			// Block the same footprint placement claimed, then move out any ground
			// unit standing on it: pathfinding cannot start from a blocked cell.
			const StructureSiteRules rules = structure_site_rules(comp.structure_type);
			const float half_width = rules.footprint_x > 0.0f ? rules.footprint_x * 0.5f : 30.0f;
			const float half_height = rules.footprint_y > 0.0f ? rules.footprint_y * 0.5f : 30.0f;
			pathfinding_.block_world_rectangle(comp.x, comp.y, half_width, half_height);
			evict_ground_units(comp.x, comp.y, half_width, half_height);
			vision_sources_.push_back({comp.faction_id, comp.x, comp.y, structure_view_range(comp.structure_type), structure_eye_height(comp.structure_type)});
			if (power_grid_.active())
				power_grid_.add_structure_demand(static_cast<int>(comp.faction_id), structure_power_demand(comp.structure_type));
            if (comp.structure_type == 2)
                territorial_control_.add_installation(comp.x, comp.y, InstallationType::AIRFIELD, comp.faction_id);
            continue;
        }
        // Factories report their storage position, but a unit spawned at that
        // exact point would be hidden inside the commander model.  Use a
        // deterministic rally line so completed units are immediately visible
        // and remain separated when several jobs finish on one tick.
        const auto proto = get_unit_prototypes().find(comp.unit_type);
        const bool arriving_aircraft = proto != get_unit_prototypes().end() && proto->second.is_aircraft && proto->second.requires_runway;
        const bool naval_unit = proto != get_unit_prototypes().end() && proto->second.is_naval;
        float spawn_x = comp.x;
        float spawn_y = comp.y;
        if (theater_water_rules_enabled_ && !arriving_aircraft && !naval_unit && !is_land_position(spawn_x, spawn_y)) {
            const auto line_id = production_manager_.faction_line(comp.faction_id);
            const auto line_it = production_manager_.production_lines().find(line_id);
            if (line_it != production_manager_.production_lines().end()) {
                const auto storage_it = production_manager_.storages().find(line_it->second.storage_id);
                if (storage_it != production_manager_.storages().end()) {
                    spawn_x = storage_it->second.x;
                    spawn_y = storage_it->second.y;
                }
            }
        }
        const float rally_x = arriving_aircraft ? pathfinding_.min_world_x_center() - pathfinding_.cell_size() : naval_unit ? spawn_x : spawn_x + 5.0f + static_cast<float>(index) * 3.0f;
        // Arriving winged aircraft enter well north/south of their runway,
        // already flying east. Recon flights retain their existing ingress.
        const bool runway_ferry = arriving_aircraft && proto->second.recon_loiter_seconds <= 0.0f;
        if (runway_ferry) {
            float ax = comp.x, ay = comp.y;
            if (territorial_control_.get_active_installation_position(comp.faction_id, InstallationType::AIRFIELD, ax, ay))
                spawn_y = ay + (ay >= 0.0f ? 4500.0f : -4500.0f);
        }
        const auto spawned_id = create_unit_with_type(rally_x, spawn_y, comp.unit_type, comp.faction_id);
        if (arriving_aircraft && spawned_id >= 0) {
            // This aircraft was manufactured at an external strategic airfield
            // and paid an operational ferry cost. It therefore enters the
            // tactical simulation already airborne; routing it through a local
            // takeoff queue strands it beyond the map boundary it must cross.
            if (auto* aircraft = component_manager_.get_component<Aircraft>(static_cast<EntityId>(spawned_id))) {
                aircraft->status = Aircraft::Status::AIRBORNE;
                aircraft->mission = Aircraft::Mission::PATROL;
                if (proto->second.recon_loiter_seconds > 0.0f) {
                    // The flight plan covers the whole trip, so there is no base
                    // to return to and no airborne limit to crash against.
                    aircraft->mission = Aircraft::Mission::RECON;
                    aircraft->max_airborne_time_ms = 0.0f;
                    aircraft->fuel_consumption_rate = 0.0f;
                    aircraft->material_consumption_rate = 0.0f;
                }
            }
            const bool flight_plan = proto->second.recon_loiter_seconds > 0.0f;
            float destination_x = flight_plan ? std::clamp(comp.x, pathfinding_.min_world_x_center(), pathfinding_.max_world_x_center()) : comp.x;
            float destination_y = flight_plan ? std::clamp(comp.y, pathfinding_.min_world_y_center(), pathfinding_.max_world_y_center()) : comp.y;
            if (!flight_plan) {
                // A ferried aircraft flies to its own airfield and lands there.
                territorial_control_.get_active_installation_position(comp.faction_id, InstallationType::AIRFIELD, destination_x, destination_y);
                auto& flight = ferry_flights_[static_cast<EntityId>(spawned_id)];
                flight = {};
                flight.airfield_x = destination_x;
                flight.airfield_y = destination_y;
                flight.approach.emplace(rally_x, spawn_y, std::numbers::pi_v<float>*0.5f,
                                        destination_x, destination_y, proto->second.speed);
            }
            if (flight_plan) {
                recon_flights_[static_cast<EntityId>(spawned_id)] = {destination_x, destination_y, proto->second.recon_loiter_seconds * 1000.0f};
            }
            move_unit(static_cast<EntityId>(spawned_id), destination_x, destination_y);
        }
    }
    production_manager_.clear_completed_constructions();
    deliver_inbound_forces();
}

void Simulation::update_harvesters(float delta_ms) {
    auto harvesters = component_manager_.entities_with<Harvester>(entity_manager_.get_entities());
    
    for (auto entity_id : harvesters) {
        auto* harvester = component_manager_.get_component<Harvester>(entity_id);
        if (!harvester) continue;
        
        production_manager_.extract_resource(harvester->node_id, delta_ms);
    }
}

void Simulation::environment_phase(float delta_ms) {
    // Resource regeneration, terrain updates, territorial control progression
    for (const auto& completed_road : road_network_.update(delta_ms)) {
        apply_completed_road(completed_road);
    }
    territorial_control_.update(50.0f);
    if (control_point_battle_.active()) {
        std::vector<ControlPointUnitPresence> unit_presence;
        const auto entities = component_manager_.entities_with<Position, Faction, GroundSteering>(entity_manager_.get_entities());
        unit_presence.reserve(entities.size());
        for (const auto entity : entities) {
            const auto* position = component_manager_.get_component<Position>(entity);
            const auto* faction = component_manager_.get_component<Faction>(entity);
            const auto* health = component_manager_.get_component<Health>(entity);
            if (position && faction && (!health || !health->is_dead)) {
                unit_presence.push_back({faction->faction_id, position->x, position->y});
            }
        }
        control_point_battle_.update(delta_ms, unit_presence);
    }
    if (reinforcement_delivery_.update(delta_ms)) {
        const auto& package = reinforcement_delivery_.package();
        // Aircraft land at the selected airfield. Ground cargo deploys at a
        // deterministic egress point just beyond its runway footprint.
        float deploy_x = reinforcement_delivery_.selected_x();
        float deploy_y = reinforcement_delivery_.selected_y() + 1000.0f;
        if (!pathfinding_.is_walkable(pathfinding_.to_grid_x(deploy_x), pathfinding_.to_grid_y(deploy_y))) {
            deploy_y = reinforcement_delivery_.selected_y() - 1000.0f;
        }
        create_unit_with_type(deploy_x, deploy_y, package.unit_type, reinforcement_delivery_.owner());
    }
    update_fuel_supply(delta_ms);
    update_power_grid(delta_ms);
    update_recon_flights(delta_ms);
    update_ferry_flights(delta_ms);
    update_air_discipline();
    update_match_result();
    if (vision_active_ && tick_ % kVisionRefreshTicks == 0) refresh_vision();
}

void Simulation::begin_match(EntityId player_post, EntityId enemy_post) {
    match_player_post_ = player_post;
    match_enemy_post_ = enemy_post;
    match_result_ = -1;
    match_victory_reason_ = 0;
    key_site_victory_sites_.clear();
    key_sites_required_ = 0;
    key_site_hold_ticks_required_ = 0;
    key_sites_held_.fill(0);
    key_site_hold_ticks_.fill(0);
    if (const auto* owner = component_manager_.get_component<Faction>(player_post)) match_player_faction_ = owner->faction_id;
    if (const auto* owner = component_manager_.get_component<Faction>(enemy_post)) match_enemy_faction_ = owner->faction_id;
    match_start_tick_ = tick_;
    match_record_.begin(tick_, command_position_scale_);
    match_record_.set_info("ai_doctrine", ai_manager_->doctrine().key);
    match_record_.set_info("ai_difficulty", ai_manager_->difficulty().key);
    match_record_.set_info("player_post", std::to_string(player_post));
    match_record_.set_info("enemy_post", std::to_string(enemy_post));
    for (const EntityId post : {player_post, enemy_post}) {
        const auto* position = component_manager_.get_component<Position>(post);
        const auto* faction = component_manager_.get_component<Faction>(post);
        if (position && faction) vision_sources_.push_back({faction->faction_id, position->x, position->y, kCommandPostViewRange, kCommandPostEyeHeight});
    }
    vision_map_.configure(world_width_ > 0.0f ? world_width_ : 40000.0f, world_height_ > 0.0f ? world_height_ : 40000.0f);
    vision_map_.sample_terrain([this](float x, float y) { return terrain_.height_at(x, y); });
    intel_map_.configure(world_width_ > 0.0f ? world_width_ : 40000.0f, world_height_ > 0.0f ? world_height_ : 40000.0f);
    intel_map_.sample_terrain([this](float x, float y) { return terrain_.height_at(x, y); });
    vision_terrain_revision_ = terrain_.revision();
    vision_active_ = true;
    combat_manager_.set_sight_check([this](FactionId faction, EntityId target) { return is_visible_to(faction, target); });
    refresh_vision();
}

bool Simulation::configure_key_site_victory(const std::vector<EntityId>& sites, int required, uint32_t hold_ticks) {
    if (match_player_post_ == INVALID_ENTITY || match_result_ != -1 || sites.empty() ||
        required <= 0 || required > static_cast<int>(sites.size()) || hold_ticks == 0 ||
        match_player_faction_ == match_enemy_faction_) return false;
    std::vector<EntityId> unique = sites;
    std::sort(unique.begin(), unique.end());
    if (std::adjacent_find(unique.begin(), unique.end()) != unique.end()) return false;
    for (const auto id : unique) {
        if (!production_manager_.resource_nodes().contains(id)) return false;
    }
    key_site_victory_sites_ = std::move(unique);
    key_sites_required_ = required;
    key_site_hold_ticks_required_ = hold_ticks;
    key_sites_held_.fill(0);
    key_site_hold_ticks_.fill(0);
    match_record_.set_info("key_site_victory", std::to_string(required) + "/" +
        std::to_string(sites.size()) + " for " + std::to_string(hold_ticks) + " ticks");
    return true;
}

bool Simulation::is_airborne(EntityId entity) const {
    const auto* aircraft = component_manager_.get_component<Aircraft>(entity);
    return aircraft && aircraft->status != Aircraft::Status::ON_GROUND && aircraft->status != Aircraft::Status::CRASHED;
}

bool Simulation::is_watched(FactionId faction, EntityId entity, float x, float y) const {
    return is_airborne(entity) ? vision_map_.visible_in_sky(faction, x, y) : vision_map_.visible(faction, x, y);
}

bool Simulation::has_unit_near(FactionId faction, float x, float y, float radius) const {
    for (const auto entity : spatial_grid().query_in_region(x, y, radius)) {
        const auto* owner = component_manager_.get_component<Faction>(entity);
        const auto* position = component_manager_.get_component<Position>(entity);
        if (!owner || owner->faction_id != faction || !position || is_airborne(entity)) continue;
        const float dx = position->x - x, dy = position->y - y;
        if (dx * dx + dy * dy <= radius * radius) return true;
    }
    return false;
}

int Simulation::recon_flights_in_air(FactionId faction) const {
    int flights = 0;
    for (const auto& [plane, flight] : recon_flights_) {
        (void)flight;
        const auto* owner = component_manager_.get_component<Faction>(plane);
        if (owner && owner->faction_id == faction) ++flights;
    }
    return flights;
}

void Simulation::update_recon_flights(float delta_ms) {
    constexpr float kStationRadius = 500.0f;
    constexpr uint32_t kLegTicks = 20 * 8;
    const float map_edge = pathfinding_.min_world_x_center();
    std::vector<EntityId> finished;
    for (auto& [plane, flight] : recon_flights_) {
        const auto* position = component_manager_.get_component<Position>(plane);
        if (!position || get_unit_is_dead(plane)) {
            finished.push_back(plane);
            continue;
        }
        if (flight.leaving) {
            if (position->x <= map_edge + pathfinding_.cell_size()) {
                destroy_unit(plane);
                finished.push_back(plane);
            }
            continue;
        }
        if (!flight.on_station) {
            if (std::hypot(position->x - flight.x, position->y - flight.y) > kStationRadius) continue;
            flight.on_station = true;
            flight.next_leg_tick = tick_;
        }
        flight.loiter_ms -= delta_ms;
        if (flight.loiter_ms <= 0.0f) {
            flight.leaving = true;
            move_unit(plane, map_edge, position->y);
        } else if (tick_ >= flight.next_leg_tick) {
            constexpr std::array<std::array<float, 2>, 4> kLegs{{{1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {0.0f, -1.0f}}};
            const auto& leg = kLegs[static_cast<size_t>(flight.leg++ % 4)];
            move_unit(plane, flight.x + leg[0] * kStationRadius, flight.y + leg[1] * kStationRadius);
            flight.next_leg_tick = tick_ + kLegTicks;
        }
    }
    for (const auto plane : finished) recon_flights_.erase(plane);
}

bool Simulation::nearest_friendly_airfield(FactionId faction, float x, float y, float& ax, float& ay) const {
    return territorial_control_.get_active_installation_position_near(
        faction, InstallationType::AIRFIELD, x, y, std::numeric_limits<float>::max(), ax, ay);
}

bool Simulation::orbit_eligible(EntityId entity) const {
    const auto* aircraft = component_manager_.get_component<Aircraft>(entity);
    return aircraft && aircraft->status == Aircraft::Status::AIRBORNE &&
           aircraft->type == Aircraft::Type::CONVENTIONAL &&
           aircraft->mission != Aircraft::Mission::RECON &&
           !ferry_flights_.contains(entity) && !queued_moves_.contains(entity);
}

bool Simulation::begin_airfield_return(EntityId entity) {
    const auto* aircraft = component_manager_.get_component<Aircraft>(entity);
    const auto* owner = component_manager_.get_component<Faction>(entity);
    const auto* position = component_manager_.get_component<Position>(entity);
    float ax = 0.0f, ay = 0.0f;
    if (!aircraft || !owner || !position || aircraft->status != Aircraft::Status::AIRBORNE ||
        aircraft->type != Aircraft::Type::CONVENTIONAL ||
        !nearest_friendly_airfield(owner->faction_id, position->x, position->y, ax, ay)) return false;
    patrol_orders_.erase(entity);
    queued_moves_.erase(entity);
    ferry_flights_[entity] = {false, true};
    move_unit(entity, ax, ay);
    return true;
}

void Simulation::update_air_discipline() {
    // Send a plane home while it still has the fuel (and airframe hours) to
    // get there, with a margin for the leg it flies before the order lands.
    constexpr float kReserveDistance = 300.0f;
    constexpr float kReserveSeconds = 5.0f;
    constexpr float kReserveFraction = 1.1f;
    for (const auto plane : component_manager_.entities_with<Aircraft>(entity_manager_.get_entities())) {
        const auto* aircraft = component_manager_.get_component<Aircraft>(plane);
        const auto* position = component_manager_.get_component<Position>(plane);
        const auto* owner = component_manager_.get_component<Faction>(plane);
        if (!aircraft || !position || !owner || aircraft->status != Aircraft::Status::AIRBORNE ||
            aircraft->type != Aircraft::Type::CONVENTIONAL || aircraft->mission == Aircraft::Mission::RECON ||
            ferry_flights_.contains(plane) || aircraft->cruise_speed <= 0.0f) continue;
        float ax = 0.0f, ay = 0.0f;
        if (!nearest_friendly_airfield(owner->faction_id, position->x, position->y, ax, ay)) continue;
        // Reserve for the curved approach, straight final and rollout too.
        // A straight-line estimate sent fighters home too late to fly a circuit.
        const float seconds = (std::hypot(ax - position->x, ay - position->y) + kReserveDistance) /
            std::min(aircraft->cruise_speed, 120.0f) + 65.0f;
        const bool low_fuel = aircraft->fuel <= (seconds * kReserveFraction + kReserveSeconds) * aircraft->fuel_consumption_rate;
        const bool low_material = aircraft->material_consumption_rate > 0.0f &&
            aircraft->material <= (seconds * kReserveFraction + kReserveSeconds) * aircraft->material_consumption_rate;
        const bool low_time = aircraft->max_airborne_time_ms > 0.0f &&
            aircraft->max_airborne_time_ms - aircraft->airborne_time_ms <= (seconds * kReserveFraction + kReserveSeconds) * 1000.0f;
        if (low_fuel || low_material || low_time) begin_airfield_return(plane);
    }
}

void Simulation::update_ferry_flights(float delta_ms) {
    std::vector<EntityId> departed;
    const float map_edge = pathfinding_.min_world_x_center();
    std::vector<EntityId> finished;
    for (auto& [plane, flight] : ferry_flights_) {
        auto* position = component_manager_.get_component<Position>(plane);
        auto* aircraft = component_manager_.get_component<Aircraft>(plane);
        const auto* owner = component_manager_.get_component<Faction>(plane);
        if (flight.landed && aircraft && aircraft->status == Aircraft::Status::ON_GROUND && !get_unit_is_dead(plane)) continue;
        if (flight.taxiing && position && aircraft && flight.approach && !get_unit_is_dead(plane)) {
            // Follow the paved runway, end link, parallel taxiway and apron.
            // A slot is reserved until this aircraft takes off or is destroyed.
            const float side = flight.approach->pose.heading < 1.0f ? 1.0f : -1.0f;
            const float runway_x = flight.airfield_x + kAirfieldRunwayOffsetX;
            const float slot_y = flight.airfield_y - 94.0f + 28.0f * float(flight.parking_slot % 7);
            const float slot_x = flight.airfield_x + 43.0f + 27.0f * float(flight.parking_slot / 7);
            const std::array<std::pair<float, float>, 4> route{{
                {runway_x, flight.airfield_y + side * 291.0f},
                {flight.airfield_x + 12.0f, flight.airfield_y + side * 291.0f},
                {flight.airfield_x + 12.0f, slot_y},
                {slot_x, slot_y}
            }};
            float remaining = 8.0f * delta_ms / 1000.0f;
            while (remaining > 0.0f && flight.taxi_leg < route.size()) {
                const auto [tx, ty] = route[flight.taxi_leg];
                const float dx = tx - position->x, dy = ty - position->y;
                const float distance = std::hypot(dx, dy);
                if (distance <= remaining + 0.001f) {
                    position->x = tx;
                    position->y = ty;
                    remaining -= distance;
                    ++flight.taxi_leg;
                } else {
                    position->x += dx / distance * remaining;
                    position->y += dy / distance * remaining;
                    remaining = 0.0f;
                }
                flight.approach->pose.heading = std::atan2(dx, dy);
            }
            position->z = terrain_.height_at(position->x, position->y);
            aircraft->x = position->x;
            aircraft->y = position->y;
            flight.approach->pose.x = position->x;
            flight.approach->pose.y = position->y;
            flight.approach->pose.altitude = 0.0f;
            flight.approach->pose.speed = flight.taxi_leg < route.size() ? 8.0f : 0.0f;
            spatial_grid_.update(plane, position->x, position->y);
            if (flight.taxi_leg == route.size()) {
                flight.taxiing = false;
                flight.landed = true;
                aircraft->status = Aircraft::Status::ON_GROUND;
                aircraft->mission = Aircraft::Mission::REFUEL;
                aircraft->target_base = INVALID_ENTITY;
                aircraft->fuel = aircraft->max_fuel;
                aircraft->material = aircraft->max_material;
                aircraft->ammunition = aircraft->max_ammunition;
                aircraft->airborne_time_ms = 0.0f;
            }
            continue;
        }
        if (!position || !aircraft || !owner || get_unit_is_dead(plane) ||
            aircraft->status != Aircraft::Status::AIRBORNE) {
            finished.push_back(plane);
            continue;
        }
        float airfield_x = 0.0f;
        float airfield_y = 0.0f;
        const bool have_airfield = !flight.leaving &&
            nearest_friendly_airfield(owner->faction_id, position->x, position->y, airfield_x, airfield_y);
        if (!have_airfield && flight.sortie) {
            finished.push_back(plane);  // nowhere to land: back to orbiting
            continue;
        }
        if (!have_airfield) {
            // No airfield to land on: keep flying out of the theatre rather than loiter.
            flight.leaving = true;
            if (position->x <= map_edge + pathfinding_.cell_size()) {
                departed.push_back(plane);
                finished.push_back(plane);
            } else if (move_targets_.find(plane) == move_targets_.end()) {
                move_unit(plane, map_edge, position->y);
            }
            continue;
        }
        if (!flight.approach || flight.airfield_x != airfield_x || flight.airfield_y != airfield_y) {
            const auto* data = component_manager_.get_component<UnitData>(plane);
            float heading = get_unit_heading(plane);
            if (!flight.sortie) heading = std::numbers::pi_v<float> * 0.5f;
            flight.approach.emplace(position->x, position->y, heading, airfield_x, airfield_y,
                                    data ? data->speed : aircraft->cruise_speed);
            flight.airfield_x = airfield_x;
            flight.airfield_y = airfield_y;
            air_orbits_.erase(plane);
            move_targets_.erase(plane);
            queued_moves_.erase(plane);
        }
        flight.approach->advance(delta_ms / 1000.0f);
        const auto& pose = flight.approach->pose;
        position->x = pose.x;
        position->y = pose.y;
        position->z = terrain_.height_at(pose.x, pose.y) + pose.altitude;
        aircraft->x = pose.x;
        aircraft->y = pose.y;
        if (auto* velocity = component_manager_.get_component<Velocity>(plane)) {
            velocity->x = std::sin(pose.heading) * pose.speed;
            velocity->y = std::cos(pose.heading) * pose.speed;
            velocity->z = 0;
        }
        spatial_grid_.update(plane, pose.x, pose.y);
        if (pose.finished) {
            int slot = 0;
            // Reuse a slot vacated by a sortie, but never one still occupied.
            while (std::any_of(ferry_flights_.begin(), ferry_flights_.end(), [&](const auto& entry) {
                const auto& other = entry.second;
                return entry.first != plane && other.parking_slot == slot &&
                    other.airfield_x == flight.airfield_x && other.airfield_y == flight.airfield_y &&
                    (other.landed || other.taxiing);
            })) ++slot;
            flight.parking_slot = slot;
            flight.taxiing = true;
            aircraft->status = Aircraft::Status::LANDING;
            // Ground crews service the aircraft once it has safely rolled out;
            // low-fuel sorties must not run dry while taxiing to the apron.
            aircraft->fuel = aircraft->max_fuel;
            aircraft->material = aircraft->max_material;
            aircraft->ammunition = aircraft->max_ammunition;
            aircraft->airborne_time_ms = 0.0f;
        }
    }
    for (const auto plane : finished) ferry_flights_.erase(plane);
    for (const auto plane : departed) destroy_unit(plane);
}

void Simulation::update_sightings(FactionId faction) {
    auto& log = sightings_[static_cast<size_t>(faction)];
    for (auto& [id, sighting] : log) {
        (void)id;
        sighting.in_view = false;
    }
    for (const auto entity : seen_enemies(faction)) {
        if (component_manager_.get_component<Aircraft>(entity)) continue;
        const auto* position = component_manager_.get_component<Position>(entity);
        const auto* data = component_manager_.get_component<UnitData>(entity);
        // Command posts carry generic unit data, but a found one stays found.
        const bool mobile = data && data->speed > 0.0f && entity != match_player_post_ && entity != match_enemy_post_;
        log[entity] = {position->x, position->y, data ? data->unit_type : -1, tick_, mobile, true};
    }
    for (auto it = log.begin(); it != log.end();) {
        const auto& sighting = it->second;
        const bool seen_gone = !sighting.in_view && get_unit_is_dead(it->first) && vision_map_.visible(faction, sighting.x, sighting.y);
        const bool checked = !sighting.in_view && vision_map_.visible(faction, sighting.x, sighting.y) &&
                             has_unit_near(faction, sighting.x, sighting.y, kSightingCheckedMeters);
        const bool stale = !sighting.in_view && sighting.mobile && tick_ - sighting.tick > kSightingMemoryTicks;
        it = seen_gone || checked || stale ? log.erase(it) : std::next(it);
    }
}

void Simulation::refresh_vision() {
    if (vision_terrain_revision_ != terrain_.revision()) {
        vision_map_.sample_terrain([this](float x, float y) { return terrain_.height_at(x, y); });
        intel_map_.sample_terrain([this](float x, float y) { return terrain_.height_at(x, y); });
        vision_terrain_revision_ = terrain_.revision();
    }
    vision_map_.clear();
    intel_map_.clear();
    for (const auto& source : vision_sources_) vision_map_.reveal(source.faction, source.x, source.y, source.radius, source.eye_height);
    for (const auto entity : component_manager_.entities_with<Position, Faction>(entity_manager_.get_entities())) {
        const auto* sensor = component_manager_.get_component<UnitData>(entity);
        const auto* health = component_manager_.get_component<Health>(entity);
        if (!sensor || (sensor->view_range <= 0.0f && sensor->intel_range <= 0.0f) || (health && (health->is_dead || health->current <= 0.0f))) continue;
        const auto* position = component_manager_.get_component<Position>(entity);
        intel_map_.reveal(component_manager_.get_component<Faction>(entity)->faction_id,
                          position->x, position->y, sensor->intel_range,
                          is_airborne(entity) ? kAircraftEyeHeight : kGroundEyeHeight);
        vision_map_.reveal(component_manager_.get_component<Faction>(entity)->faction_id, position->x, position->y, sensor->view_range,
                           is_airborne(entity) ? kAircraftEyeHeight : kGroundEyeHeight);
    }
    for (const EntityId post : {match_player_post_, match_enemy_post_}) {
        const auto* faction = component_manager_.get_component<Faction>(post);
        if (faction) update_sightings(faction->faction_id);
    }
    // Radar grants current anonymous contacts, not identity, sight or a lock.
    for (size_t side = 0; side < radar_contacts_.size(); ++side) {
        auto& contacts = radar_contacts_[side];
        contacts.clear();
        std::set<std::pair<int, int>> cells;
        const auto faction = static_cast<FactionId>(side);
        for (const auto entity : entity_manager_.get_entities()) {
            const auto* owner = component_manager_.get_component<Faction>(entity);
            const auto* position = component_manager_.get_component<Position>(entity);
            if (!owner || !position || owner->faction_id == faction || get_unit_is_dead(entity) ||
                is_watched(faction, entity, position->x, position->y)) continue;
            const bool detected = is_airborne(entity) ? intel_map_.visible_in_sky(faction, position->x, position->y)
                                                      : intel_map_.visible(faction, position->x, position->y);
            if (detected) cells.emplace(static_cast<int>(std::floor(position->x / 100.0f)),
                                        static_cast<int>(std::floor(position->y / 100.0f)));
        }
        for (const auto& [x,y] : cells) contacts.push_back({x*100.0f+50.0f, y*100.0f+50.0f});
    }
    update_air_threat_reports();
    ++vision_revision_;
}

std::vector<EntityId> Simulation::seen_enemies(FactionId faction) const {
    std::vector<EntityId> seen;
    for (const auto entity : entity_manager_.get_entities()) {
        const auto* owner = component_manager_.get_component<Faction>(entity);
        const auto* position = component_manager_.get_component<Position>(entity);
        const auto* health = component_manager_.get_component<Health>(entity);
        if (!owner || !position || owner->faction_id == faction || (health && health->is_dead)) continue;
        if (is_watched(faction, entity, position->x, position->y)) seen.push_back(entity);
    }
    std::sort(seen.begin(), seen.end());
    return seen;
}

void Simulation::record_external_action(std::string name, std::vector<double> args) {
    match_record_.add_action(tick_, std::move(name), std::move(args));
}

void Simulation::play_back_commands(std::vector<InputCommand> commands) {
    std::stable_sort(commands.begin(), commands.end(), [](const InputCommand& a, const InputCommand& b) { return a.tick_id < b.tick_id; });
    playback_ = std::move(commands);
    playback_cursor_ = 0;
}

// Orders that ran at this tick go back into the queue in their original order,
// so formations regroup exactly as they did.
void Simulation::feed_playback() {
    std::vector<InputCommand> due;
    while (playback_cursor_ < playback_.size() && playback_[playback_cursor_].tick_id <= tick_) {
        if (playback_[playback_cursor_].tick_id == tick_) due.push_back(playback_[playback_cursor_]);
        ++playback_cursor_;
    }
    if (!due.empty()) command_manager_.inject_local_commands(due);
}

uint64_t Simulation::state_hash() const {
    uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](const void* data, size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t index = 0; index < size; ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ull;
        }
    };
    mix(&tick_, sizeof(tick_));
    auto entities = entity_manager_.get_entities();
    std::sort(entities.begin(), entities.end());
    for (const EntityId entity : entities) {
        mix(&entity, sizeof(entity));
        if (const auto* faction = component_manager_.get_component<Faction>(entity)) mix(&faction->faction_id, sizeof(faction->faction_id));
        if (const auto* position = component_manager_.get_component<Position>(entity)) {
            mix(&position->x, sizeof(position->x));
            mix(&position->y, sizeof(position->y));
        }
        if (const auto* health = component_manager_.get_component<Health>(entity)) {
            mix(&health->current, sizeof(health->current));
            mix(&health->is_dead, sizeof(health->is_dead));
        }
    }
    for (const FactionId faction : {FactionId::ELITE_PRECISION, FactionId::MASS_WARFARE, FactionId::INDUSTRIAL_EXPERIMENTAL}) {
        if (const auto* resources = production_manager_.tactical_resources(production_manager_.faction_storage(faction))) {
            mix(&resources->materials.current, sizeof(resources->materials.current));
            mix(&resources->fuel.current, sizeof(resources->fuel.current));
        }
    }
    for (const auto& [id, node] : power_grid_.nodes()) {
        mix(&id, sizeof(id));
        mix(&node.owner, sizeof(node.owner));
    }
    for (const auto& [id, line] : power_grid_.lines()) {
        mix(&id, sizeof(id));
        mix(&line.owner, sizeof(line.owner));
        mix(&line.intact, sizeof(line.intact));
        mix(&line.repair_remaining_ms, sizeof(line.repair_remaining_ms));
    }
    mix(&match_result_, sizeof(match_result_));
    mix(&match_victory_reason_, sizeof(match_victory_reason_));
    for (const auto held : key_sites_held_) mix(&held, sizeof(held));
    for (const auto ticks : key_site_hold_ticks_) mix(&ticks, sizeof(ticks));
    return hash;
}

MatchSample Simulation::sample_factions() const {
    MatchSample sample;
    sample.tick = tick_;
    for (const EntityId entity : entity_manager_.get_entities()) {
        const auto* faction = component_manager_.get_component<Faction>(entity);
        const auto* data = component_manager_.get_component<UnitData>(entity);
        const auto* health = component_manager_.get_component<Health>(entity);
        const auto* position = component_manager_.get_component<Position>(entity);
        const auto index = faction ? static_cast<size_t>(faction->faction_id) : sample.factions.size();
        if (index >= sample.factions.size() || !data || !position || (health && health->is_dead)) continue;
        auto& summary = sample.factions[index];
        ++summary.units;
        if (data->speed > 0.0f && component_manager_.get_component<Weapon>(entity) &&
            entity != match_player_post_ && entity != match_enemy_post_) {
            ++summary.fighters;
            summary.army_x += position->x;
            summary.army_y += position->y;
        }
    }
    for (size_t index = 0; index < sample.factions.size(); ++index) {
        auto& summary = sample.factions[index];
        if (summary.fighters > 0) {
            summary.army_x /= static_cast<float>(summary.fighters);
            summary.army_y /= static_cast<float>(summary.fighters);
        }
        if (const auto* resources = production_manager_.tactical_resources(production_manager_.faction_storage(static_cast<FactionId>(index)))) {
            summary.materials = resources->materials.current;
        }
    }
    return sample;
}

void Simulation::record_match_tick() {
    if (!match_record_.recording()) return;
    for (size_t faction = 0; faction < sightings_.size(); ++faction) {
        if (!sightings_[faction].empty()) match_record_.note_first_contact(faction, tick_);
    }
    if (tick_ % MatchRecord::kHashEveryTicks == 0) match_record_.add_hash(tick_, state_hash());
    if (tick_ % MatchRecord::kSampleEveryTicks == 0) match_record_.add_sample(sample_factions());
    if (match_result_ != -1) {
        match_record_.add_hash(tick_, state_hash());
        match_record_.add_sample(sample_factions());
        match_record_.finish(tick_, match_result_, match_seconds());
    }
}

void Simulation::update_match_result() {
    if (match_result_ != -1 || match_player_post_ == INVALID_ENTITY) return;
    const bool player_lost = get_unit_is_dead(match_player_post_);
    const bool enemy_lost = get_unit_is_dead(match_enemy_post_);
    if (player_lost || enemy_lost) {
        match_result_ = player_lost == enemy_lost ? 2 : player_lost ? 1 : 0;
        match_victory_reason_ = 0;
        return;
    }
    if (key_site_victory_sites_.empty()) return;
    key_sites_held_.fill(0);
    const auto count_owned = [this](EntityId id, int owner) {
        if (owner >= 0 && static_cast<size_t>(owner) < key_sites_held_.size() &&
            std::binary_search(key_site_victory_sites_.begin(), key_site_victory_sites_.end(), id))
            ++key_sites_held_[static_cast<size_t>(owner)];
    };
    for (const auto& site : material_sites()) count_owned(site.id, site.owner);
    for (const auto& depot : fuel_depots()) count_owned(depot.id, depot.owner);
    for (const auto faction : {match_player_faction_, match_enemy_faction_}) {
        const size_t index = static_cast<size_t>(faction);
        if (index >= key_site_hold_ticks_.size()) continue;
        key_site_hold_ticks_[index] = key_sites_held_[index] >= key_sites_required_
            ? key_site_hold_ticks_[index] + 1 : 0;
    }
    const bool player_held = key_site_hold_ticks_[static_cast<size_t>(match_player_faction_)] >= key_site_hold_ticks_required_;
    const bool enemy_held = key_site_hold_ticks_[static_cast<size_t>(match_enemy_faction_)] >= key_site_hold_ticks_required_;
    if (player_held || enemy_held) {
        match_result_ = player_held == enemy_held ? 2 : player_held ? 0 : 1;
        match_victory_reason_ = 1;
    }
}

void Simulation::update_fuel_supply(float delta_ms) {
    update_site_captures();
    std::erase_if(fuel_captures_, [this](const FuelCapture& capture) {
        const auto depot = production_manager_.resource_nodes().find(capture.depot);
        const auto* position = component_manager_.get_component<Position>(capture.unit);
        const auto* faction = component_manager_.get_component<Faction>(capture.unit);
        const auto* health = component_manager_.get_component<Health>(capture.unit);
        if (depot == production_manager_.resource_nodes().end() || !position || !faction ||
            (health && health->is_dead)) return true;
        const float depot_x = depot->second.x;
        const float depot_y = depot->second.y;
        if (std::hypot(position->x - depot_x, position->y - depot_y) <= kFuelDepotCaptureRadius) {
            // Both sides know this without sight: one is standing on the depot
            // and the other has just stopped receiving its fuel.
            const EntityId previous = production_manager_.fuel_depot_holder(capture.depot);
            if (previous != production_manager_.faction_storage(faction->faction_id)) {
                record_radio_event(RadioEventKind::FUEL_DEPOT_SECURED, faction->faction_id, depot_x, depot_y, 1, capture.depot);
                for (size_t side = 0; side < FACTION_COUNT && previous != INVALID_ENTITY; ++side) {
                    if (production_manager_.faction_storage(static_cast<FactionId>(side)) == previous) {
                        record_radio_event(RadioEventKind::FUEL_DEPOT_LOST, static_cast<FactionId>(side), depot_x, depot_y, 1, capture.depot);
                    }
                }
            }
            production_manager_.assign_fuel_depot(capture.depot, production_manager_.faction_storage(faction->faction_id), true);
            return true;
        }
        const auto target = move_targets_.find(capture.unit);
        return target == move_targets_.end() ||
            std::hypot(target->second.arrival.x - depot_x, target->second.arrival.y - depot_y) > kFuelDepotCaptureRadius;
    });

    (void)delta_ms;
    std::erase_if(fuel_shipments_, [this](FuelShipment& shipment) {
        auto& reason = fuel_supply_reasons_[static_cast<size_t>(shipment.faction)];
        const auto* position = component_manager_.get_component<Position>(shipment.transport);
        if (!position || get_unit_is_dead(shipment.transport)) {
            reason = "FUEL SHIPMENT SHOT DOWN";
            return true;
        }
        const float distance = std::hypot(position->x - shipment.x, position->y - shipment.y);
        const auto* data = component_manager_.get_component<UnitData>(shipment.transport);
        shipment.remaining_ms = std::max(0.0f, distance - kFuelShipmentLandingRadius) /
            std::max(data ? data->speed : 90.0f, 1.0f) * 1000.0f;
        if (distance > kFuelShipmentLandingRadius) return false;
        // Landed: the aircraft is unloaded and leaves the theatre.
        destroy_unit(shipment.transport);
        if (territorial_control_.active_installation_count(shipment.faction, InstallationType::AIRFIELD) == 0) {
            reason = "FUEL SHIPMENT LOST // NO AIRFIELD";
            return true;
        }
        production_manager_.update_storage(production_manager_.faction_storage(shipment.faction), 0.0f, kFuelShipmentLoad);
        reason = "FUEL SHIPMENT DELIVERED";
        return true;
    });
}

bool Simulation::request_fuel_shipment(FactionId player) {
    auto& reason = fuel_supply_reasons_[static_cast<size_t>(player)];
    if (fuel_shipment(player)) {
        reason = "FUEL SHIPMENT ALREADY INBOUND";
        return false;
    }
    float x = 0.0f;
    float y = 0.0f;
    if (!territorial_control_.get_active_installation_position(player, InstallationType::AIRFIELD, x, y)) {
        reason = "AIRFIELD REQUIRED FOR FUEL SHIPMENTS";
        return false;
    }
    const auto storage = production_manager_.storages().find(production_manager_.faction_storage(player));
    if (storage == production_manager_.storages().end()) return false;
    const auto& funds = storage->second.tactical;
    if (funds.fuel.capacity - funds.fuel.current < kFuelShipmentLoad) {
        reason = "FUEL STORAGE FULL";
        return false;
    }
    const auto cargo = cargo_aircraft_type(player);
    if (!cargo) {
        reason = "NO CARGO AIRCRAFT";
        return false;
    }
    if (at_unit_cap(player)) {
        reason = "UNIT CAP REACHED";
        return false;
    }
    if (funds.materials.current < kFuelShipmentMaterialCost ||
        !production_manager_.deduct_faction_resources(player, kFuelShipmentMaterialCost, 0.0f)) {
        reason = "INSUFFICIENT MATERIALS";
        return false;
    }
    // The cargo aircraft enters at the west map edge level with its airfield and
    // flies straight there, so anything under its path can shoot at it.
    const EntityId transport = launch_cargo_aircraft(player, x, y);
    if (transport == INVALID_ENTITY) {
        production_manager_.update_storage(production_manager_.faction_storage(player), kFuelShipmentMaterialCost, 0.0f);
        reason = "UNIT CAP REACHED";
        return false;
    }
    const float entry_x = pathfinding_.min_world_x_center() - pathfinding_.cell_size();
    const float total_ms = std::max(0.0f, x - entry_x - kFuelShipmentLandingRadius) / get_unit_prototypes().at(*cargo).speed * 1000.0f;
    fuel_shipments_.push_back({player, x, y, total_ms, transport, total_ms});
    reason = "FUEL SHIPMENT INBOUND";
    return true;
}

const FuelShipment* Simulation::fuel_shipment(FactionId faction) const {
    for (const auto& shipment : fuel_shipments_) {
        if (shipment.faction == faction) return &shipment;
    }
    return nullptr;
}

// Where a unit goes to work on a station or line: the station itself, or the
// nearest stretch of the line it can stand on.
bool Simulation::power_work_point(EntityId unit, PowerOrder order, uint32_t target, float& x, float& y) const {
    const auto* position = component_manager_.get_component<Position>(unit);
    if (!position) return false;
    const auto& navigation = navigation_for(unit);
    const auto reachable = [&](float px, float py) {
        return navigation.is_walkable(navigation.to_grid_x(px), navigation.to_grid_y(py)) &&
            (!theater_water_rules_enabled_ || is_land_position(px, py));
    };
    if (order == PowerOrder::CAPTURE_STATION) {
        const PowerNode* node = power_grid_.node(target);
        if (!node) return false;
        if (reachable(node->x, node->y)) { x = node->x; y = node->y; return true; }
        // A station's own ground may be blocked; stop beside it.
        for (const float ring : {15.0f, 30.0f}) {
            for (int step = 0; step < 8; ++step) {
                const float angle = static_cast<float>(step) * 0.78539816f;
                const float px = node->x + std::cos(angle) * ring;
                const float py = node->y + std::sin(angle) * ring;
                if (reachable(px, py)) { x = px; y = py; return true; }
            }
        }
        return false;
    }
    // A repair is done at the break; other work anywhere along the line.
    const PowerLine* line = power_grid_.line(target);
    if (!line) return false;
    const bool at_break = order == PowerOrder::REPAIR_LINE;
    const float from_x = at_break ? line->cut_x : position->x;
    const float from_y = at_break ? line->cut_y : position->y;
    float best = std::numeric_limits<float>::infinity();
    for (const auto& point : power_grid_.sample_line(target, 20.0f)) {
        const float distance = std::hypot(point[0] - from_x, point[1] - from_y);
        if (distance >= best || !reachable(point[0], point[1])) continue;
        best = distance;
        x = point[0];
        y = point[1];
    }
    return std::isfinite(best);
}

std::string Simulation::power_order_refusal(EntityId unit, PowerOrder order, uint32_t target) const {
    const auto* faction = component_manager_.get_component<Faction>(unit);
    const auto* health = component_manager_.get_component<Health>(unit);
    if (!faction || !health || health->is_dead) return "NO UNIT TO SEND";
    if (component_manager_.get_component<Aircraft>(unit) || component_manager_.get_component<NavalVessel>(unit))
        return "GROUND UNIT REQUIRED";
    const int side = static_cast<int>(faction->faction_id);
    if (order == PowerOrder::CAPTURE_STATION) {
        const PowerNode* node = power_grid_.node(target);
        if (!node || node->kind != PowerNodeKind::STATION) return "NO POWER STATION THERE";
        if (node->owner == side) return "POWER STATION ALREADY HELD";
    } else {
        const PowerLine* line = power_grid_.line(target);
        if (!line) return "NO POWER LINE THERE";
        switch (order) {
            case PowerOrder::CAPTURE_LINE:
                if (!line->intact) return "POWER LINE IS DOWN // ENGINEER MUST REPAIR IT";
                if (line->owner == side) return "POWER LINE ALREADY HELD";
                break;
            case PowerOrder::SEVER_LINE:
                if (!line->intact) return "POWER LINE ALREADY DOWN";
                break;
            case PowerOrder::REPAIR_LINE: {
                if (line->intact) return "POWER LINE IS INTACT";
                if (line->repair_remaining_ms > 0.0f) return "POWER LINE REPAIR UNDER WAY";
                if (!is_construction_unit(unit)) return "ENGINEER REQUIRED TO REPAIR POWER LINES";
                const auto* funds = production_manager_.tactical_resources(production_manager_.faction_storage(faction->faction_id));
                if (!funds || funds->materials.current < power_grid_.repair_cost(target)) return "INSUFFICIENT MATERIALS";
                break;
            }
            default:
                return "UNKNOWN POWER ORDER";
        }
    }
    float x = 0.0f;
    float y = 0.0f;
    if (!power_work_point(unit, order, target, x, y)) return "NO GROUND ROUTE TO IT";
    return {};
}

size_t Simulation::issue_power_command(EntityId unit, FactionId faction, PowerOrder order, uint32_t target) {
    if (!power_order_refusal(unit, order, target).empty()) return 0;
    return issue_commands({unit}, faction, CommandType::HARVEST, 0.0f, 0.0f, power_order_extra(order, target));
}

void Simulation::order_power_work(EntityId unit, uint32_t extra) {
    const auto order = static_cast<PowerOrder>((extra >> 16) & 0xFFu);
    const uint32_t target = extra & 0xFFFFu;
    float x = 0.0f;
    float y = 0.0f;
    if (!power_order_refusal(unit, order, target).empty() || !power_work_point(unit, order, target, x, y)) return;
    // One errand at a time: a new one replaces any capture the unit was on.
    std::erase_if(fuel_captures_, [unit](const FuelCapture& capture) { return capture.unit == unit; });
    std::erase_if(site_captures_, [unit](const SiteCapture& capture) { return capture.unit == unit; });
    std::erase_if(power_tasks_, [unit](const PowerTask& task) { return task.unit == unit; });
    power_tasks_.push_back({unit, order, target, x, y});
    move_unit(unit, x, y);
}

void Simulation::update_power_grid(float delta_ms) {
    if (!power_grid_.active()) return;
    std::erase_if(power_tasks_, [this](const PowerTask& task) {
        const auto* position = component_manager_.get_component<Position>(task.unit);
        const auto* faction = component_manager_.get_component<Faction>(task.unit);
        const auto* health = component_manager_.get_component<Health>(task.unit);
        if (!position || !faction || (health && health->is_dead)) return true;
        const int side = static_cast<int>(faction->faction_id);
        const PowerNode* station = task.order == PowerOrder::CAPTURE_STATION ? power_grid_.node(task.target) : nullptr;
        const float distance = station
            ? std::min(std::hypot(position->x - station->x, position->y - station->y),
                       std::hypot(position->x - task.x, position->y - task.y))
            : task.order == PowerOrder::REPAIR_LINE
                ? std::hypot(position->x - task.x, position->y - task.y)
                : power_grid_.distance_to_line(task.target, position->x, position->y);
        if (distance <= kPowerWorkRadius) {
            switch (task.order) {
                case PowerOrder::CAPTURE_STATION: power_grid_.capture_station(task.target, side); break;
                case PowerOrder::CAPTURE_LINE: power_grid_.capture_line(task.target, side); break;
                case PowerOrder::SEVER_LINE: power_grid_.sever_line(task.target, position->x, position->y); break;
                case PowerOrder::REPAIR_LINE: {
                    const PowerLine* line = power_grid_.line(task.target);
                    if (!line || line->intact || line->repair_remaining_ms > 0.0f) break;
                    // Paid when the work starts, like any construction.
                    if (!production_manager_.deduct_faction_resources(faction->faction_id, power_grid_.repair_cost(task.target), 0.0f)) {
                        power_notices_[static_cast<size_t>(side)] = "POWER LINE REPAIR REFUSED // INSUFFICIENT MATERIALS";
                        break;
                    }
                    power_grid_.begin_repair(task.target, side);
                    power_notices_[static_cast<size_t>(side)].clear();
                    break;
                }
            }
            return true;
        }
        // A unit sent elsewhere, or one that has stopped short, gives up.
        const auto target = move_targets_.find(task.unit);
        return target == move_targets_.end() ||
            std::hypot(target->second.arrival.x - task.x, target->second.arrival.y - task.y) > kPowerWorkRadius;
    });
    power_grid_.update(delta_ms);
    for (size_t side = 0; side < FACTION_COUNT; ++side) {
        const EntityId storage = production_manager_.faction_storage(static_cast<FactionId>(side));
        if (storage == INVALID_ENTITY) continue;
        const PowerSupply supply = power_grid_.supply(static_cast<int>(side));
        production_manager_.set_tactical_power(storage, supply.generation_mw, supply.demand_mw);
    }
}

const std::string& Simulation::fuel_supply_reason(FactionId faction) const {
    return fuel_supply_reasons_[static_cast<size_t>(faction)];
}

bool Simulation::assign_fuel_depot(EntityId depot, FactionId holder) {
    const auto node = production_manager_.resource_nodes().find(depot);
    const EntityId storage = production_manager_.faction_storage(holder);
    if (node == production_manager_.resource_nodes().end() || storage == INVALID_ENTITY ||
        node->second.type != ResourceNode::Type::ENERGY) return false;
    production_manager_.assign_fuel_depot(depot, storage, false);
    return true;
}

bool Simulation::assign_material_site(EntityId site, FactionId holder) {
    const auto node = production_manager_.resource_nodes().find(site);
    const EntityId line = production_manager_.faction_line(holder);
    if (node == production_manager_.resource_nodes().end() || line == INVALID_ENTITY ||
        node->second.type == ResourceNode::Type::ENERGY) return false;
    EntityId extractor_id = INVALID_ENTITY;
    EntityId node_id = INVALID_ENTITY;
    if (!production_manager_.find_or_create_extractor(node->second.x, node->second.y, extractor_id, node_id) || node_id != site) return false;
    production_manager_.extractors().at(extractor_id).storage_id = line;
    return true;
}

std::vector<FuelDepotStatus> Simulation::fuel_depots() const {
    std::vector<FuelDepotStatus> depots;
    for (const auto& [id, node] : production_manager_.resource_nodes()) {
        if (node.type != ResourceNode::Type::ENERGY) continue;
        FuelDepotStatus status{id, node.x, node.y, node.amount, node.max_amount};
        const EntityId holder = production_manager_.fuel_depot_holder(id);
        for (size_t faction = 0; faction < FACTION_COUNT && holder != INVALID_ENTITY; ++faction) {
            if (production_manager_.faction_storage(static_cast<FactionId>(faction)) == holder) {
                status.owner = static_cast<int>(faction);
            }
        }
        for (const auto& capture : fuel_captures_) {
            const auto* faction = component_manager_.get_component<Faction>(capture.unit);
            if (capture.depot != id || !faction) continue;
            status.capturing = true;
            status.capturing_faction = static_cast<int>(faction->faction_id);
        }
        depots.push_back(status);
    }
    std::sort(depots.begin(), depots.end(),
        [](const FuelDepotStatus& a, const FuelDepotStatus& b) { return a.id < b.id; });
    return depots;
}

bool Simulation::begin_control_point_battle(const std::vector<ControlPointDefinition>& points,
                                            FactionId player, FactionId enemy, float hold_duration_ms) {
    return control_point_battle_.begin(points, player, enemy, hold_duration_ms);
}

bool Simulation::configure_reinforcement_delivery(float x, float y, float radius, FactionId owner) {
    float airfield_x = 0.0f;
    float airfield_y = 0.0f;
    const size_t airfield_count = territorial_control_.active_installation_count(owner, InstallationType::AIRFIELD);
    if (airfield_count == 0 || !territorial_control_.get_active_installation_position(owner, InstallationType::AIRFIELD, airfield_x, airfield_y)) {
        reinforcement_delivery_.reject("AIRFIELD REQUIRED FOR REINFORCEMENTS");
        return false;
    }
    if (!reinforcement_delivery_.configure(airfield_x, airfield_y, radius, owner)) return false;
    if (airfield_count == 1) return reinforcement_delivery_.select_airfield(airfield_x, airfield_y);
    reinforcement_delivery_.require_airfield_selection();
    return true;
}

bool Simulation::select_reinforcement_delivery_zone(FactionId player, float x, float y) {
    if (player != reinforcement_delivery_.owner()) return false;
    float airfield_x = 0.0f;
    float airfield_y = 0.0f;
    if (!territorial_control_.get_active_installation_position_near(player, InstallationType::AIRFIELD,
                                                                     x, y, 1000.0f, airfield_x, airfield_y)) {
        reinforcement_delivery_.require_airfield_selection();
        return false;
    }
    return reinforcement_delivery_.select_airfield(airfield_x, airfield_y);
}

bool Simulation::request_reinforcement_delivery(FactionId player) {
    if (player != reinforcement_delivery_.owner()) return false;
    const auto& package = reinforcement_delivery_.package();
    const EntityId line = production_manager_.faction_line(player);
    const auto storage = production_manager_.storages().find(line);
    const bool funded = storage != production_manager_.storages().end() &&
        storage->second.tactical.materials.current >= package.material_cost;
    if (!reinforcement_delivery_.request(funded, !at_unit_cap(player))) return false;
    return production_manager_.deduct_faction_resources(player, package.material_cost, 0.0f);
}

void Simulation::set_reinforcement_resources(FactionId faction, float material, float energy) {
    const EntityId line = production_manager_.faction_line(faction);
    auto storage = production_manager_.storages().find(line);
    if (storage == production_manager_.storages().end()) return;
    storage->second.metal_storage = std::max(0.0f, material);
    storage->second.energy_storage = std::max(0.0f, energy);
    storage->second.tactical.materials.current = storage->second.metal_storage;
    storage->second.tactical.fuel.current = std::min(storage->second.energy_storage,
                                                       storage->second.tactical.fuel.capacity);
}

float Simulation::get_unit_x(EntityId entity) const {
    auto* pos = component_manager_.get_component<Position>(entity);
    return pos ? pos->x : 0.0f;
}

float Simulation::get_unit_y(EntityId entity) const {
    auto* pos = component_manager_.get_component<Position>(entity);
    return pos ? pos->y : 0.0f;
}

bool Simulation::has_move_target(EntityId entity) const {
    return move_targets_.find(entity) != move_targets_.end();
}

bool Simulation::is_making_room(EntityId entity) const {
    const auto* steering = component_manager_.get_component<GroundSteering>(entity);
    return steering && steering->yielding && move_targets_.find(entity) != move_targets_.end();
}

bool Simulation::get_unit_route(EntityId entity, std::vector<std::pair<float, float>>& route) const {
    route.clear();
    const auto target = move_targets_.find(entity);
    const auto* position = component_manager_.get_component<Position>(entity);
    if (target == move_targets_.end() || !position) return false;
    route.emplace_back(position->x, position->y);
    if (!trace_route_leg(entity, position->x, position->y, target->second, route)) return true;
    // Each queued leg starts where the previous one arrives.
    if (const auto queue = queued_moves_.find(entity); queue != queued_moves_.end()) {
        for (const auto& waypoint : queue->second) {
            const auto [from_x, from_y] = route.back();
            if (!trace_route_leg(entity, from_x, from_y, waypoint, route)) break;
        }
    }
    return true;
}

// Appends one leg's turns and arrival point after (from_x, from_y), which the
// caller has already placed in `route`. Returns false when the leg stopped
// short (the point cap or a dead end), so later legs cannot follow it.
bool Simulation::trace_route_leg(EntityId entity, float from_x, float from_y, const MoveTarget& target,
                                 std::vector<std::pair<float, float>>& route) const {
    const auto& navigation = navigation_for(entity);
    const float arrival_x = target.arrival.x;
    const float arrival_y = target.arrival.y;
    const float route_x = target.strategic_route.x;
    const float route_y = target.strategic_route.y;

    // Walk the same flow field update_movement steers by. The unit drives in
    // its cell's flow direction from wherever it stands in that cell, so step
    // whole cells from the unit's own position rather than snapping to cell
    // centres. A corner is a cell where that direction changes. Once the
    // arrival point is in line of sight the movement system drives straight
    // at it, so the walk ends there.
    constexpr int MAX_TRACE_CELLS = 4096;
    const float cell = navigation.cell_size();
    float x = from_x;
    float y = from_y;
    std::pair<float, float> heading{0.0f, 0.0f};
    bool reaches_arrival = navigation.has_line_of_sight(x, y, arrival_x, arrival_y);
    for (int step = 0; !reaches_arrival && step < MAX_TRACE_CELLS &&
            route.size() + 2 < MAX_ROUTE_POINTS; ++step) {
        const auto direction = navigation.flow_direction(x, y, route_x, route_y);
        if (direction.first == 0.0f && direction.second == 0.0f) break;
        if (step > 0 && direction != heading) route.emplace_back(x, y);
        heading = direction;
        x += direction.first * cell;
        y += direction.second * cell;
        reaches_arrival = navigation.has_line_of_sight(x, y, arrival_x, arrival_y);
    }
    if (route.back() != std::pair<float, float>{x, y}) {
        // Where the walk ends is a turn unless the last leg already points
        // at the arrival point.
        const float cross = heading.first * (arrival_y - y) - heading.second * (arrival_x - x);
        if (!reaches_arrival || std::abs(cross) > 0.01f * cell) route.emplace_back(x, y);
    }
    if (reaches_arrival) route.emplace_back(arrival_x, arrival_y);
    return reaches_arrival && route.size() + 2 < MAX_ROUTE_POINTS;
}

bool Simulation::get_aircraft_approach_pose(EntityId entity, float& altitude, float& pitch, float& bank) const {
    const auto it = ferry_flights_.find(entity);
    if (it == ferry_flights_.end() || !it->second.approach || it->second.leaving) return false;
    const auto& pose = it->second.approach->pose;
    altitude = pose.altitude;
    pitch = pose.pitch;
    bank = pose.bank;
    return true;
}

float Simulation::get_unit_heading(EntityId entity) const {
    if (const auto it = ferry_flights_.find(entity); it != ferry_flights_.end() && it->second.approach && !it->second.leaving)
        return it->second.approach->pose.heading;
    if (const auto* steering = component_manager_.get_component<GroundSteering>(entity)) {
        return steering->heading;
    }
    if (const auto* velocity = component_manager_.get_component<Velocity>(entity);
        velocity && (std::abs(velocity->x) > 0.0001f || std::abs(velocity->y) > 0.0001f)) {
        return std::atan2(velocity->x, velocity->y);
    }
    return 0.0f;
}

bool Simulation::get_unit_off_road_state(EntityId entity, float& wear, float& distance,
                                          float& speed_multiplier) const {
    const auto* off_road = component_manager_.get_component<OffRoadWear>(entity);
    if (!off_road) return false;
    wear = off_road->accumulated;
    distance = off_road->distance_traveled;
    speed_multiplier = 1.0f - std::min(
        get_off_road_settings().maximum_speed_penalty,
        wear * get_off_road_settings().speed_penalty_per_wear);
    return true;
}

bool Simulation::get_unit_steering_state(EntityId entity, float& heading, float& desired_heading, float& speed) const {
    const auto* steering = component_manager_.get_component<GroundSteering>(entity);
    if (!steering) return false;
    heading = steering->heading;
    desired_heading = steering->desired_heading;
    speed = steering->current_speed;
    return true;
}

bool Simulation::get_unit_health(EntityId entity, float& current, float& max) {
    auto* health = component_manager_.get_component<Health>(entity);
    if (!health) return false;
    current = health->current;
    max = health->max;
    return true;
}

bool Simulation::get_unit_is_dead(EntityId entity) {
    auto* health = component_manager_.get_component<Health>(entity);
    return health ? health->is_dead : true;
}

bool Simulation::apply_damage(EntityId entity, float damage) {
    auto* health = component_manager_.get_component<Health>(entity);
    if (!health) return false;
    health->current -= damage;
    health->is_dead = health->current <= 0.0f;
    return true;
}

bool Simulation::get_unit_faction_id(EntityId entity, FactionId& faction_id) {
    auto* faction = component_manager_.get_component<Faction>(entity);
    if (!faction) return false;
    faction_id = faction->faction_id;
    return true;
}

void Simulation::destroy_unit(EntityId entity) {
    ferry_flights_.erase(entity);
    air_orbits_.erase(entity);
    if (!entity_manager_.destroy_entity(Entity(entity))) {
        return;
    }
    const auto* dead_health = component_manager_.get_component<Health>(entity);
    if (dead_health && (dead_health->is_dead || dead_health->current <= 0))
        record_combat_event(CombatEventKind::DESTROYED, entity);
    auto& production = production_manager_;
    production.production_lines().erase(entity);
    for (auto& [id, extractor] : production.extractors())
        if (extractor.storage_id == entity) extractor.active = false;
    combat_manager_.unregister_entity(entity);
    move_targets_.erase(entity);
    queued_moves_.erase(entity);
    patrol_orders_.erase(entity);
    logistics_manager_.archive_intelligence(entity);
    logistics_manager_.remove_aircraft(entity);
    component_manager_.remove_entity(entity);
    spatial_grid_.remove(entity);
}

} // namespace rts

using namespace rts;
// Logistics API
extern "C" {
    void simulation_start() {
        get_simulation()->start();
    }

    void simulation_stop() {
        get_simulation()->stop();
    }

    void simulation_reset() {
        get_simulation()->reset();
    }

    void simulation_update(float delta_ms) {
        get_simulation()->update(delta_ms);
    }

    int simulation_create_unit(float x, float y) {
        Entity entity = get_simulation()->create_unit(x, y);
        return static_cast<int>(entity.id);
    }

    void simulation_move_unit(int entity_id, float x, float y) {
        get_simulation()->move_unit(static_cast<EntityId>(entity_id), x, y);
    }

    void simulation_move_units_formation(
        const int32_t* entity_ids,
        int entity_count,
        float center_x,
        float center_y,
        float spacing) {
        if (!entity_ids || entity_count <= 0 || entity_count > 100000) {
            return;
        }

        std::vector<EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] >= 0) {
                entities.push_back(static_cast<EntityId>(entity_ids[index]));
            }
        }
        get_simulation()->move_units_formation(entities, center_x, center_y, spacing);
    }

    void simulation_destroy_unit(int entity_id) {
        get_simulation()->destroy_unit(static_cast<EntityId>(entity_id));
    }

    int simulation_entity_count() {
        return static_cast<int>(get_simulation()->entity_count());
    }

    float simulation_last_tick_ms() {
        return get_simulation()->last_tick_ms();
    }

    float simulation_get_unit_x(int entity_id) {
        return get_simulation()->get_unit_x(static_cast<EntityId>(entity_id));
    }

    float simulation_get_unit_y(int entity_id) {
        return get_simulation()->get_unit_y(static_cast<EntityId>(entity_id));
    }

    int simulation_has_unit_move_target(int entity_id) {
        return get_simulation()->has_move_target(static_cast<EntityId>(entity_id)) ? 1 : 0;
    }

    int simulation_get_unit_route(int entity_id, int player_id, float* points_xy, int point_capacity) {
        if (!points_xy || entity_id <= 0 || point_capacity <= 0 || player_id < 0 || player_id > 2) return 0;
        rts::Simulation* sim = get_simulation();
        const auto entity = static_cast<EntityId>(entity_id);
        const auto* faction = sim->component_manager().get_component<Faction>(entity);
        if (!faction || faction->faction_id != static_cast<rts::FactionId>(player_id)) return 0;
        std::vector<std::pair<float, float>> route;
        if (!sim->get_unit_route(entity, route)) return 0;
        const int count = std::min(static_cast<int>(route.size()), point_capacity);
        for (int index = 0; index < count; ++index) {
            points_xy[index * 2] = route[static_cast<std::size_t>(index)].first;
            points_xy[index * 2 + 1] = route[static_cast<std::size_t>(index)].second;
        }
        return count;
    }

    int simulation_get_unit_waypoints(int entity_id, int player_id, float* points_xy, int point_capacity) {
        if (!points_xy || entity_id <= 0 || point_capacity <= 0 || player_id < 0 || player_id > 2) return 0;
        rts::Simulation* sim = get_simulation();
        const auto entity = static_cast<EntityId>(entity_id);
        const auto* faction = sim->component_manager().get_component<Faction>(entity);
        if (!faction || faction->faction_id != static_cast<rts::FactionId>(player_id)) return 0;
        const auto waypoints = sim->get_unit_waypoints(entity);
        const int count = std::min(static_cast<int>(waypoints.size()), point_capacity);
        for (int index = 0; index < count; ++index) {
            points_xy[index * 2] = waypoints[static_cast<std::size_t>(index)].first;
            points_xy[index * 2 + 1] = waypoints[static_cast<std::size_t>(index)].second;
        }
        return count;
    }

    int simulation_get_unit_headings(const int32_t* entity_ids, int entity_count, float* headings, int heading_capacity) {
        if (!entity_ids || !headings || entity_count < 0 || entity_count > 100000 || heading_capacity < entity_count) return 0;
        for (int index = 0; index < entity_count; ++index) {
            headings[index] = get_simulation()->get_unit_heading(static_cast<EntityId>(entity_ids[index]));
        }
        return entity_count;
    }

    int simulation_get_unit_steering_state(int entity_id, float* heading, float* desired_heading, float* speed) {
        if (!heading || !desired_heading || !speed) return 0;
        return get_simulation()->get_unit_steering_state(
            static_cast<EntityId>(entity_id), *heading, *desired_heading, *speed
        ) ? 1 : 0;
    }

    int simulation_get_unit_off_road_state(int entity_id, float* wear, float* distance, float* speed_multiplier) {
        if (!wear || !distance || !speed_multiplier) return 0;
        return get_simulation()->get_unit_off_road_state(
            static_cast<EntityId>(entity_id), *wear, *distance, *speed_multiplier) ? 1 : 0;
    }

    int simulation_get_unit_health(int entity_id, float* current, float* max) {
        if (!current || !max) return 0;
        float c, m;
        if (!get_simulation()->get_unit_health(static_cast<EntityId>(entity_id), c, m)) {
            return 0;
        }
        *current = c;
        *max = m;
        return 1;
    }

    int simulation_get_unit_is_dead(int entity_id) {
        return get_simulation()->get_unit_is_dead(static_cast<EntityId>(entity_id)) ? 1 : 0;
    }

    int simulation_get_unit_positions(
        const int32_t* entity_ids,
        int entity_count,
        float* positions_xy,
        int position_capacity) {
        if (!entity_ids || !positions_xy || entity_count < 0 || entity_count > 100000 ||
            position_capacity < entity_count * 2) {
            return 0;
        }

        for (int index = 0; index < entity_count; ++index) {
            positions_xy[index * 2] = get_simulation()->get_unit_x(static_cast<EntityId>(entity_ids[index]));
            positions_xy[index * 2 + 1] = get_simulation()->get_unit_y(static_cast<EntityId>(entity_ids[index]));
        }
        return entity_count;
    }

    void simulation_logistics_add_airbase(int airbase_id, float x, float y, int capacity) {
        Airbase airbase{};
        airbase.x = x;
        airbase.y = y;
        airbase.runway_capacity = capacity;
        airbase.current_fuel = 1000.0f;
        airbase.max_fuel = 5000.0f;
        get_simulation()->logistics_manager().add_airbase(airbase_id, airbase);
    }

    void simulation_logistics_add_carrier(int carrier_id, float x, float y, int deck_capacity) {
        Carrier carrier{};
        carrier.tier = Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 1;
        carrier.refuel_rate = 100;
        carrier.rearm_rate = 20;
        carrier.max_fuel = 5000.0f;
        carrier.current_fuel = carrier.max_fuel;
        carrier.max_munitions = 1000.0f;
        carrier.current_munitions = carrier.max_munitions;
        carrier.runway_usable = true;
        carrier.deck_capacity = deck_capacity;
        carrier.speed = 0.0f;
        carrier.max_speed = 30.0f;
        get_simulation()->logistics_manager().add_carrier(carrier_id, carrier);
    }

    void simulation_logistics_update_intelligence(int entity_id, float x, float y, int tick) {
        get_simulation()->logistics_manager().update_intelligence(entity_id, x, y, tick);
    }

    void simulation_logistics_update_all(float delta_ms) {
        get_simulation()->logistics_manager().update_all(delta_ms);
    }

    void simulation_logistics_add_aircraft(int aircraft_id, float x, float y, float fuel) {
        get_simulation()->logistics_manager().add_aircraft(aircraft_id, x, y, fuel);
    }

    void simulation_logistics_add_naval_vessel(int vessel_id, float x, float y, float fuel) {
        get_simulation()->logistics_manager().add_naval_vessel(vessel_id, x, y, fuel);
    }

    void simulation_logistics_add_naval_base(int base_id, float x, float y, float max_recovery_distance) {
        get_simulation()->logistics_manager().add_naval_base(base_id, x, y, max_recovery_distance);
    }

    void simulation_logistics_resupply_naval_vessel(int vessel_id, float amount) {
        get_simulation()->logistics_manager().resupply_naval_vessel(vessel_id, amount);
    }

    void simulation_combat_spawn_projectile(int entity_id, float target_x, float target_y) {
        auto& manager = get_simulation()->combat_manager();
        auto* weapon = get_simulation()->component_manager().get_component<Weapon>(static_cast<EntityId>(entity_id));
        if (!weapon) return;
        
        auto* pos = get_simulation()->component_manager().get_component<Position>(static_cast<EntityId>(entity_id));
        if (!pos) return;
        
        float dx = target_x - pos->x;
        float dy = target_y - pos->y;
        float speed = weapon->projectile_speed;
        float damage = weapon->damage;
        float aoe_radius = weapon->aoe_radius;
        
        auto* faction = get_simulation()->component_manager().get_component<Faction>(static_cast<EntityId>(entity_id));
        FactionId faction_id = faction ? faction->faction_id : FactionId::ELITE_PRECISION;
        
        manager.projectile_manager().spawn(pos->x, pos->y, dx, dy, speed, damage, aoe_radius, 5.0f, faction_id);
    }
    
    void simulation_set_unit_faction(int entity_id, int faction_id) {
        get_simulation()->set_unit_faction(static_cast<EntityId>(entity_id), static_cast<FactionId>(faction_id));
    }
}

extern "C" {
    void logistics_add_airbase(int airbase_id, float x, float y, int capacity) {
        simulation_logistics_add_airbase(airbase_id, x, y, capacity);
    }

    void logistics_add_carrier(int carrier_id, float x, float y, int deck_capacity) {
        simulation_logistics_add_carrier(carrier_id, x, y, deck_capacity);
    }

    void logistics_update_intelligence(int entity_id, float x, float y, int tick) {
        simulation_logistics_update_intelligence(entity_id, x, y, tick);
    }

    void logistics_update_all(float delta_ms) {
        simulation_logistics_update_all(delta_ms);
    }

    void logistics_add_aircraft(int aircraft_id, float x, float y, float fuel) {
        simulation_logistics_add_aircraft(aircraft_id, x, y, fuel);
    }

    void logistics_add_naval_vessel(int vessel_id, float x, float y, float fuel) {
        simulation_logistics_add_naval_vessel(vessel_id, x, y, fuel);
    }

    void logistics_add_naval_base(int base_id, float x, float y, float max_recovery_distance) {
        simulation_logistics_add_naval_base(base_id, x, y, max_recovery_distance);
    }

    void logistics_resupply_naval_vessel(int vessel_id, float amount) {
        simulation_logistics_resupply_naval_vessel(vessel_id, amount);
    }

    void combat_spawn_projectile(int entity_id, float target_x, float target_y) {
        simulation_combat_spawn_projectile(entity_id, target_x, target_y);
    }

    void economy_add_resource_node(int node_id, float x, float y, float amount, int type) {
        rts::ResourceNode node{};
        node.x = x;
        node.y = y;
        node.amount = amount;
        node.max_amount = amount;
        node.depleted = false;
        
        switch (type) {
            case 0: node.type = rts::ResourceNode::Type::METAL; break;
            case 1: node.type = rts::ResourceNode::Type::ENERGY; break;
            default: return;
        }
        
        get_simulation()->production_manager().add_resource_node(static_cast<EntityId>(node_id), node);
    }

    void economy_add_extractor(int extractor_id, float x, float y, int node_id, float extraction_rate) {
        rts::Extractor extractor{};
        extractor.x = x;
        extractor.y = y;
        extractor.resource_node_id = static_cast<EntityId>(node_id);
        extractor.extraction_rate = extraction_rate;
        extractor.last_extraction_tick = 0.0f;
        extractor.active = true;
        
        get_simulation()->production_manager().extractors()[static_cast<EntityId>(extractor_id)] = extractor;
    }

    void economy_add_storage(int storage_id, float x, float y, float metal_capacity, float energy_capacity) {
        rts::Storage storage{};
        storage.x = x;
        storage.y = y;
        storage.metal_capacity = metal_capacity;
        storage.energy_capacity = energy_capacity;
        storage.metal_storage = 0.0f;
        storage.energy_storage = 0.0f;
        
        get_simulation()->production_manager().add_storage(static_cast<EntityId>(storage_id), storage);
    }

    void economy_add_tactical_storage(int storage_id, float x, float y, float materials_capacity,
                                      float fuel_capacity, float power_generation_mw, float power_demand_mw) {
        if (!std::isfinite(materials_capacity) || !std::isfinite(fuel_capacity) ||
            materials_capacity < 0.0f || fuel_capacity < 0.0f) return;
        rts::Storage storage{};
        storage.x = x; storage.y = y;
        storage.tactical.materials = {0.0f, materials_capacity, 0.0f};
        storage.tactical.fuel = {0.0f, fuel_capacity};
        storage.tactical.power = {power_generation_mw, power_demand_mw};
        get_simulation()->production_manager().add_storage(static_cast<EntityId>(storage_id), storage);
        auto* resources = get_simulation()->production_manager().tactical_resources(static_cast<EntityId>(storage_id));
        if (resources) *resources = storage.tactical;
    }

    void economy_add_production_line(int line_id, int storage_id, float build_speed_metal, float build_speed_energy, int max_jobs) {
        rts::ProductionLine line{};
        line.storage_id = static_cast<EntityId>(storage_id);
        line.build_speed_metal = build_speed_metal;
        line.build_speed_energy = build_speed_energy;
        line.active_jobs = 0;
        line.max_jobs = max_jobs;
        
        get_simulation()->production_manager().add_production_line(static_cast<EntityId>(line_id), line);
    }

    void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick) {
        rts::ConstructionQueueEntry entry{};
        entry.entity_id = static_cast<EntityId>(entity_id);
        entry.build_progress = 0.0f;
        entry.total_cost_metal = metal_cost;
        entry.total_cost_energy = energy_cost;
        entry.metal_per_tick = metal_per_tick;
        entry.energy_per_tick = energy_per_tick;
        entry.completed = false;
        
        switch (type) {
            case 0: entry.type = rts::ConstructionQueueEntry::Type::BUILDING; break;
            case 1: entry.type = rts::ConstructionQueueEntry::Type::UNIT; break;
            default: return;
        }
        
        get_simulation()->production_manager().add_to_queue(static_cast<EntityId>(line_id), entry);
    }

    void economy_update_all(float delta_ms) {
        get_simulation()->update_economy(delta_ms);
    }
}

extern "C" {
    void network_send_visual_pack_handshake(const char* pack_id, uint32_t pack_version, const char* pack_hash) {
        if (!pack_id || !pack_hash) return;
        rts::ConnectionHandshake handshake{};
        handshake.protocol_version = rts::NETWORK_PROTOCOL_VERSION;
        handshake.visual_pack_version = pack_version;
        std::strncpy(reinterpret_cast<char*>(handshake.visual_pack_id), pack_id, rts::VISUAL_PACK_ID_LENGTH);
        std::strncpy(reinterpret_cast<char*>(handshake.visual_pack_hash), pack_hash, rts::VISUAL_PACK_HASH_LENGTH);
        get_simulation()->network_manager().send_handshake(handshake);
    }

    void network_set_expected_visual_pack(const char* pack_id, uint32_t pack_version, const char* pack_hash) {
        if (!pack_id || !pack_hash) return;
        rts::ConnectionHandshake handshake{};
        handshake.protocol_version = rts::NETWORK_PROTOCOL_VERSION;
        handshake.visual_pack_version = pack_version;
        std::strncpy(reinterpret_cast<char*>(handshake.visual_pack_id), pack_id, rts::VISUAL_PACK_ID_LENGTH);
        std::strncpy(reinterpret_cast<char*>(handshake.visual_pack_hash), pack_hash, rts::VISUAL_PACK_HASH_LENGTH);
        get_simulation()->network_manager().set_expected_handshake(handshake);
    }

    void network_update(float delta_ms) {
        get_simulation()->network_manager().update(delta_ms);
    }
    
    void network_send_command(uint32_t tick, int entity_id, int cmd_type, float target_x, float target_y) {
        InputCommand cmd;
        cmd.tick_id = tick;
        cmd.player_id = static_cast<uint8_t>(entity_id);
        cmd.cmd_type = static_cast<uint8_t>(cmd_type);
        cmd.target_x = static_cast<uint16_t>(target_x);
        cmd.target_y = static_cast<uint16_t>(target_y);
        cmd.extra = 0;
        get_simulation()->network_manager().send_command(cmd);
    }
    
    int network_receive_command(uint32_t* tick, int* entity_id, int* cmd_type, float* target_x, float* target_y) {
        InputCommand cmd;
        if (get_simulation()->network_manager().receive_command(cmd)) {
            *tick = cmd.tick_id;
            *entity_id = static_cast<int>(cmd.player_id);
            *cmd_type = static_cast<int>(cmd.cmd_type);
            *target_x = static_cast<float>(cmd.target_x);
            *target_y = static_cast<float>(cmd.target_y);
            return 1;
        }
        return 0;
    }
    
    void network_set_remote_position(int entity_id, float x, float y) {
        get_simulation()->network_manager().set_remote_position(static_cast<EntityId>(entity_id), x, y);
    }
    
    int network_get_remote_position(int entity_id, float* x, float* y) {
        return get_simulation()->network_manager().get_remote_position(static_cast<EntityId>(entity_id), *x, *y);
    }
    
    uint64_t network_bytes_sent() {
        return get_simulation()->network_manager().bytes_sent();
    }
    
    uint64_t network_bytes_received() {
        return get_simulation()->network_manager().bytes_received();
    }
    
    uint32_t network_packets_sent() {
        return get_simulation()->network_manager().packets_sent();
    }
    
    uint32_t network_packets_received() {
        return get_simulation()->network_manager().packets_received();
    }
    
    float network_ping_ms() {
        return get_simulation()->network_manager().ping_ms();
    }
    
    float network_jitter_ms() {
        return get_simulation()->network_manager().jitter_ms();
    }
    
    float network_packet_loss_pct() {
        return get_simulation()->network_manager().packet_loss_pct();
    }
    
    uint32_t network_packets_lost() {
        return get_simulation()->network_manager().packets_lost();
    }
    
    void network_send_delta_snapshot(uint32_t snapshot_id) {
        get_simulation()->network_manager().send_delta_snapshot(snapshot_id);
    }
    
    int network_receive_delta_snapshot(uint32_t* snapshot_data) {
        DeltaSnapshot* snapshot = reinterpret_cast<DeltaSnapshot*>(snapshot_data);
        return get_simulation()->network_manager().receive_delta_snapshot(*snapshot) ? 1 : 0;
    }
    
    // --- Rendering extern "C" wrappers ---
    
    void render_add_unit(float x, float y, uint32_t unit_type) {
        get_simulation()->renderer().add_unit_instance(x, y, unit_type);
    }
    
    void render_update() {
        get_simulation()->renderer().begin_frame();
        get_simulation()->renderer().draw_all();
        get_simulation()->renderer().end_frame();
    }
    
    void set_debug_mode(bool enabled) {
        get_simulation()->renderer().set_debug_mode(enabled);
    }
    
    int render_get_instance_count() {
        return get_simulation()->renderer().get_instance_count();
    }
    
    // Faction extern "C" wrappers
    void simulation_initialize_faction(int faction_id, float x, float y) {
        rts::Simulation* sim = get_simulation();
        sim->initialize_faction(static_cast<rts::FactionId>(faction_id), x, y);
    }
    
    int simulation_create_unit_with_type(float x, float y, int unit_type, int faction_id) {
        rts::Simulation* sim = get_simulation();
        return sim->create_unit_with_type(x, y, static_cast<rts::UnitType>(unit_type), static_cast<rts::FactionId>(faction_id));
    }
    
    int simulation_issue_move_commands(const int32_t* entity_ids, int entity_count, int player_id, float center_x, float center_y, float spacing, int queued) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_move_commands(entities, static_cast<rts::FactionId>(player_id), center_x, center_y, spacing, queued != 0);
        return static_cast<int>(result);
    }
    
    int simulation_issue_stop_commands(const int32_t* entity_ids, int entity_count, int player_id) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_stop_commands(entities, static_cast<rts::FactionId>(player_id));
        return static_cast<int>(result);
    }
    
    int simulation_issue_attack_commands(const int32_t* entity_ids, int entity_count, int player_id, int64_t target_entity_id) {
        rts::Simulation* sim = get_simulation();
        if (target_entity_id <= 0 || target_entity_id > std::numeric_limits<uint32_t>::max()) return 0;
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_attack_commands(entities, static_cast<rts::FactionId>(player_id), rts::EntityId{static_cast<uint32_t>(target_entity_id)});
        return static_cast<int>(result);
    }
    
    int simulation_issue_patrol_commands(const int32_t* entity_ids, int entity_count, int player_id, float x, float y) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_patrol_commands(entities, static_cast<rts::FactionId>(player_id), x, y);
        return static_cast<int>(result);
    }
    
    int simulation_issue_return_commands(const int32_t* entity_ids, int entity_count, int player_id) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_return_commands(entities, static_cast<rts::FactionId>(player_id));
        return static_cast<int>(result);
    }
    
    int simulation_issue_build_commands(const int32_t* entity_ids, int entity_count, int player_id, float x, float y, int64_t unit_type) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_build_commands(entities, static_cast<rts::FactionId>(player_id), x, y, unit_type);
        return static_cast<int>(result);
    }
    
    int simulation_issue_harvest_commands(const int32_t* entity_ids, int entity_count, int player_id, float x, float y) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_harvest_commands(entities, static_cast<rts::FactionId>(player_id), x, y);
        return static_cast<int>(result);
    }
    
    int simulation_issue_defend_commands(const int32_t* entity_ids, int entity_count, int player_id, float x, float y) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) {
            return 0;
        }
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        size_t result = sim->issue_defend_commands(entities, static_cast<rts::FactionId>(player_id), x, y);
        return static_cast<int>(result);
    }

    int simulation_issue_install_commands(const int32_t* entity_ids, int entity_count, int player_id, float x, float y, int installation_type) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) return 0;
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        if (installation_type != static_cast<int>(rts::InstallationType::FORWARD_OPERATING_BASE)) return 0;
        return static_cast<int>(sim->issue_install_commands(entities, static_cast<rts::FactionId>(player_id), x, y,
            static_cast<rts::InstallationType>(installation_type)));
    }
    
    int simulation_issue_requisition_commands(const int32_t* entity_ids, int entity_count, int player_id, float x, float y, int unit_type) {
        rts::Simulation* sim = get_simulation();
        if (!entity_ids || entity_count <= 0 || entity_count > static_cast<int>(MAX_COMMANDS_PER_TICK) || player_id < 0 || player_id > 2) return 0;
        std::vector<rts::EntityId> entities;
        entities.reserve(static_cast<std::size_t>(entity_count));
        for (int index = 0; index < entity_count; ++index) {
            if (entity_ids[index] <= 0) return 0;
            entities.push_back(rts::EntityId{static_cast<uint32_t>(entity_ids[index])});
        }
        return static_cast<int>(sim->issue_requisition_commands(entities, static_cast<rts::FactionId>(player_id), x, y,
            static_cast<rts::UnitType>(unit_type)));
    }
}

// ===== Faction Initialization (inside namespace rts) =====

namespace rts {

void Simulation::configure_world_size(float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1.0f || height < 1.0f) {
        return;
    }
    world_width_ = width;
    world_height_ = height;

    command_position_scale_ = std::max(width, height) > 327.0f ? 1.0f : INPUT_COMMAND_POSITION_SCALE;

    // Keep navigation bounded at the same 320x320 strategic resolution as the
    // authored heightmap. A 40 km theater therefore uses 125 m cells instead
    // of attempting to allocate a 40,000 x 40,000 one-meter grid.
    constexpr float NAVIGATION_RESOLUTION = 320.0f;
    const float cell_size = std::max(1.0f, std::max(width, height) / NAVIGATION_RESOLUTION);
    const auto grid_width = static_cast<int>(std::ceil(width / cell_size));
    const auto grid_height = static_cast<int>(std::ceil(height / cell_size));
    terrain_.set_world_bounds(width, height);
    pathfinding_ = Pathfinding{grid_width, grid_height, cell_size, -width * 0.5f, -height * 0.5f};
    naval_pathfinding_ = Pathfinding{grid_width, grid_height, cell_size, -width * 0.5f, -height * 0.5f};
    air_pathfinding_ = Pathfinding{grid_width, grid_height, cell_size, -width * 0.5f, -height * 0.5f};
}

void Simulation::configure_theater_landmasses(
    float first_center_x, float first_center_y, float first_width, float first_height,
    float second_center_x, float second_center_y, float second_width, float second_height) {
    auto inside = [](float x, float y, float cx, float cy, float width, float height) {
        return std::abs(x - cx) <= width * 0.5f && std::abs(y - cy) <= height * 0.5f;
    };
    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 320; ++x) {
            const float world_x = pathfinding_.to_world_x(x);
            const float world_y = pathfinding_.to_world_y(y);
            const bool land = inside(world_x, world_y, first_center_x, first_center_y, first_width, first_height) ||
                inside(world_x, world_y, second_center_x, second_center_y, second_width, second_height);
            pathfinding_.set_cell(x, y, land);
            naval_pathfinding_.set_cell(x, y, !land);
        }
    }
    theater_water_rules_enabled_ = true;
}

bool Simulation::is_land_position(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    if (!theater_water_rules_enabled_) return true;
    return pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y));
}

void Simulation::block_civilian_area(float x, float y, float radius) {
    if (!is_land_position(x, y)) return;
    pathfinding_.block_world_area(x, y, radius);
}

bool Simulation::site_claimed(float x, float y, float half_x, float half_y) const {
    // Levelling moves whole height cells, so keep one cell of clearance from
    // ground another structure already stands on.
    const float margin = pathfinding_.cell_size();
    for (const auto& claim : site_claims_) {
        if (std::abs(claim.x - x) < claim.half_x + half_x + margin &&
            std::abs(claim.y - y) < claim.half_y + half_y + margin) {
            return true;
        }
    }
    return false;
}

bool Simulation::command_post_site_claimed(float x, float y, float half_x, float half_y) const {
    for (size_t index = 0; index < FACTION_COUNT; ++index) {
        const EntityId base = production_manager_.faction_line(static_cast<FactionId>(index));
        if (base == INVALID_ENTITY || !entity_manager_.is_alive(base)) continue;
        if (std::abs(get_unit_x(base) - x) < kCommandPostFootprintX * 0.5f + half_x &&
            std::abs(get_unit_y(base) - y) < kCommandPostFootprintY * 0.5f + half_y) {
            return true;
        }
    }
    return false;
}

StructureSite Simulation::assess_structure_site(uint8_t structure_type, float x, float y, bool check_command_post) const {
    StructureSite site{};
    const StructureSiteRules rules = structure_site_rules(structure_type);
    if (rules.footprint_x <= 0.0f || !std::isfinite(x) || !std::isfinite(y)) return site;

    const float half_x = rules.footprint_x * 0.5f;
    const float half_y = rules.footprint_y * 0.5f;
    if (site_claimed(x, y, half_x, half_y) ||
        (check_command_post && command_post_site_claimed(x, y, half_x, half_y))) return site;
    const float sample_x = rules.footprint_x / 4.0f;
    const float sample_y = rules.footprint_y / 4.0f;
    float heights[5][5]{};
    float total_height = 0.0f;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 5; ++column) {
            const float sample_world_x = x - half_x + sample_x * static_cast<float>(column);
            const float sample_world_y = y - half_y + sample_y * static_cast<float>(row);
            const bool inside_world = sample_world_x >= pathfinding_.min_world_x_center() - pathfinding_.cell_size() * 0.5f &&
                sample_world_x <= pathfinding_.max_world_x_center() + pathfinding_.cell_size() * 0.5f &&
                sample_world_y >= pathfinding_.min_world_y_center() - pathfinding_.cell_size() * 0.5f &&
                sample_world_y <= pathfinding_.max_world_y_center() + pathfinding_.cell_size() * 0.5f;
            if (!inside_world) return site;
            if (!is_land_position(sample_world_x, sample_world_y)) return site;
            heights[row][column] = terrain_.height_at(sample_world_x, sample_world_y);
            total_height += heights[row][column];
        }
    }

    float minimum_height = heights[0][0];
    float maximum_height = heights[0][0];
    float maximum_slope = 0.0f;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 5; ++column) {
            minimum_height = std::min(minimum_height, heights[row][column]);
            maximum_height = std::max(maximum_height, heights[row][column]);
            if (column > 0) {
                maximum_slope = std::max(maximum_slope,
                    std::abs(heights[row][column] - heights[row][column - 1]) / sample_x);
            }
            if (row > 0) {
                maximum_slope = std::max(maximum_slope,
                    std::abs(heights[row][column] - heights[row - 1][column]) / sample_y);
            }
        }
    }
    const float height_variation = maximum_height - minimum_height;
    site.pad_height = total_height / 25.0f;
    if (maximum_slope <= rules.direct_slope && height_variation <= rules.direct_height_variation) {
        site.buildable = true;
        return site;
    }
    if (maximum_slope > rules.earthworks_slope || height_variation > rules.earthworks_height_variation) {
        return site;
    }
    float total_depth = 0.0f;
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 5; ++column) {
            total_depth += std::abs(heights[row][column] - site.pad_height);
        }
    }
    const float mean_depth = total_depth / 25.0f;
    site.buildable = true;
    site.earthworks = true;
    site.material = kEarthworksBaseMaterial + mean_depth * rules.earthworks_material_per_meter;
    site.fuel = kEarthworksBaseFuel + mean_depth * rules.earthworks_fuel_per_meter;
    site.seconds = earthworks_seconds(mean_depth);
    return site;
}

bool Simulation::validate_structure_placement(uint8_t structure_type, float x, float y) const {
    return assess_structure_site(structure_type, x, y).buildable;
}

bool Simulation::structure_site_claimed(uint8_t structure_type, float x, float y) const {
    const StructureSiteRules rules = structure_site_rules(structure_type);
    if (rules.footprint_x <= 0.0f || !std::isfinite(x) || !std::isfinite(y)) return false;
    const float half_x = rules.footprint_x * 0.5f;
    const float half_y = rules.footprint_y * 0.5f;
    return site_claimed(x, y, half_x, half_y) || command_post_site_claimed(x, y, half_x, half_y);
}

bool Simulation::queue_structure(EntityId line, FactionId faction, uint8_t structure_type, float x, float y) {
    const StructureSite site = assess_structure_site(structure_type, x, y);
    if (!site.buildable) return false;
    if (!production_manager_.can_queue_structure(line, faction, structure_type, site.material, site.fuel, site.earthworks ? 1 : 0)) {
        return false;
    }
    if (site.earthworks) {
        production_manager_.queue_earthworks(line, faction, structure_type, x, y, site.material, site.fuel, site.seconds);
    }
    production_manager_.queue_structure(line, faction, structure_type, x, y);
    const StructureSiteRules rules = structure_site_rules(structure_type);
    site_claims_.push_back({x, y, rules.footprint_x * 0.5f, rules.footprint_y * 0.5f, site.pad_height});
    return true;
}

bool Simulation::validate_engineer_placement(float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y) || !is_land_position(x, y) ||
        !pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y))) {
        return false;
    }

    // A construction unit needs one usable local building footprint. Requiring
    // valid road probes in both axes rejected otherwise healthy slopes and
    // made engineer spawning much stricter than normal play.
    const StructureSite site = assess_structure_site(0, x, y, false);
    return site.buildable && !site.earthworks;
}

bool Simulation::validate_road_placement(float start_x, float start_y, float end_x, float end_y) const {
    if (!std::isfinite(start_x) || !std::isfinite(start_y) ||
        !std::isfinite(end_x) || !std::isfinite(end_y)) return false;
    const float length = std::hypot(end_x - start_x, end_y - start_y);
    if (length < pathfinding_.cell_size() || length > 12000.0f) return false;

    const int samples = std::max(2, static_cast<int>(std::ceil(length / (pathfinding_.cell_size() * 0.5f))));
    float previous_height = terrain_.height_at(start_x, start_y);
    for (int index = 0; index <= samples; ++index) {
        const float fraction = static_cast<float>(index) / static_cast<float>(samples);
        const float x = std::lerp(start_x, end_x, fraction);
        const float y = std::lerp(start_y, end_y, fraction);
        if (!is_land_position(x, y) ||
            !pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y))) return false;
        const float height = terrain_.height_at(x, y);
        if (index > 0 && std::abs(height - previous_height) /
            (length / static_cast<float>(samples)) > 1.0f) return false;
        previous_height = height;
    }
    return true;
}

bool Simulation::queue_road(EntityId engineer, FactionId owner, float start_x, float start_y,
                            float end_x, float end_y) {
    if (!entity_manager_.is_alive(engineer) ||
        component_manager_.get_component<GroundSteering>(engineer) == nullptr) return false;
    const auto* faction = component_manager_.get_component<Faction>(engineer);
    if (!faction || faction->faction_id != owner || !validate_road_placement(start_x, start_y, end_x, end_y)) return false;

    const float length = std::hypot(end_x - start_x, end_y - start_y);
    const auto& road_settings = get_road_settings();
    if (!production_manager_.deduct_faction_resources(owner, road_settings.material_cost, 0.0f)) return false;
    return road_network_.queue(
        engineer, owner, start_x, start_y, end_x, end_y,
        road_settings.construction_time_base_seconds + length * road_settings.construction_time_per_meter);
}

void Simulation::evict_ground_units(float x, float y, float half_x, float half_y) {
    // block_world_rectangle also takes half a cell beyond the footprint.
    const float cell = pathfinding_.cell_size();
    const float reach_x = half_x + cell;
    const float reach_y = half_y + cell;
    const auto entities = component_manager_.entities_with<Position, GroundSteering>(entity_manager_.get_entities());
    for (const auto entity : entities) {
        if (component_manager_.get_component<Aircraft>(entity) || component_manager_.get_component<NavalVessel>(entity)) continue;
        auto* position = component_manager_.get_component<Position>(entity);
        if (!position || std::abs(position->x - x) > reach_x || std::abs(position->y - y) > reach_y) continue;
        if (pathfinding_.is_walkable(pathfinding_.to_grid_x(position->x), pathfinding_.to_grid_y(position->y))) continue;
        // Try the closest side first, then the others, stepping a little further
        // out each ring in case the edge itself is water or another building.
        struct Exit { float distance; float exit_x; float exit_y; };
        Exit exits[4] = {
            {x + reach_x - position->x, x + reach_x + cell, position->y},
            {position->x - (x - reach_x), x - reach_x - cell, position->y},
            {y + reach_y - position->y, position->x, y + reach_y + cell},
            {position->y - (y - reach_y), position->x, y - reach_y - cell},
        };
        std::sort(std::begin(exits), std::end(exits), [](const Exit& a, const Exit& b) { return a.distance < b.distance; });
        bool moved = false;
        for (int ring = 0; ring < 8 && !moved; ++ring) {
            for (const auto& exit : exits) {
                const float push = cell * static_cast<float>(ring);
                const float out_x = exit.exit_x + (exit.exit_x > x + reach_x ? push : exit.exit_x < x - reach_x ? -push : 0.0f);
                const float out_y = exit.exit_y + (exit.exit_y > y + reach_y ? push : exit.exit_y < y - reach_y ? -push : 0.0f);
                if (!pathfinding_.is_walkable(pathfinding_.to_grid_x(out_x), pathfinding_.to_grid_y(out_y))) continue;
                position->x = out_x;
                position->y = out_y;
                spatial_grid_.update(entity, out_x, out_y);
                if (auto* steering = component_manager_.get_component<GroundSteering>(entity)) steering->current_speed = 0.0f;
                moved = true;
                break;
            }
        }
    }
}

void Simulation::apply_completed_road(const RoadSegment& segment) {
    const float dx = segment.end_x - segment.start_x;
    const float dy = segment.end_y - segment.start_y;
    const float length = std::hypot(dx, dy);
    const int samples = std::max(2, static_cast<int>(std::ceil(length / (pathfinding_.cell_size() * 0.5f))));
    const int cell_radius = std::max(0, static_cast<int>(std::ceil(segment.width / pathfinding_.cell_size())));
    for (int index = 0; index <= samples; ++index) {
        const float fraction = static_cast<float>(index) / static_cast<float>(samples);
        const float x = std::lerp(segment.start_x, segment.end_x, fraction);
        const float y = std::lerp(segment.start_y, segment.end_y, fraction);
        const int center_x = pathfinding_.to_grid_x(x);
        const int center_y = pathfinding_.to_grid_y(y);
        for (int row = center_y - cell_radius; row <= center_y + cell_radius; ++row) {
            for (int column = center_x - cell_radius; column <= center_x + cell_radius; ++column) {
                if (pathfinding_.is_walkable(column, row)) {
                    pathfinding_.set_traversal_cost(column, row, get_road_settings().traversal_cost);
                }
            }
        }
    }
}

EntityId Simulation::create_faction_base(FactionId faction, float x, float y) {
    const auto data = get_faction_start_data().find(faction);
    if (data == get_faction_start_data().end() || !std::isfinite(x) || !std::isfinite(y) ||
        !pathfinding_.is_walkable(pathfinding_.to_grid_x(x), pathfinding_.to_grid_y(y)) ||
        production_manager_.faction_line(faction) != INVALID_ENTITY) return INVALID_ENTITY;
    const auto id = create_unit(x, y).id;
    set_unit_faction(id, faction);
    component_manager_.remove_component<Weapon>(id);
    component_manager_.add_component(id, UnitData{10, 40});
    component_manager_.add_component(id, Health{1500, 1500});
    const auto& start = data->second;
    production_manager_.add_storage(id, Storage{x, y, start.start_material, start.start_energy,
        10000, 10000});
    ProductionLine line{}; line.storage_id = id; line.max_jobs = 5;
    production_manager_.add_production_line(id, line);
    production_manager_.add_faction_production_line(faction, id);
    // Starting commanders do not generate resources. Economy comes from
    // territory sites claimed with harvest/extraction commands.
    return id;
}

void Simulation::initialize_faction(FactionId faction_id, float x, float y) {
    // Get faction start data
    const auto& faction_data = get_faction_start_data();
    auto it = faction_data.find(faction_id);
    if (it == faction_data.end()) {
        std::cerr << "Unknown faction ID: " << static_cast<int>(faction_id) << std::endl;
        return;
    }
    
    const auto& start_data = it->second;
    
    // Create storage for the faction
    int storage_id = static_cast<int>(faction_id) * 10000;
    rts::Storage storage{};
    storage.x = x;
    storage.y = y;
    storage.metal_storage = start_data.start_material;
    storage.energy_storage = start_data.start_energy;
    storage.metal_capacity = 10000.0f;
    storage.energy_capacity = 10000.0f;
    
    production_manager_.add_storage(static_cast<EntityId>(storage_id), storage);
    
    // Create production line for the faction
    int line_id = static_cast<int>(faction_id) * 10000 + 1;
    rts::ProductionLine line{};
    line.storage_id = static_cast<EntityId>(storage_id);
    line.build_speed_metal = 1.0f;
    line.build_speed_energy = 1.0f;
    line.active_jobs = 0;
    line.max_jobs = 5;
    
    production_manager_.add_production_line(static_cast<EntityId>(line_id), line);
    production_manager_.add_faction_production_line(faction_id, static_cast<EntityId>(line_id));
    
    
    // Spawn starting units
    for (size_t i = 0; i < start_data.start_units.size(); ++i) {
        const float offset_x = static_cast<float>((i * 7) % 20) - 10.0f;
        const float offset_y = static_cast<float>((i * 11) % 20) - 10.0f;
        create_unit_with_type(x + offset_x, y + offset_y, start_data.start_units[i], faction_id);
    }
}

int rts::Simulation::create_unit_with_type(float x, float y, rts::UnitType unit_type, rts::FactionId faction_id) {
    if (!std::isfinite(x) || !std::isfinite(y) || at_unit_cap(faction_id)) {
        return -1;
    }

    // Get unit prototype
    const auto& prototypes = get_unit_prototypes();
    auto proto_it = prototypes.find(unit_type);
    if (proto_it == prototypes.end()) {
        std::cerr << "Unknown unit type: " << static_cast<int>(unit_type) << std::endl;
        return -1;
    }

    const auto& proto = proto_it->second;
    if (proto.construction_unit && !validate_engineer_placement(x, y)) {
        return -1;
    }
    const auto& navigation = proto.is_naval ? naval_pathfinding_ : pathfinding_;
    const bool inside_theater = x >= navigation.min_world_x_center() - navigation.cell_size() &&
        x <= navigation.max_world_x_center() + navigation.cell_size() &&
        y >= navigation.min_world_y_center() - navigation.cell_size() &&
        y <= navigation.max_world_y_center() + navigation.cell_size();
    // Runway aircraft are ferried into the theater from an external airfield;
    // their initial off-map position is intentionally outside the land/naval
    // spawn masks and must not be treated as a water spawn.
    if (theater_water_rules_enabled_ && inside_theater && !proto.is_aircraft &&
        !navigation.is_walkable(navigation.to_grid_x(x), navigation.to_grid_y(y))) return -1;

    Entity entity = entity_manager_.create_entity();
    if (static_cast<size_t>(faction_id) < FACTION_COUNT) ++unit_counts_[static_cast<size_t>(faction_id)];
    
    float terrain_height = terrain_.height_at(x, y);
    Position pos = {x, y, terrain_height};
    // Newly spawned units must remain at their authored spawn point until an
    // authoritative command or autonomous system assigns a destination.
    Velocity vel{0.0f, 0.0f, 0.0f};
    Health hp = {proto.hp, proto.hp};
    
    Material mat = {100.0f, 1000.0f, 0.0f};
    
    Faction faction = {faction_id};
    
    const auto& weapon = proto.weapon;
    // A deterministic per-unit offset keeps an army from firing as one volley,
    // which would put every target search on the same tick.
    const float first_shot_delay = weapon.fire_rate * (0.25f + 0.75f * static_cast<float>(entity.id % 97u) / 96.0f);
    Weapon wep = {weapon.damage, weapon.range, weapon.fire_rate, 0.0f,
                  weapon.projectile_speed, weapon.lead_bias, weapon.aoe_radius, first_shot_delay,
                  weapon.hits_ground, weapon.hits_air};
    
    component_manager_.add_component(entity.id, pos);
    component_manager_.add_component(entity.id, vel);
    component_manager_.add_component(entity.id, hp);
    component_manager_.add_component(entity.id, mat);
    component_manager_.add_component(entity.id, faction);
    component_manager_.add_component(entity.id, wep);
    component_manager_.add_component(entity.id, UnitData{proto.speed, proto.view_range, static_cast<int>(unit_type), proto.intel_range});
    if (!proto.is_aircraft && !proto.is_naval) {
        component_manager_.add_component(entity.id, GroundSteering{
            0.0f, 0.0f, 0.0f,
            proto.steering_acceleration, proto.steering_deceleration,
            proto.steering_turn_rate, proto.steering_turn_rate_at_speed,
            proto.steering_minimum_turn_radius, proto.steering_max_reverse_speed,
            proto.steering_reverse_preference_threshold, proto.steering_response,
            proto.steering_maneuver_turn_rate,
            proto.steering_can_pivot_turn,
            proto.footprint_radius
        });
        component_manager_.add_component(entity.id, OffRoadWear{0.0f, 0.0f, unit_type});
    }

    // Territorial capabilities are authored with the unit prototype. Keep a
    // small compatibility fallback for older content files that predate the
    // optional capabilities array.
    if (!proto.capabilities.empty()) {
        for (const auto capability : proto.capabilities) {
            territorial_control_.assign_capability(entity, capability);
        }
    } else {
        territorial_control_.assign_capability(entity, SeizureCapability::SEIZURE);
        territorial_control_.assign_capability(entity, SeizureCapability::SECURE);
        territorial_control_.assign_capability(entity, SeizureCapability::DEFEND);
    }
    if (proto.is_naval) {
        component_manager_.add_component(entity.id, NavalVessel{x,y,proto.operational_energy,proto.operational_energy,proto.energy_consumption_rate,false});
    }
    if (proto.is_aircraft) {
        Aircraft aircraft{};
        aircraft.x = x;
        aircraft.y = y;
        aircraft.fuel = proto.operational_energy;
        aircraft.max_fuel = proto.operational_energy;
        aircraft.fuel_consumption_rate = proto.energy_consumption_rate;
        aircraft.material = proto.operational_material;
        aircraft.max_material = proto.operational_material;
        aircraft.material_consumption_rate = proto.material_consumption_rate;
        aircraft.ammunition = proto.payload;
        aircraft.max_ammunition = proto.payload;
        aircraft.cruise_speed = proto.speed;
        aircraft.airborne_time_ms = 0.0f;
        aircraft.max_airborne_time_ms = proto.max_airborne_time_seconds * 1000.0f;
        aircraft.range = proto.operational_range;
        aircraft.status = Aircraft::Status::ON_GROUND;
        aircraft.mission = Aircraft::Mission::REFUEL;
        aircraft.type = proto.requires_runway
            ? Aircraft::Type::CONVENTIONAL
            : Aircraft::Type::VTOL;
        aircraft.target_base = INVALID_ENTITY;
        aircraft.closest_recovery_facility = INVALID_ENTITY;
        aircraft.predicted_return_energy = std::numeric_limits<float>::infinity();
        aircraft.predicted_return_material = std::numeric_limits<float>::infinity();
        aircraft.predicted_return_time_ms = std::numeric_limits<float>::infinity();
        aircraft.safe_return = !proto.requires_runway;
        aircraft.queue_slot = -1;
        component_manager_.add_component(entity.id, aircraft);
    }
    
    spatial_grid_.insert(entity.id, x, y);
    
    return static_cast<int>(entity.id);
}

void rts::Simulation::set_unit_faction(EntityId entity, FactionId faction_id) {
    auto* faction = component_manager_.get_component<Faction>(entity);
    if (faction) {
        faction->faction_id = faction_id;
    }
}



void rts::Simulation::process_commands() {
    const auto local_commands = command_manager_.get_local_commands();
    command_manager_.clear_local_commands();
    for (size_t i = 0; i < local_commands.size();) {
        const auto& cmd = local_commands[i++];
        if (!validate_command(cmd, tick_)) continue;
        if (network_manager_.is_connected()) network_manager_.send_command(cmd);
        command_log_.push_back(cmd);
        match_record_.add_command(cmd);
        if (cmd.cmd_type == static_cast<uint8_t>(CommandType::MOVE) && cmd.extra > 0) {
            std::vector<EntityId> formation{cmd.entity_id};
            while (i < local_commands.size()) {
                const auto& next = local_commands[i];
                if (next.cmd_type != cmd.cmd_type || next.player_id != cmd.player_id ||
                    next.target_x != cmd.target_x || next.target_y != cmd.target_y || next.extra != cmd.extra) break;
                ++i;
                if (validate_command(next, tick_) && std::find(formation.begin(), formation.end(), next.entity_id) == formation.end()) {
                    if (network_manager_.is_connected()) network_manager_.send_command(next);
                    formation.push_back(next.entity_id);
                    command_log_.push_back(next);
                    match_record_.add_command(next);
                }
            }
            move_units_formation(formation, static_cast<float>(cmd.target_x) / command_position_scale_,
                                 static_cast<float>(cmd.target_y) / command_position_scale_,
                                 (cmd.extra & ~MOVE_QUEUE_FLAG) / command_position_scale_,
                                 (cmd.extra & MOVE_QUEUE_FLAG) != 0);
        } else {
            process_command_internal(cmd);
        }
    }
}

void rts::Simulation::process_network_commands() {
    InputCommand cmd{};
    while (network_manager_.receive_command(cmd)) {
        if (!validate_command(cmd, tick_)) continue;
        command_log_.push_back(cmd);
        match_record_.add_command(cmd);
        process_command_internal(cmd);
    }
}

void rts::Simulation::process_command_internal(const InputCommand& cmd) {
    if (!validate_command(cmd, tick_)) return;
    
    switch (cmd.cmd_type) {
        case static_cast<uint8_t>(CommandType::MOVE): {
            if (auto* aircraft=component_manager_.get_component<Aircraft>(cmd.entity_id);
                aircraft && aircraft->status==Aircraft::Status::ON_GROUND) {
                if (aircraft->type==Aircraft::Type::VTOL) logistics_manager_.launch_vtol(cmd.entity_id);
                else logistics_manager_.queue_aircraft_for_takeoff(cmd.entity_id,
                    logistics_manager_.find_nearest_aircraft_recovery_facility(aircraft->x,aircraft->y,cmd.entity_id));
            }
            const float target_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float target_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            
            if ((cmd.extra & MOVE_QUEUE_FLAG) != 0) {
                queue_unit_move_with_route(static_cast<EntityId>(cmd.entity_id), target_x, target_y, target_x, target_y);
            } else if (cmd.extra != 0) {
                const float spacing = static_cast<float>(cmd.extra) / command_position_scale_;
                const float route_x = target_x + spacing;
                const float route_y = target_y + spacing;
                move_unit_with_route(static_cast<EntityId>(cmd.entity_id), target_x, target_y, route_x, route_y);
            } else {
                move_unit(static_cast<EntityId>(cmd.entity_id), target_x, target_y);
            }
            break;
        }
        case static_cast<uint8_t>(CommandType::ATTACK): {
            const float target_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float target_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            attack_unit(static_cast<EntityId>(cmd.entity_id), static_cast<EntityId>(cmd.extra));
            break;
        }
        case static_cast<uint8_t>(CommandType::STOP): {
            stop_unit(cmd.entity_id);
            break;
        }
        case static_cast<uint8_t>(CommandType::BUILD): {
            const float build_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float build_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            if (cmd.extra == FUEL_SHIPMENT_ORDER_FLAG) {
                if (const auto* owner = component_manager_.get_component<Faction>(cmd.entity_id)) request_fuel_shipment(owner->faction_id);
                break;
            }
            if ((cmd.extra & PARADROP_ORDER_FLAG) != 0) {
                const auto* owner = component_manager_.get_component<Faction>(cmd.entity_id);
                if (owner) order_paradrop(owner->faction_id, static_cast<UnitType>(cmd.extra & 0xFFu), build_x, build_y);
                break;
            }
            if ((cmd.extra & (ROAD_DELIVERY_ORDER_FLAG | RAIL_DELIVERY_ORDER_FLAG)) != 0) {
                const auto* owner = component_manager_.get_component<Faction>(cmd.entity_id);
                if (owner) order_delivery_via(owner->faction_id, static_cast<UnitType>(cmd.extra & 0xFFu),
                    (cmd.extra & ROAD_DELIVERY_ORDER_FLAG) != 0 ? GroundTransport::ROAD : GroundTransport::RAIL);
                break;
            }
            if ((cmd.extra & STRUCTURE_ORDER_FLAG) != 0) {
                const auto* owner = component_manager_.get_component<Faction>(cmd.entity_id);
                if (owner) {
                    queue_structure(production_manager_.faction_line(owner->faction_id), owner->faction_id,
                                    static_cast<uint8_t>(cmd.extra & 0xFFu), build_x, build_y);
                }
                break;
            }
            const UnitType unit_type = static_cast<UnitType>(cmd.extra);
            build_structure(static_cast<EntityId>(cmd.entity_id), build_x, build_y, unit_type);
            break;
        }
        case static_cast<uint8_t>(CommandType::INSTALL): {
            const float install_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float install_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            const InstallationType installation_type = static_cast<InstallationType>(cmd.extra);
            install_fob(static_cast<EntityId>(cmd.entity_id), install_x, install_y, installation_type);
            break;
        }
        case static_cast<uint8_t>(CommandType::REQUISITION): {
            const float requisition_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float requisition_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            const UnitType unit_type = static_cast<UnitType>(cmd.extra);
            requisition_unit(static_cast<EntityId>(cmd.entity_id), requisition_x, requisition_y, unit_type);
            break;
        }
        case static_cast<uint8_t>(CommandType::HARVEST): {
            const float harvest_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float harvest_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            if ((cmd.extra & POWER_ORDER_FLAG) != 0) order_power_work(static_cast<EntityId>(cmd.entity_id), cmd.extra);
            else harvest_resource(static_cast<EntityId>(cmd.entity_id), harvest_x, harvest_y);
            break;
        }
        case static_cast<uint8_t>(CommandType::RETURN): {
            return_unit(static_cast<EntityId>(cmd.entity_id));
            break;
        }
        case static_cast<uint8_t>(CommandType::DEFEND): {
            const float defend_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float defend_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            defend_area(static_cast<EntityId>(cmd.entity_id), defend_x, defend_y);
            break;
        }
        case static_cast<uint8_t>(CommandType::PATROL): {
            const float patrol_x = static_cast<float>(cmd.target_x) / command_position_scale_;
            const float patrol_y = static_cast<float>(cmd.target_y) / command_position_scale_;
            patrol_unit(static_cast<EntityId>(cmd.entity_id), patrol_x, patrol_y);
            break;
        }
        default:
            break;
    }
}


} // namespace rts

using namespace rts;

extern "C" {
    // AI bindings share the same owner as fixed-tick simulation updates.
    void ai_init() { get_simulation()->ai_manager().reset(); }
    void ai_update(float delta_ms) { get_simulation()->ai_manager().update(delta_ms); }
    void ai_reset() { get_simulation()->ai_manager().reset(); }
    void ai_set_faction_id(int faction_id) {
        if (faction_id < 0 || faction_id > static_cast<int>(FactionId::INDUSTRIAL_EXPERIMENTAL)) return;
        get_simulation()->ai_manager().set_faction_id(static_cast<FactionId>(faction_id));
    }
    int ai_get_visible_unit_count() {
        return static_cast<int>(get_simulation()->ai_manager().get_visible_units().size());
    }
    int ai_get_enemy_unit_count() {
        return static_cast<int>(get_simulation()->ai_manager().get_enemy_units().size());
    }

    // Economy getters for GDExtension
    int economy_get_resource_node_count() {
        return static_cast<int>(get_simulation()->production_manager().resource_nodes().size());
    }
    
    bool economy_get_resource_node_info(int node_id, float* out_x, float* out_y, float* out_amount, int* out_type) {
        auto it = get_simulation()->production_manager().resource_nodes().find(static_cast<EntityId>(node_id));
        if (it == get_simulation()->production_manager().resource_nodes().end()) {
            return false;
        }
        *out_x = it->second.x;
        *out_y = it->second.y;
        *out_amount = it->second.amount;
        *out_type = static_cast<int>(it->second.type);
        return true;
    }
    
    bool economy_get_storage_info(int storage_id, float* out_metal_storage, float* out_energy_storage,
                                  float* out_metal_capacity, float* out_energy_capacity) {
        auto it = get_simulation()->production_manager().storages().find(static_cast<EntityId>(storage_id));
        if (it == get_simulation()->production_manager().storages().end()) {
            return false;
        }
        *out_metal_storage = it->second.metal_storage;
        *out_energy_storage = it->second.energy_storage;
        *out_metal_capacity = it->second.metal_capacity;
        *out_energy_capacity = it->second.energy_capacity;
        return true;
    }

    bool economy_get_tactical_resources(int storage_id, float* out_materials, float* out_materials_capacity,
                                        float* out_fuel, float* out_fuel_capacity,
                                        float* out_power_generation_mw, float* out_power_demand_mw,
                                        float* out_power_reserve_mw) {
        if (!out_materials || !out_materials_capacity || !out_fuel || !out_fuel_capacity ||
            !out_power_generation_mw || !out_power_demand_mw || !out_power_reserve_mw) return false;
        const auto* resources = get_simulation()->production_manager().tactical_resources(static_cast<EntityId>(storage_id));
        if (!resources) return false;
        *out_materials = resources->materials.current;
        *out_materials_capacity = resources->materials.max;
        *out_fuel = resources->fuel.current;
        *out_fuel_capacity = resources->fuel.capacity;
        *out_power_generation_mw = resources->power.generation_mw;
        *out_power_demand_mw = resources->power.demand_mw;
        *out_power_reserve_mw = resources->power.reserve_mw();
        return true;
    }
    
    int economy_get_queue_size(int line_id) {
        auto it = get_simulation()->production_manager().production_lines().find(static_cast<EntityId>(line_id));
        if (it == get_simulation()->production_manager().production_lines().end()) {
            return 0;
        }
        return static_cast<int>(it->second.queue.size());
    }
    
    int economy_get_completed_build_count() {
        return get_simulation()->production_manager().get_construction_count();
    }
}

extern "C" {
    // Combat getters for GDExtension
    bool combat_get_unit_health(int entity_id, float* out_current, float* out_max) {
        if (!out_current || !out_max) return false;
        float current, max;
        if (!::simulation_get_unit_health(entity_id, &current, &max)) {
            return false;
        }
        *out_current = current;
        *out_max = max;
        return true;
    }
    
    int combat_get_unit_is_dead(int entity_id) {
        return ::simulation_get_unit_is_dead(entity_id);
    }
    
    bool combat_apply_damage(int entity_id, float damage) {
        return get_simulation()->apply_damage(static_cast<EntityId>(entity_id), damage);
    }
    
    int combat_get_unit_faction_id(int entity_id) {
        FactionId faction_id;
        if (!get_simulation()->get_unit_faction_id(static_cast<EntityId>(entity_id), faction_id)) {
            return static_cast<int>(FactionId::ELITE_PRECISION);
        }
        return static_cast<int>(faction_id);
    }
}

extern "C" {
    // Logistics getters for GDExtension
    int logistics_carrier_deck_occupancy(int facility_id) {
        return static_cast<int>(get_simulation()->logistics_manager().carrier_deck_occupancy(static_cast<EntityId>(facility_id)));
    }
    
    int logistics_takeoff_queue_size(int facility_id) {
        return static_cast<int>(get_simulation()->logistics_manager().takeoff_queue_size(static_cast<EntityId>(facility_id)));
    }
    
    int logistics_landing_queue_size(int facility_id) {
        return static_cast<int>(get_simulation()->logistics_manager().landing_queue_size(static_cast<EntityId>(facility_id)));
    }
    
    int logistics_active_runway_operations(int facility_id) {
        return static_cast<int>(get_simulation()->logistics_manager().active_runway_operations(static_cast<EntityId>(facility_id)));
    }
    
    bool logistics_is_safe_return(int entity_id) {
        return get_simulation()->logistics_manager().is_safe_return(static_cast<EntityId>(entity_id), get_simulation()->pathfinding());
    }
    
    int logistics_get_intelligence_age(int entity_id) {
        Intelligence* intel = get_simulation()->logistics_manager().get_intelligence(static_cast<EntityId>(entity_id));
        if (!intel) return -1;
        return static_cast<int>(get_simulation()->simulation_tick() - intel->last_seen_tick);
    }
    
    bool logistics_is_intelligence_stale(int entity_id) {
        Intelligence* intel = get_simulation()->logistics_manager().get_intelligence(static_cast<EntityId>(entity_id));
        if (!intel) return false;
        return (get_simulation()->simulation_tick() - intel->last_seen_tick) > 100;
    }
}


extern "C" {
    int territory_get_zone_count() {
        return static_cast<int>(get_simulation()->territorial_control_manager().zone_count());
    }
    
    bool territory_get_zone_info(int zone_id, float* out_x, float* out_y, int* out_state, int* out_type, int* out_security) {
        if (zone_id < 0 || zone_id >= territory_get_zone_count()) return false;
        if (!out_x || !out_y || !out_state || !out_type || !out_security) return false;
        
        auto& manager = get_simulation()->territorial_control_manager();
        const auto& zones = manager.get_zones();
        if (zone_id >= static_cast<int>(zones.size())) return false;
        
        const auto& zone = zones[zone_id];
        *out_x = zone.center_x;
        *out_y = zone.center_y;
        *out_state = static_cast<int>(zone.state);
        *out_type = static_cast<int>(zone.type);
        *out_security = static_cast<int>(zone.security_score);
        return true;
    }

    bool territory_get_installation_info(float x, float y, int* out_type, int* out_faction,
                                         int* out_active, int* out_constructing,
                                         float* out_progress, float* out_cost) {
        if (!out_type || !out_faction || !out_active || !out_constructing || !out_progress || !out_cost) return false;
        const auto state = get_simulation()->territorial_control_manager().get_installation_at(x, y);
        if (!state.active && !state.constructing) return false;
        *out_type = static_cast<int>(state.type);
        *out_faction = static_cast<int>(state.faction);
        *out_active = state.active ? 1 : 0;
        *out_constructing = state.constructing ? 1 : 0;
        *out_progress = state.construction_progress;
        *out_cost = state.construction_cost;
        return true;
    }
}
