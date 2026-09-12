#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <iostream>

#include "simulation/simulation.hpp"
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
    logistics_manager_.set_component_manager(&component_manager_);
    network_manager_.set_simulation(this);
    ai_manager_ = std::make_unique<AIManager>();
    ai_manager_->set_simulation(this);
}

void Simulation::start() {
    // Reset simulation state for fresh world
    clear_entities();
    running_ = true;
    tick_ = 0;
    elapsed_ms_ = 0.0f;
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
    faction_research_.clear();
    territorial_control_.reset();
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
    patrol_orders_.clear();
    component_manager_.cleanup_all();
    spatial_grid_.clear();
    entity_manager_.clear();
}

void Simulation::update(float delta_ms) {
    if (!running_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }

    elapsed_ms_ = std::min(elapsed_ms_ + delta_ms, MAX_CATCH_UP_MS);
    
    while (elapsed_ms_ >= 50.0f) {
        const auto tick_start = std::chrono::steady_clock::now();
        tick_++;
        process_commands();
        prediction_phase(50.0f);
        combat_phase(50.0f);
        environment_phase(50.0f);
        logistics_phase(50.0f);
        economy_phase(50.0f);
        if (ai_enabled_) ai_manager_->update(50.0f);
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

    Entity entity = entity_manager_.create_entity();
    
    Position pos = {x, y, 0.0f};
    Velocity vel = {0.0f, 0.0f, 0.0f};
    Health hp = {100.0f, 100.0f};
    
    Material mat = {100.0f, 1000.0f, 0.0f};
    Energy en = {100.0f, 1000.0f, 0.0f, 0.0f};
    Research rs = {0.0f, 1000.0f, 0.0f};
    
    Faction faction = {FactionId::ELITE_PRECISION};
    
    Weapon wep = {10.0f, 100.0f, 1.0f, 0.0f, 200.0f, 0.5f, 0.0f, 0.0f};
    
    component_manager_.add_component(entity.id, pos);
    component_manager_.add_component(entity.id, vel);
    component_manager_.add_component(entity.id, hp);
    component_manager_.add_component(entity.id, mat);
    component_manager_.add_component(entity.id, en);
    component_manager_.add_component(entity.id, rs);
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
    auto& navigation = navigation_for(entity);
    auto* position = component_manager_.get_component<Position>(entity);
    auto* velocity = component_manager_.get_component<Velocity>(entity);
    if (!position || !velocity || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(route_x) || !std::isfinite(route_y) ||
        !navigation.is_walkable(navigation.to_grid_x(x), navigation.to_grid_y(y)) ||
        !navigation.is_walkable(navigation.to_grid_x(route_x), navigation.to_grid_y(route_y))) {
        return;
    }

    if(auto* aircraft=component_manager_.get_component<Aircraft>(entity);
       aircraft && aircraft->status==Aircraft::Status::ON_GROUND) {
        if(aircraft->type==Aircraft::Type::VTOL) logistics_manager_.launch_vtol(entity);
        else logistics_manager_.queue_aircraft_for_takeoff(entity,
            logistics_manager_.find_nearest_aircraft_recovery_facility(aircraft->x,aircraft->y,entity));
    }
    move_targets_[entity] = {
        {x, y, position->z},
        {route_x, route_y, position->z}
    };
}

void Simulation::move_units_formation(
    const std::vector<EntityId>& entities,
    float center_x,
    float center_y,
    float spacing) {
    if (entities.empty() || !std::isfinite(center_x) || !std::isfinite(center_y) ||
        !std::isfinite(spacing) || spacing <= 0.0f) {
        return;
    }

    const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(entities.size()))));
    const int rows = static_cast<int>((entities.size() + static_cast<std::size_t>(columns) - 1) /
                                      static_cast<std::size_t>(columns));
    float effective_spacing = spacing;
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
        move_unit_with_route(entities[index], arrival_x, arrival_y, route_x, route_y);
    }
}

