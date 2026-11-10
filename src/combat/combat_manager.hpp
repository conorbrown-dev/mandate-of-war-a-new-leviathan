#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <unordered_map>

#include "ecs/component_manager.hpp"
#include "combat/projectile_manager.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/weapon.hpp"

namespace rts {

class SpatialGrid;
class ComponentManager;

struct InterceptSolution {
    float aim_x;
    float aim_y;
    float time_to_impact;
    bool valid;
};

class InterceptCalculator {
public:
    InterceptSolution calculate_intercept(
        float shooter_x, float shooter_y, float projectile_speed,
        float target_x, float target_y, float target_vel_x, float target_vel_y
    );
};

struct FireSolution {
    EntityId target_id;
    float aim_x;
    float aim_y;
    float projectile_speed;
    float damage;
    float aoe_radius;
    float lead_bias;
    bool valid;
};

class CombatManager {
public:
    CombatManager();

    using SightCheck = std::function<bool(FactionId, EntityId)>;
    // Set during a match so nothing fires at a target its side cannot see.
    void set_sight_check(SightCheck check) { sight_check_ = std::move(check); }
    void update(float delta_ms);
    void update(float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager);
    
    void register_entity(EntityId entity);
    void unregister_entity(EntityId entity);
    void start_attack(EntityId shooter_id, EntityId target_id, ComponentManager& component_manager);
    
    ProjectileManager& projectile_manager() { return projectile_manager_; }
    const ProjectileManager& projectile_manager() const { return projectile_manager_; }
    
    std::vector<EntityId> damaged_entities() const { return damaged_entities_; }
    
    void reset();
    void set_event_sink(CombatEventSink sink) {
        event_sink_ = std::move(sink);
        projectile_manager_.set_event_sink(event_sink_);
    }
    
private:
    SightCheck sight_check_;
    CombatEventSink event_sink_;
    // Live targets for one fire pass, laid out by cell so a volley does not pay
    // a spatial-grid query plus component lookups for every nearby unit.
    struct TargetCandidate {
        EntityId id;
        float x, y;
        FactionId faction;
        bool in_air = false;
    };
    void build_target_index(const std::vector<EntityId>& entities, ComponentManager& component_manager);
    std::vector<TargetCandidate> target_candidates_;
    std::vector<TargetCandidate> target_sort_buffer_;
    std::vector<uint32_t> target_cell_starts_;
    std::vector<uint32_t> target_cell_cursor_;
    float target_min_x_ = 0.0f, target_min_y_ = 0.0f, target_cell_size_ = 1.0f;
    int target_columns_ = 0, target_rows_ = 0;

    ProjectileManager projectile_manager_;
    InterceptCalculator intercept_calculator_;
    std::vector<EntityId> damaged_entities_;
    
    std::unordered_map<EntityId, FireSolution> pending_fire_;
    std::unordered_set<EntityId> explicit_attack_targets_;
    
public:
    const std::unordered_set<EntityId>& explicit_attack_targets() const { return explicit_attack_targets_; }
    std::unordered_map<EntityId, FireSolution>& pending_fire() { return pending_fire_; }
    const std::unordered_map<EntityId, FireSolution>& pending_fire() const { return pending_fire_; }
    
    FireSolution find_target(
        float shooter_x, float shooter_y, float view_range, FactionId faction,
        ComponentManager& component_manager,
        EntityId explicit_target = 0,
        bool hits_ground = true,
        bool hits_air = false
    );
    
    FireSolution calculate_fire_solution(
        float shooter_x, float shooter_y, float target_x, float target_y,
        float target_vel_x, float target_vel_y, const Weapon& weapon
    );
    
    void resolve_fire(
        float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager
    );
};

} // namespace rts
