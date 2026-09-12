#pragma once

#include <vector>

#include "ecs/component_manager.hpp"
#include "ecs/components/factions.hpp"
#include "logistics/logistics.hpp"

namespace rts {

class SpatialGrid;

struct Projectile {
    EntityId id;
    float x;
    float y;
    float vel_x;
    float vel_y;
    float speed;
    float damage;
    float aoe_radius;
    float time_alive;
    float max_lifetime;
    bool active;
    FactionId faction_id;
};

class ProjectileManager {
public:
    explicit ProjectileManager(size_t max_projectiles = 1000);

    void spawn(float x, float y, float vel_x, float vel_y, float speed,
               float damage, float aoe_radius, float max_lifetime, FactionId faction_id);
    
    void update(float delta_ms);
    void update(float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager);
    
    void clear();
    
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    
    size_t active_count() const;
    size_t total_spawned() const { return total_spawned_; }

private:
    std::vector<Projectile> projectiles_;
    size_t max_projectiles_;
    size_t total_spawned_{0};
    
    void check_impact(Projectile& p, SpatialGrid& spatial_grid, ComponentManager& component_manager);
    void apply_damage(EntityId entity_id, float damage, ComponentManager& component_manager);
};

} // namespace rts
