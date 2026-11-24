#include "combat/combat_manager.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

#include "spatial/spatial_grid.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/faction.hpp"
#include "ecs/components/weapon.hpp"

namespace rts {

InterceptSolution InterceptCalculator::calculate_intercept(
    float shooter_x, float shooter_y, float projectile_speed,
    float target_x, float target_y, float target_vel_x, float target_vel_y
) {
    InterceptSolution solution{};
    solution.valid = false;
    
    float dx = target_x - shooter_x;
    float dy = target_y - shooter_y;
    
    float target_speed_sq = target_vel_x * target_vel_x + target_vel_y * target_vel_y;
    float projectile_speed_sq = projectile_speed * projectile_speed;
    
    float a = target_speed_sq - projectile_speed_sq;
    float b = 2.0f * (target_vel_x * dx + target_vel_y * dy);
    float c = dx * dx + dy * dy;
    
    if (std::abs(a) < 1e-6f) {
        if (std::abs(b) < 1e-6f) {
            if (std::abs(c) < 1e-6f) {
                solution.time_to_impact = 0.0f;
                solution.aim_x = shooter_x;
                solution.aim_y = shooter_y;
                solution.valid = true;
            }
            return solution;
        }
        float discriminant_linear = b * b - 4.0f * a * c;
        if (discriminant_linear >= 0.0f) {
            float t = -c / b;
            if (t > 0.0f) {
                solution.time_to_impact = t;
                solution.aim_x = target_x + target_vel_x * t;
                solution.aim_y = target_y + target_vel_y * t;
                solution.valid = true;
            }
        }
        return solution;
    }
    
    float discriminant = b * b - 4.0f * a * c;
    
    if (discriminant < 0.0f) {
        return solution;
    }
    
    float sqrt_disc = std::sqrt(discriminant);
    float t1 = (-b + sqrt_disc) / (2.0f * a);
    float t2 = (-b - sqrt_disc) / (2.0f * a);
    
    float t = std::min(t1, t2);
    if (t < 0.0f) {
        t = std::max(t1, t2);
    }
    
    if (t < 0.0f) {
        return solution;
    }
    
    solution.time_to_impact = t;
    solution.aim_x = target_x + target_vel_x * t;
    solution.aim_y = target_y + target_vel_y * t;
    solution.valid = true;
    
    return solution;
}

CombatManager::CombatManager() {}

void CombatManager::update(float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager) {
    resolve_fire(delta_ms, spatial_grid, component_manager);
    projectile_manager_.update(delta_ms, spatial_grid, component_manager);
}

void CombatManager::register_entity(EntityId entity) {
}

void CombatManager::unregister_entity(EntityId entity) {
    pending_fire_.erase(entity);
    explicit_attack_targets_.erase(entity);
}

void CombatManager::start_attack(EntityId shooter_id, EntityId target_id, ComponentManager& component_manager) {
    auto* shooter_faction = component_manager.get_component<Faction>(shooter_id);
    auto* target_faction = component_manager.get_component<Faction>(target_id);
    
    if (!shooter_faction || !target_faction || shooter_faction->faction_id == target_faction->faction_id) {
        return;
    }
    
    pending_fire_[shooter_id].target_id = target_id;
    pending_fire_[shooter_id].valid = true;
    explicit_attack_targets_.insert(shooter_id);
}

void CombatManager::reset() {
    sight_check_ = nullptr;
    projectile_manager_.clear();
    damaged_entities_.clear();
    pending_fire_.clear();
    explicit_attack_targets_.clear();
}

FireSolution CombatManager::find_target(
    float shooter_x, float shooter_y, float view_range, FactionId faction,
    ComponentManager& component_manager,
    EntityId explicit_target
) {
    FireSolution solution{};
    solution.valid = false;
    
    if (explicit_target > 0) {
        auto* target_pos = component_manager.get_component<Position>(explicit_target);
        auto* target_faction = component_manager.get_component<Faction>(explicit_target);
        auto* target_health = component_manager.get_component<Health>(explicit_target);
        
        if (target_pos && target_faction && target_health && !target_health->is_dead && 
            target_faction->faction_id != faction) {
            float dx = target_pos->x - shooter_x;
            float dy = target_pos->y - shooter_y;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist <= view_range && (!sight_check_ || sight_check_(faction, explicit_target))) {
                auto* target_vel = component_manager.get_component<Velocity>(explicit_target);
                float target_vel_x = target_vel ? target_vel->x : 0.0f;
                float target_vel_y = target_vel ? target_vel->y : 0.0f;
                
                solution.target_id = explicit_target;
                solution.aim_x = target_pos->x;
                solution.aim_y = target_pos->y;
                solution.projectile_speed = 0.0f;
                solution.damage = 0.0f;
                solution.aoe_radius = 0.0f;
                solution.lead_bias = dist;
                solution.valid = true;
                return solution;
            }
        }
    }
    
