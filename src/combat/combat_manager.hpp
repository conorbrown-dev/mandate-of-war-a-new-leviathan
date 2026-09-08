#pragma once

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

    void update(float delta_ms);
    void update(float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager);
    
    void register_entity(EntityId entity);
    void unregister_entity(EntityId entity);
    void start_attack(EntityId shooter_id, EntityId target_id, ComponentManager& component_manager);
    
    ProjectileManager& projectile_manager() { return projectile_manager_; }
    const ProjectileManager& projectile_manager() const { return projectile_manager_; }
    
    std::vector<EntityId> damaged_entities() const { return damaged_entities_; }
    
    void reset();
    
private:
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
        SpatialGrid& spatial_grid, ComponentManager& component_manager,
        EntityId explicit_target = 0
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
