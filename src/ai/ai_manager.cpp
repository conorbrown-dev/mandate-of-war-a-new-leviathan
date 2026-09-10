#include "ai/ai_manager.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/faction.hpp"
#include <algorithm>
#include <cmath>

namespace rts {

AIManager::AIManager() = default;

void AIManager::set_simulation(Simulation* sim) {
    reset();
    simulation_ = sim;
}

void AIManager::set_faction_id(FactionId faction_id) {
    if (faction_id != FactionId::ELITE_PRECISION &&
        faction_id != FactionId::MASS_WARFARE &&
        faction_id != FactionId::INDUSTRIAL_EXPERIMENTAL) return;
    reset();
    faction_id_ = faction_id;
}

void AIManager::update(float delta_ms) {
    if (!simulation_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }
    
    time_since_last_decision_ += delta_ms;
    
    if (time_since_last_decision_ >= decision_interval_) {
        time_since_last_decision_ = std::fmod(time_since_last_decision_, decision_interval_);
        update_visibility();
        gather_resources();
        produce_units();
        defend_base();
        
        if (!visible_units_.empty()) {
            attack_enemy();
        }
    }
}

void AIManager::reset() {
    visible_units_.clear();
    enemy_units_.clear();
    time_since_last_decision_ = 0.0f;
    objective_set_ = false;
}

void AIManager::update_visibility() {
    visible_units_.clear();
    enemy_units_.clear();
    
    auto& components = simulation_->component_manager();
    const auto alive = [&components](EntityId id) {
        const auto* health = components.get_component<Health>(id);
        return health && !health->is_dead && health->current > 0.0f;
    };
    // The legacy visible_units API denotes owned, controllable units.
    for (EntityId id : simulation_->get_entity_list()) {
        const auto* faction = components.get_component<Faction>(id);
        if (faction && faction->faction_id == faction_id_ && alive(id)) {
            visible_units_.push_back(id);
        }
    }
    std::sort(visible_units_.begin(), visible_units_.end());

    // Per-faction current vision. Global remembered Intelligence records have
    // no observer faction, so they cannot authorize access to enemy state.
    for (EntityId observer : visible_units_) {
        const auto* position = components.get_component<Position>(observer);
        const auto* unit = components.get_component<UnitData>(observer);
        if (!position || !unit || !std::isfinite(unit->view_range) ||
            unit->view_range <= 0.0f) continue;
        for (EntityId candidate : simulation_->spatial_grid().query_in_region(
                 position->x, position->y, unit->view_range)) {
            const auto* faction = components.get_component<Faction>(candidate);
            if (faction && faction->faction_id != faction_id_ && alive(candidate)) {
                enemy_units_.push_back(candidate);
            }
        }
    }
    std::sort(enemy_units_.begin(), enemy_units_.end());
    enemy_units_.erase(std::unique(enemy_units_.begin(), enemy_units_.end()), enemy_units_.end());
}

std::vector<EntityId> AIManager::get_visible_units() const {
    return visible_units_;
}

std::vector<EntityId> AIManager::get_enemy_units() const {
    return enemy_units_;
}

void AIManager::set_objective(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    objective_x_ = x; objective_y_ = y; objective_set_ = true;
}

void AIManager::gather_resources() {
    // Owned extractors run in the shared fixed-tick production phase.
    if (!simulation_) return;
    auto& production = simulation_->production_manager();
    const auto base = production.faction_line(faction_id_);
    if (base == INVALID_ENTITY) return;
    for (uint32_t index = 0; index < get_research_projects().size(); ++index) {
        if (production.can_research(faction_id_, Simulation::research_id(index))) {
            simulation_->issue_commands({base}, faction_id_, CommandType::RESEARCH, 0, 0, index);
            break;
        }
    }
}

void AIManager::produce_units() {
    if (!simulation_) return;
    auto& production = simulation_->production_manager();
    const auto base = production.faction_line(faction_id_);
    if (base == INVALID_ENTITY) return;
    std::vector<UnitType> candidates;
    for (const auto& [type, prototype] : get_unit_prototypes())
        if (production.can_queue_unit(base, faction_id_, type)) candidates.push_back(type);
    std::sort(candidates.begin(), candidates.end(), [](UnitType a, UnitType b) {
        const auto& prototypes = get_unit_prototypes();
        const float ca = prototypes.at(a).material_cost, cb = prototypes.at(b).material_cost;
        return ca != cb ? ca < cb : a < b;
    });
    if (!candidates.empty()) simulation_->issue_build_commands({base}, faction_id_, 0, 0, static_cast<int>(candidates.front()));
}

void AIManager::attack_enemy() {
    if (!simulation_) return;
    const auto base = simulation_->production_manager().faction_line(faction_id_);
    // Scenario objectives are public map positions, never hidden enemy queries.
    if (base == INVALID_ENTITY || simulation_->get_unit_is_dead(base)) return;
    update_visibility(); // Refresh once per decision, not once per unit/target pair.
    for (auto unit : visible_units_) {
        if (unit == base) continue;
        const auto* position = simulation_->component_manager().get_component<Position>(unit);
        const auto* weapon = simulation_->component_manager().get_component<Weapon>(unit);
        const auto* sensor = simulation_->component_manager().get_component<UnitData>(unit);
        if (!position || !weapon || !sensor || sensor->speed <= 0) continue;
        EntityId target = INVALID_ENTITY;
        float best = std::numeric_limits<float>::infinity();
        bool defending = false;
        const float base_x = simulation_->get_unit_x(base), base_y = simulation_->get_unit_y(base);
        for (auto enemy : enemy_units_) {
            const float dx = simulation_->get_unit_x(enemy) - position->x;
            const float dy = simulation_->get_unit_y(enemy) - position->y;
            const float distance = dx*dx + dy*dy;
            const float bx=simulation_->get_unit_x(enemy)-base_x, by=simulation_->get_unit_y(enemy)-base_y;
            const bool threat=bx*bx+by*by <= 40*40;
            if ((threat && !defending) || (threat == defending && distance < best)) {
                best = distance; target = enemy; defending = threat;
            }
        }
        if (target != INVALID_ENTITY) {
            const float range = std::min(weapon->range, sensor->view_range) * 0.8f;
            if (best > range*range) simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE,
                simulation_->get_unit_x(target), simulation_->get_unit_y(target));
            else {
                simulation_->issue_stop_commands({unit}, faction_id_);
                simulation_->issue_attack_commands({unit}, faction_id_, target);
            }
        } else if (objective_set_) {
            // Search a public, bounded objective perimeter when no enemy is
            // currently visible. This does not read hidden unit coordinates.
            const auto corner = (simulation_->simulation_tick()/200 + unit%4)%4;
            const float x = objective_x_ + (corner<2 ? -15.0f : 15.0f);
            const float y = objective_y_ + (corner%2 ? -15.0f : 15.0f);
            simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE, x, y);
        }
    }
}

void AIManager::defend_base() {
    // Visible nearby threats take precedence over the public advance objective
    // in attack_enemy; no separate stop order overwrites those responses.
}

} // namespace rts