    if (target_candidates_.empty()) return solution;
    const auto cell_index = [this](float offset, int count) {
        return std::clamp(static_cast<int>(std::floor(offset / target_cell_size_)), 0, count - 1);
    };
    const int first_column = cell_index(shooter_x - view_range - target_min_x_, target_columns_);
    const int last_column = cell_index(shooter_x + view_range - target_min_x_, target_columns_);
    const int first_row = cell_index(shooter_y - view_range - target_min_y_, target_rows_);
    const int last_row = cell_index(shooter_y + view_range - target_min_y_, target_rows_);

    for (int row = first_row; row <= last_row; ++row) {
        const size_t row_offset = static_cast<size_t>(row) * static_cast<size_t>(target_columns_);
        const uint32_t begin = target_cell_starts_[row_offset + first_column];
        const uint32_t end = target_cell_starts_[row_offset + last_column + 1];
        for (uint32_t index = begin; index < end; ++index) {
            const auto& candidate = target_candidates_[index];
            if (candidate.faction == faction) continue;
            const float dx = candidate.x - shooter_x;
            const float dy = candidate.y - shooter_y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= view_range && (!solution.valid || dist < solution.lead_bias ||
                                       (dist == solution.lead_bias && candidate.id < solution.target_id)) &&
                (!sight_check_ || sight_check_(faction, candidate.id))) {
                solution.target_id = candidate.id;
                solution.aim_x = candidate.x;
                solution.aim_y = candidate.y;
                solution.projectile_speed = 0.0f;
                solution.damage = 0.0f;
                solution.aoe_radius = 0.0f;
                solution.lead_bias = dist;
                solution.valid = true;
            }
        }
    }

    return solution;
}

void CombatManager::build_target_index(const std::vector<EntityId>& entities, ComponentManager& component_manager) {
    target_candidates_.clear();
    float min_x = std::numeric_limits<float>::max(), min_y = min_x;
    float max_x = std::numeric_limits<float>::lowest(), max_y = max_x;
    for (const auto entity_id : entities) {
        if (entity_id == 0) continue;
        const auto* position = component_manager.get_component<Position>(entity_id);
        const auto* faction = component_manager.get_component<Faction>(entity_id);
        const auto* health = component_manager.get_component<Health>(entity_id);
        if (!position || !faction || !health || health->is_dead ||
            !std::isfinite(position->x) || !std::isfinite(position->y)) continue;
        target_candidates_.push_back({entity_id, position->x, position->y, faction->faction_id});
        min_x = std::min(min_x, position->x);
        max_x = std::max(max_x, position->x);
        min_y = std::min(min_y, position->y);
        max_y = std::max(max_y, position->y);
    }
    if (target_candidates_.empty()) return;

    // Cell size grows with the battlefield so the table stays bounded.
    constexpr float MIN_CELL_SIZE = 32.0f;
    constexpr float MAX_CELLS_PER_AXIS = 256.0f;
    target_cell_size_ = std::max({MIN_CELL_SIZE, (max_x - min_x) / MAX_CELLS_PER_AXIS, (max_y - min_y) / MAX_CELLS_PER_AXIS});
    target_min_x_ = min_x;
    target_min_y_ = min_y;
    target_columns_ = static_cast<int>((max_x - min_x) / target_cell_size_) + 1;
    target_rows_ = static_cast<int>((max_y - min_y) / target_cell_size_) + 1;

    // Counting sort into row-major cells, so a span of columns in one row is
    // a single contiguous range.
    const auto cell_of = [this](const TargetCandidate& candidate) {
        const auto column = static_cast<size_t>((candidate.x - target_min_x_) / target_cell_size_);
        const auto row = static_cast<size_t>((candidate.y - target_min_y_) / target_cell_size_);
        return row * static_cast<size_t>(target_columns_) + column;
    };
    target_cell_starts_.assign(static_cast<size_t>(target_columns_) * target_rows_ + 1, 0);
    for (const auto& candidate : target_candidates_) ++target_cell_starts_[cell_of(candidate) + 1];
    for (size_t cell = 1; cell < target_cell_starts_.size(); ++cell) target_cell_starts_[cell] += target_cell_starts_[cell - 1];
    target_cell_cursor_.assign(target_cell_starts_.begin(), target_cell_starts_.end() - 1);
    target_sort_buffer_.resize(target_candidates_.size());
    for (const auto& candidate : target_candidates_) target_sort_buffer_[target_cell_cursor_[cell_of(candidate)]++] = candidate;
    target_candidates_.swap(target_sort_buffer_);
}

