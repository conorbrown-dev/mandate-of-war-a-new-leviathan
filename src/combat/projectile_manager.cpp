#include "combat/projectile_manager.hpp"
#include <cmath>

#include "spatial/spatial_grid.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/faction.hpp"

namespace rts {

ProjectileManager::ProjectileManager(size_t max_projectiles)
    : max_projectiles_(max_projectiles) {}

void ProjectileManager::spawn(float x, float y, float vel_x, float vel_y,
                              float speed, float damage, float aoe_radius,
                              float max_lifetime, FactionId faction_id) {
    if (projectiles_.size() >= max_projectiles_) {
        return;
    }
    
    Projectile p{};
    p.id = static_cast<EntityId>(projectiles_.size());
    p.x = x;
    p.y = y;
    
    p.faction_id = faction_id;
    
    float mag = std::sqrt(vel_x * vel_x + vel_y * vel_y);
    if (mag > 0.0f) {
        p.vel_x = (vel_x / mag) * speed;
        p.vel_y = (vel_y / mag) * speed;
    } else {
        p.vel_x = speed;
        p.vel_y = 0.0f;
    }
    
    p.speed = speed;
    p.damage = damage;
    p.aoe_radius = aoe_radius;
    p.time_alive = 0.0f;
    p.max_lifetime = max_lifetime;
    p.active = true;
    
    projectiles_.push_back(p);
}

void ProjectileManager::update(float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager) {
    float dt = delta_ms / 1000.0f;
    
    for (auto& p : projectiles_) {
        if (!p.active) continue;
        
        p.time_alive += dt;
        
        if (p.time_alive >= p.max_lifetime) {
            p.active = false;
            continue;
        }
        
        p.x += p.vel_x * dt;
        p.y += p.vel_y * dt;
        
        check_impact(p, spatial_grid, component_manager);
    }
}

void ProjectileManager::check_impact(Projectile& p, SpatialGrid& spatial_grid, ComponentManager& component_manager) {
    auto entities = spatial_grid.query_in_region(p.x, p.y, p.aoe_radius);
    
    bool hit_enemy = false;
    for (auto entity_id : entities) {
        auto* pos = component_manager.get_component<Position>(entity_id);
        if (!pos) continue;
        
        float dx = p.x - pos->x;
        float dy = p.y - pos->y;
        float dist2 = dx * dx + dy * dy;
        float dist = std::sqrt(dist2);
        
        if (dist2 <= p.aoe_radius * p.aoe_radius) {
            auto* entity_faction = component_manager.get_component<Faction>(entity_id);
            if (!entity_faction || entity_faction->faction_id != p.faction_id) {
                apply_damage(entity_id, p.damage, component_manager);
                hit_enemy = true;
            }
        }
    }
    
    if (hit_enemy) {
        p.active = false;
    }
}

void ProjectileManager::apply_damage(EntityId entity_id, float damage, ComponentManager& component_manager) {
    auto* health = component_manager.get_component<Health>(entity_id);
    if (health) {
        health->current -= damage;
        if (health->current <= 0.0f) {
            health->current = 0.0f;
            health->is_dead = true;
        }
    }
}

void ProjectileManager::clear() {
    projectiles_.clear();
}

size_t ProjectileManager::active_count() const {
    size_t count = 0;
    for (const auto& p : projectiles_) {
        if (p.active) count++;
    }
    return count;
}

} // namespace rts