void Simulation::stop_unit(EntityId entity) {
    patrol_orders_.erase(entity);
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
        auto funds = production_manager_.storages().find(base);
        const float needed = vessel->max_fuel - vessel->fuel;
        if (funds != production_manager_.storages().end() && funds->second.energy_storage >= needed) {
            funds->second.energy_storage -= needed;
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
    if (faction) production_manager_.queue_unit(entity, faction->faction_id, unit_type, x, y);
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

void Simulation::harvest_resource(EntityId entity, float x, float y) {
    EntityId extractor_id, node_id;
    if (!production_manager_.find_or_create_extractor(x, y, extractor_id, node_id)) {
        return;
    }
    const auto* faction = component_manager_.get_component<Faction>(entity);
    if (!faction) return;
    const EntityId storage_id = production_manager_.faction_line(faction->faction_id);
    if (storage_id == INVALID_ENTITY) return;
    auto extractor = production_manager_.extractors().find(extractor_id);
    if (extractor == production_manager_.extractors().end()) return;
    // A field facility is a territorial objective.  Claiming it reroutes its
    // single shared Materials output to the claimant's command storage.
    extractor->second.storage_id = storage_id;
    
    component_manager_.add_component(entity, Harvester{node_id, 0.0f, 0.0f});
    move_unit(entity, x, y);
}

bool Simulation::destroy_resource_site(EntityId entity, float x, float y) {
    if (!component_manager_.get_component<Faction>(entity)) return false;
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

std::string Simulation::research_id(uint32_t index) {
    std::vector<std::string> ids;
    for (const auto& [id, project] : get_research_projects()) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return index < ids.size() ? ids[index] : "";
}

bool Simulation::is_visible_to(FactionId faction, EntityId target) const {
    const auto* position = component_manager_.get_component<Position>(target);
    const auto* hp = component_manager_.get_component<Health>(target);
    if (!position || !hp || hp->is_dead || hp->current <= 0) return false;
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
        if (dist_sq < nearest_dist_sq) {
            nearest_dist_sq = dist_sq;
            nearest = enemy;
        }
    }
    return nearest;
}

bool Simulation::is_position_visible_to(FactionId faction, float x, float y) const {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
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
    if (cmd.tick_id != execution_tick || cmd.player_id > 2 ||
        cmd.cmd_type > static_cast<uint8_t>(CommandType::RESEARCH)) {
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
        return cmd.extra == 0;
    }
    const auto type = static_cast<CommandType>(cmd.cmd_type);
    if (type != CommandType::MOVE && type != CommandType::PATROL && type != CommandType::DEFEND &&
        (cmd.target_x != 0 || cmd.target_y != 0)) {
        fprintf(stderr, "VALIDATE_FAIL: invalid target for command type (entity=%u type=%u target=(%d,%d))\n", cmd.entity_id, cmd.cmd_type, cmd.target_x, cmd.target_y);
        return false;
    }
    const auto* unit = component_manager_.get_component<UnitData>(cmd.entity_id);
    if (type == CommandType::ATTACK) {
        const auto* target = component_manager_.get_component<Faction>(cmd.extra);
        const auto* hp = component_manager_.get_component<Health>(cmd.extra);
        return target && hp && !hp->is_dead && hp->current > 0 && target->faction_id != owner->faction_id &&
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
            const float dx=vessel->x-position->x,dy=vessel->y-position->y;
            bool ok = dx*dx+dy*dy<=80*80 && funds->second.energy_storage>=vessel->max_fuel-vessel->fuel;
            if (!ok) fprintf(stderr, "VALIDATE_FAIL: RETURN naval position/fuel check failed (entity=%u dist=%.2f fuel=%.2f)\n", cmd.entity_id, sqrt(dx*dx+dy*dy), funds->second.energy_storage - vessel->max_fuel + vessel->fuel);
            return ok;
        }

        const auto* aircraft = component_manager_.get_component<Aircraft>(cmd.entity_id);
        bool ok = aircraft && aircraft->status != Aircraft::Status::CRASHED;
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: RETURN aircraft check failed (entity=%u has_aircraft=%d status=%d)\n", cmd.entity_id, !!aircraft, aircraft ? (int)aircraft->status : -1);
        return ok;
    }
    if (type == CommandType::BUILD) {
        bool ok = cmd.extra <= 255 && production_manager_.can_queue_unit(cmd.entity_id, owner->faction_id, static_cast<UnitType>(cmd.extra));
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: BUILD check failed (entity=%u extra=%u type=%u)\n", cmd.entity_id, cmd.extra, static_cast<UnitType>(cmd.extra));
        return ok;
    }
    if (type == CommandType::RESEARCH) {
        bool ok = production_manager_.faction_line(owner->faction_id) == cmd.entity_id &&
                production_manager_.can_research(owner->faction_id, research_id(cmd.extra));
        if (!ok) fprintf(stderr, "VALIDATE_FAIL: RESEARCH check failed (entity=%u extra=%u)\n", cmd.entity_id, cmd.extra);
        return ok;
    }
    if (unit && unit->speed <= 0) {
        fprintf(stderr, "VALIDATE_FAIL: unit speed <= 0 (entity=%u speed=%.1f)\n", cmd.entity_id, unit->speed);
        return false;
    }
    const auto& navigation = navigation_for(cmd.entity_id);
    const float x = decode_input_command_position(cmd.target_x);
    const float y = decode_input_command_position(cmd.target_y);
    if (type != CommandType::DEFEND && type != CommandType::HARVEST) {
        int gx = navigation.to_grid_x(x);
        int gy = navigation.to_grid_y(y);
        if (!navigation.is_walkable(gx, gy)) {
            fprintf(stderr, "VALIDATE_FAIL: position not walkable (entity=%u x=%.2f y=%.2f grid=%d,%d)\n", cmd.entity_id, x, y, gx, gy);
            return false;
        }
    }
    if (type == CommandType::MOVE) {
        if (cmd.extra > 10000) {
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
    if (!encode_input_command_position(x, encoded_x) || !encode_input_command_position(y, encoded_y) ||
        entities.size() > MAX_COMMANDS_PER_TICK) return 0;
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
                                      float x, float y, float spacing) {
    if (!std::isfinite(spacing) || spacing <= 0 || spacing > 100) return 0;
    return issue_commands(ids, player, CommandType::MOVE, x, y,
                          static_cast<uint32_t>(std::round(spacing * INPUT_COMMAND_POSITION_SCALE)));
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
size_t Simulation::issue_harvest_commands(const std::vector<EntityId>& ids, FactionId player, float x, float y) {
    return issue_commands(ids, player, CommandType::HARVEST, x, y);
}
size_t Simulation::issue_defend_commands(const std::vector<EntityId>& ids, FactionId player, float x, float y) {
    return issue_commands(ids, player, CommandType::DEFEND, x, y);
}

void Simulation::render_add_unit(float x, float y, uint32_t unit_type) {
    renderer_.add_unit_instance(x, y, unit_type);
}

void Simulation::render_update() {
    renderer_.begin_frame();
    renderer_.draw_all();
    renderer_.end_frame();
}

void Simulation::prediction_phase(float delta_ms) {
    const float dt = delta_ms / 1000.0f;
    const auto entities = component_manager_.entities_with<Position, Velocity>(entity_manager_.get_entities());

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

        auto& navigation = navigation_for(entity_id);
        auto target = move_targets_.find(entity_id);
        if (target != move_targets_.end()) {
            const float dx = target->second.arrival.x - pos->x;
            const float dy = target->second.arrival.y - pos->y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const auto* data = component_manager_.get_component<UnitData>(entity_id);
            const float move_speed = data ? data->speed : 12.0f;
            const float max_step = move_speed * dt;

            if (distance <= max_step || distance < 0.001f) {
                pos->x = target->second.arrival.x;
                pos->y = target->second.arrival.y;
                vel->x = 0.0f;
                vel->y = 0.0f;
                move_targets_.erase(target);
            } else {
                if (navigation.has_line_of_sight(
                        pos->x,
                        pos->y,
                        target->second.arrival.x,
                        target->second.arrival.y)) {
                    vel->x = dx / distance * move_speed;
                    vel->y = dy / distance * move_speed;
                } else {
                    const auto direction = navigation.flow_direction(
                        pos->x,
                        pos->y,
                        target->second.strategic_route.x,
                        target->second.strategic_route.y
                    );
                    vel->x = direction.first * move_speed;
                    vel->y = direction.second * move_speed;
                }
            }
        }

        pos->x += vel->x * dt;
        pos->y += vel->y * dt;
        pos->z += vel->z * dt;
        if (auto* aircraft = component_manager_.get_component<Aircraft>(entity_id)) {
            aircraft->x = pos->x;
            aircraft->y = pos->y;
        }
        spatial_grid_.update(entity_id, pos->x, pos->y);
        if (vessel) { vessel->x = pos->x; vessel->y = pos->y; }
        auto patrol = patrol_orders_.find(entity_id);
        if (patrol != patrol_orders_.end() && move_targets_.find(entity_id) == move_targets_.end()) {
            patrol->second.returning = !patrol->second.returning;
            const auto target_position = patrol->second.returning ? patrol->second.origin : patrol->second.destination;
            move_unit(entity_id, target_position.x, target_position.y);
        }
    }
}

void Simulation::combat_phase(float delta_ms) {
    combat_manager_.update(delta_ms, spatial_grid_, component_manager_);
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
    production_manager_.update_all(delta_ms);
    update_harvesters(delta_ms);
    
    // Spawn units for completed constructions
    auto& completed = production_manager_.get_completed_constructions();
    for (size_t index = 0; index < completed.size(); ++index) {
        const auto& comp = completed[index];
        // Factories report their storage position, but a unit spawned at that
        // exact point would be hidden inside the commander model.  Use a
        // deterministic rally line so completed units are immediately visible
        // and remain separated when several jobs finish on one tick.
        const float rally_x = comp.x + 5.0f + static_cast<float>(index) * 3.0f;
        create_unit_with_type(rally_x, comp.y, comp.unit_type, comp.faction_id);
    }
    production_manager_.clear_completed_constructions();
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
    territorial_control_.update(50.0f);
}

float Simulation::get_unit_x(EntityId entity) const {
    auto* pos = component_manager_.get_component<Position>(entity);
    return pos ? pos->x : 0.0f;
}

float Simulation::get_unit_y(EntityId entity) const {
    auto* pos = component_manager_.get_component<Position>(entity);
    return pos ? pos->y : 0.0f;
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
    if (!entity_manager_.destroy_entity(Entity(entity))) {
        return;
    }
    auto& production = production_manager_;
    production.production_lines().erase(entity);
    for (auto& [id, extractor] : production.extractors())
        if (extractor.storage_id == entity) extractor.active = false;
    combat_manager_.unregister_entity(entity);
    move_targets_.erase(entity);
    patrol_orders_.erase(entity);
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
            case 2: node.type = rts::ResourceNode::Type::RESEARCH; break;
            default: return;
        }
        
        std::cerr << "DEBUG: economy_add_resource_node called: node_id=" << node_id << " before_add_size=" << get_simulation()->production_manager().resource_nodes().size() << "\n";
        get_simulation()->production_manager().add_resource_node(static_cast<EntityId>(node_id), node);
        std::cerr << "DEBUG: economy_add_resource_node after_add_size=" << get_simulation()->production_manager().resource_nodes().size() << "\n";
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

    void economy_add_storage(int storage_id, float x, float y, float metal_capacity, float energy_capacity, float research_capacity) {
        rts::Storage storage{};
        storage.x = x;
        storage.y = y;
        storage.metal_capacity = metal_capacity;
        storage.energy_capacity = energy_capacity;
        storage.research_capacity = research_capacity;
        storage.metal_storage = 0.0f;
        storage.energy_storage = 0.0f;
        storage.research_storage = 0.0f;
        
        get_simulation()->production_manager().add_storage(static_cast<EntityId>(storage_id), storage);
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
    
    int simulation_issue_move_commands(const int32_t* entity_ids, int entity_count, int player_id, float center_x, float center_y, float spacing) {
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
        size_t result = sim->issue_move_commands(entities, static_cast<rts::FactionId>(player_id), center_x, center_y, spacing);
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
}

// ===== Faction Initialization (inside namespace rts) =====

namespace rts {

void Simulation::configure_world_size(float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1.0f || height < 1.0f) {
        return;
    }

    const auto grid_width = static_cast<int>(std::round(width));
    const auto grid_height = static_cast<int>(std::round(height));
    pathfinding_ = Pathfinding{grid_width, grid_height, 1.0f, -width * 0.5f, -height * 0.5f};
    naval_pathfinding_ = Pathfinding{grid_width, grid_height, 1.0f, -width * 0.5f, -height * 0.5f};
    air_pathfinding_ = Pathfinding{grid_width, grid_height, 1.0f, -width * 0.5f, -height * 0.5f};
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
        start.start_research, 10000, 10000, 2000});
    ProductionLine line{}; line.storage_id = id; line.max_jobs = 5;
    production_manager_.add_production_line(id, line);
    production_manager_.add_faction_production_line(faction, id);
    FactionResearch research{}; research.available_projects = get_research_projects();
    production_manager_.set_faction_research(faction, research);
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
    storage.research_storage = start_data.start_research;
    storage.metal_capacity = 10000.0f;
    storage.energy_capacity = 10000.0f;
    storage.research_capacity = 2000.0f;
    
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
    
    // Initialize faction research state
    FactionResearch faction_research{};
    faction_research.available_projects = get_research_projects();
    faction_research.completed_projects = {};
    faction_research.active_queue = {};
    production_manager_.set_faction_research(faction_id, faction_research);
    
    // Spawn starting units
    for (size_t i = 0; i < start_data.start_units.size(); ++i) {
        const float offset_x = static_cast<float>((i * 7) % 20) - 10.0f;
        const float offset_y = static_cast<float>((i * 11) % 20) - 10.0f;
        create_unit_with_type(x + offset_x, y + offset_y, start_data.start_units[i], faction_id);
    }
}

int rts::Simulation::create_unit_with_type(float x, float y, rts::UnitType unit_type, rts::FactionId faction_id) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return -1;
    }

    // Get unit prototype
    const auto& prototypes = get_unit_prototypes();
    auto proto_it = prototypes.find(unit_type);
    if (proto_it == prototypes.end()) {
        std::cerr << "Unknown unit type: " << static_cast<int>(unit_type) << std::endl;
        return -1;
    }

    Entity entity = entity_manager_.create_entity();
    
    const auto& proto = proto_it->second;
    
    float terrain_height = terrain_.height_at(x, y);
    Position pos = {x, y, terrain_height};
    // Newly spawned units must remain at their authored spawn point until an
    // authoritative command or autonomous system assigns a destination.
    Velocity vel{0.0f, 0.0f, 0.0f};
    Health hp = {proto.hp, proto.hp};
    
    Material mat = {100.0f, 1000.0f, 0.0f};
    Energy en = {100.0f, 1000.0f, 0.0f, 0.0f};
    Research rs = {0.0f, 1000.0f, 0.0f};
    
    Faction faction = {faction_id};
    
    // Weapon stats - simplified based on unit type
    Weapon wep = {proto.range / 2.0f, 200.0f, 1.0f, 0.0f, proto.range, 0.5f, 5.0f, 1.0f};
    
    component_manager_.add_component(entity.id, pos);
    component_manager_.add_component(entity.id, vel);
    component_manager_.add_component(entity.id, hp);
    component_manager_.add_component(entity.id, mat);
    component_manager_.add_component(entity.id, en);
    component_manager_.add_component(entity.id, rs);
    component_manager_.add_component(entity.id, faction);
    component_manager_.add_component(entity.id, wep);
    component_manager_.add_component(entity.id, UnitData{proto.speed, proto.view_range});
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

const rts::FactionResearch& rts::Simulation::get_faction_research(FactionId faction_id) const {
    static const FactionResearch empty_research{};
    auto it = faction_research_.find(faction_id);
    if (it == faction_research_.end()) {
        return empty_research;
    }
    return it->second;
}

void rts::Simulation::set_faction_research(FactionId faction_id, const FactionResearch& research) {
    faction_research_[faction_id] = research;
}

void rts::Simulation::process_commands() {
    const auto local_commands = command_manager_.get_local_commands();
    command_manager_.clear_local_commands();
    for (size_t i = 0; i < local_commands.size();) {
        const auto& cmd = local_commands[i++];
        if (!validate_command(cmd, tick_)) continue;
        command_log_.push_back(cmd);
        if (cmd.cmd_type == static_cast<uint8_t>(CommandType::MOVE) && cmd.extra > 0) {
            std::vector<EntityId> formation{cmd.entity_id};
            while (i < local_commands.size()) {
                const auto& next = local_commands[i];
                if (next.cmd_type != cmd.cmd_type || next.player_id != cmd.player_id ||
                    next.target_x != cmd.target_x || next.target_y != cmd.target_y || next.extra != cmd.extra) break;
                ++i;
                if (validate_command(next, tick_) && std::find(formation.begin(), formation.end(), next.entity_id) == formation.end()) {
                    formation.push_back(next.entity_id);
                    command_log_.push_back(next);
                }
            }
            move_units_formation(formation, decode_input_command_position(cmd.target_x),
                                 decode_input_command_position(cmd.target_y), cmd.extra / INPUT_COMMAND_POSITION_SCALE);
        } else {
            process_command_internal(cmd);
        }
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
            const float target_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float target_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
            
            if (cmd.extra != 0) {
                const float spacing = static_cast<float>(cmd.extra) / INPUT_COMMAND_POSITION_SCALE;
                const float route_x = target_x + spacing;
                const float route_y = target_y + spacing;
                move_unit_with_route(static_cast<EntityId>(cmd.entity_id), target_x, target_y, route_x, route_y);
            } else {
                move_unit(static_cast<EntityId>(cmd.entity_id), target_x, target_y);
            }
            break;
        }
        case static_cast<uint8_t>(CommandType::ATTACK): {
            const float target_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float target_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
            attack_unit(static_cast<EntityId>(cmd.entity_id), static_cast<EntityId>(cmd.extra));
            break;
        }
        case static_cast<uint8_t>(CommandType::STOP): {
            stop_unit(cmd.entity_id);
            break;
        }
        case static_cast<uint8_t>(CommandType::BUILD): {
            const float build_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float build_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
            const UnitType unit_type = static_cast<UnitType>(cmd.extra);
            build_structure(static_cast<EntityId>(cmd.entity_id), build_x, build_y, unit_type);
            break;
        }
        case static_cast<uint8_t>(CommandType::RESEARCH): {
            production_manager_.begin_research(static_cast<FactionId>(cmd.player_id), research_id(cmd.extra));
            break;
        }
        case static_cast<uint8_t>(CommandType::INSTALL): {
            const float install_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float install_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
            const InstallationType installation_type = static_cast<InstallationType>(cmd.extra);
            install_fob(static_cast<EntityId>(cmd.entity_id), install_x, install_y, installation_type);
            break;
        }
        case static_cast<uint8_t>(CommandType::HARVEST): {
            const float harvest_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float harvest_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
            harvest_resource(static_cast<EntityId>(cmd.entity_id), harvest_x, harvest_y);
            break;
        }
        case static_cast<uint8_t>(CommandType::RETURN): {
            return_unit(static_cast<EntityId>(cmd.entity_id));
            break;
        }
        case static_cast<uint8_t>(CommandType::DEFEND): {
            const float defend_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float defend_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
            defend_area(static_cast<EntityId>(cmd.entity_id), defend_x, defend_y);
            break;
        }
        case static_cast<uint8_t>(CommandType::PATROL): {
            const float patrol_x = static_cast<float>(cmd.target_x) / INPUT_COMMAND_POSITION_SCALE;
            const float patrol_y = static_cast<float>(cmd.target_y) / INPUT_COMMAND_POSITION_SCALE;
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
    
    bool economy_get_storage_info(int storage_id, float* out_metal_storage, float* out_energy_storage, float* out_research_storage, 
                                  float* out_metal_capacity, float* out_energy_capacity, float* out_research_capacity) {
        auto it = get_simulation()->production_manager().storages().find(static_cast<EntityId>(storage_id));
        if (it == get_simulation()->production_manager().storages().end()) {
            return false;
        }
        *out_metal_storage = it->second.metal_storage;
        *out_energy_storage = it->second.energy_storage;
        *out_research_storage = it->second.research_storage;
        *out_metal_capacity = it->second.metal_capacity;
        *out_energy_capacity = it->second.energy_capacity;
        *out_research_capacity = it->second.research_capacity;
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
}