FireSolution CombatManager::calculate_fire_solution(
    float shooter_x, float shooter_y, float target_x, float target_y,
    float target_vel_x, float target_vel_y, const Weapon& weapon
) {
    FireSolution solution{};
    solution.valid = false;
    
    InterceptSolution intercept = intercept_calculator_.calculate_intercept(
        shooter_x, shooter_y, weapon.projectile_speed,
        target_x, target_y, target_vel_x, target_vel_y
    );
    
    if (intercept.valid) {
        solution.aim_x = intercept.aim_x;
        solution.aim_y = intercept.aim_y;
        solution.projectile_speed = weapon.projectile_speed;
        solution.damage = weapon.damage;
        solution.aoe_radius = weapon.aoe_radius;
        solution.lead_bias = weapon.lead_bias;
        solution.valid = true;
    }
    
    return solution;
}

void CombatManager::resolve_fire(
    float delta_ms, SpatialGrid& spatial_grid, ComponentManager& component_manager
) {
    auto entities = spatial_grid.query_all();
    
    bool have_live_faction = false;
    bool have_opposing_factions = false;
    FactionId first_live_faction = FactionId::ELITE_PRECISION;

    // Advance cooldowns in O(n), but avoid an expensive target query for every
    // unit when the world contains no opposing live factions. This keeps the
    // Goal 02 single-faction scale workload isolated from provisional combat.
    for (auto entity_id : entities) {
        if (entity_id == 0) continue;

        auto* pos = component_manager.get_component<Position>(entity_id);
        auto* faction = component_manager.get_component<Faction>(entity_id);
        auto* health = component_manager.get_component<Health>(entity_id);
        
        if (!pos || !faction || !health || health->is_dead) {
            continue;
        }

        if (!have_live_faction) {
            first_live_faction = faction->faction_id;
            have_live_faction = true;
        } else if (faction->faction_id != first_live_faction) {
            have_opposing_factions = true;
        }

        auto* weapon = component_manager.get_component<Weapon>(entity_id);
        if (weapon) {
            weapon->cooldown_remaining = std::max(
                0.0f,
                weapon->cooldown_remaining - delta_ms / 1000.0f
            );
        }
    }

    if (!have_opposing_factions) {
        return;
    }

    build_target_index(entities, component_manager);
    
    for (auto entity_id : entities) {
        if (entity_id == 0) continue;
        
        auto* pos = component_manager.get_component<Position>(entity_id);
        auto* faction = component_manager.get_component<Faction>(entity_id);
        auto* health = component_manager.get_component<Health>(entity_id);
        
        if (!pos || !faction || !health || health->is_dead) {
            continue;
        }
        
        auto* weapon = component_manager.get_component<Weapon>(entity_id);
        if (!weapon) {
            continue;
        }
        
        const auto* aircraft=component_manager.get_component<Aircraft>(entity_id);
        if(aircraft && aircraft->status!=Aircraft::Status::AIRBORNE) continue;
        if (weapon->cooldown_remaining <= 0.0f) {
            FireSolution target = find_target(
                pos->x, pos->y, component_manager.get_component<UnitData>(entity_id)
                    ? std::min(weapon->range, component_manager.get_component<UnitData>(entity_id)->view_range)
                    : weapon->range, faction->faction_id,
                component_manager,
                explicit_attack_targets_.count(entity_id) > 0 ? pending_fire_.at(entity_id).target_id : 0
            );
            
            if (target.valid) {
                FireSolution fire = calculate_fire_solution(
                    pos->x, pos->y,
                    target.aim_x, target.aim_y,
                    0.0f, 0.0f, *weapon
                );
                
                if (fire.valid) {
                    float dx = fire.aim_x - pos->x;
                    float dy = fire.aim_y - pos->y;
                    float mag = std::sqrt(dx * dx + dy * dy);
                    float vel_x = (mag > 0.0f) ? (dx / mag) * fire.projectile_speed : fire.projectile_speed;
                    float vel_y = (mag > 0.0f) ? (dy / mag) * fire.projectile_speed : 0.0f;
                    
                    projectile_manager_.spawn(
                        pos->x, pos->y, vel_x, vel_y, fire.projectile_speed,
                        fire.damage, fire.aoe_radius, 3.0f, faction->faction_id
                    );
                    
                    weapon->cooldown_remaining = weapon->fire_rate;
                }
            }
        }
    }
}

} // namespace rts
