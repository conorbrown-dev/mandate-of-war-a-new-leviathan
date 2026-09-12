#include "combat/projectile_manager.hpp"
#include <cmath>
#include <algorithm>

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
        
        const float old_x = p.x, old_y = p.y;
        const float dx = p.vel_x * dt, dy = p.vel_y * dt;
        const float length_sq = dx*dx + dy*dy;
        const float radius = std::max(0.5f, p.aoe_radius);
        auto candidates = spatial_grid.query_in_region(old_x+dx/2, old_y+dy/2,
            std::sqrt(length_sq)/2+radius);
        float impact = 2;
        for (auto id : candidates) {
            const auto* owner = component_manager.get_component<Faction>(id);
            const auto* health = component_manager.get_component<Health>(id);
            const auto* position = component_manager.get_component<Position>(id);
            if (!position || !health || health->is_dead || (owner && owner->faction_id==p.faction_id)) continue;
            const float ox=old_x-position->x, oy=old_y-position->y;
            const float c=ox*ox+oy*oy-radius*radius;
            if(c<=0) { impact=0; break; }
            if(length_sq<=0) continue;
            const float b=ox*dx+oy*dy, discriminant=b*b-length_sq*c;
            if(discriminant<0) continue;
            const float t=(-b-std::sqrt(discriminant))/length_sq;
            if(t>=0 && t<=1) impact=std::min(impact,t);
        }
        const float travel=std::min(impact,1.0f);
        p.x=old_x+dx*travel; p.y=old_y+dy*travel;
        if(impact<=1) check_impact(p, spatial_grid, component_manager);
    }
    std::erase_if(projectiles_, [](const Projectile& p) { return !p.active; });
}

void ProjectileManager::check_impact(Projectile& p, SpatialGrid& spatial_grid, ComponentManager& component_manager) {
    const float radius = std::max(0.5f, p.aoe_radius) + 0.001f;
    auto entities = spatial_grid.query_in_region(p.x, p.y, radius);
    
    bool hit_enemy = false;
    for (auto entity_id : entities) {
        auto* pos = component_manager.get_component<Position>(entity_id);
        if (!pos) continue;
        
        float dx = p.x - pos->x;
        float dy = p.y - pos->y;
        float dist2 = dx * dx + dy * dy;
        float dist = std::sqrt(dist2);
        
        if (dist2 <= radius * radius) {
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
