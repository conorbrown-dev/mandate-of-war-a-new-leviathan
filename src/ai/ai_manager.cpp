#include "ai/ai_manager.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/faction.hpp"

namespace rts {

AIManager::AIManager() = default;

void AIManager::set_simulation(Simulation* sim) {
    simulation_ = sim;
}

void AIManager::set_faction_id(FactionId faction_id) {
    faction_id_ = faction_id;
}

void AIManager::update(float delta_ms) {
    if (!simulation_) {
        return;
    }
    
    time_since_last_decision_ += delta_ms;
    
    if (time_since_last_decision_ >= decision_interval_) {
        time_since_last_decision_ = 0.0f;
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
}

void AIManager::update_visibility() {
    visible_units_.clear();
    enemy_units_.clear();
    
    const auto& all_entities = simulation_->get_entity_list();
    
    for (EntityId entity_id : all_entities) {
        const Faction* faction = simulation_->component_manager().get_component<Faction>(entity_id);
        
        if (faction && faction->faction_id == faction_id_) {
            visible_units_.push_back(entity_id);
        } else if (faction && faction->faction_id != faction_id_) {
            enemy_units_.push_back(entity_id);
        }
    }
}

std::vector<EntityId> AIManager::get_visible_units() const {
    return visible_units_;
}

std::vector<EntityId> AIManager::get_enemy_units() const {
    return enemy_units_;
}

int AIManager::count_visible_extractor_slots() const {
    const auto& extractors = simulation_->production_manager().extractors();
    return static_cast<int>(extractors.size());
}

void AIManager::build_extractor_if_needed() {
    const auto& storages = simulation_->production_manager().storages();
    
    for (const auto& pair : storages) {
        const Storage& storage = pair.second;
        
        if (storage.metal_storage >= 500.0f && storage.energy_storage >= 200.0f) {
            float spawn_x = 0.0f;
            float spawn_y = 0.0f;
            
            int extractor_id = static_cast<int>(faction_id_) * 1000 + count_visible_extractor_slots() + 1;
            
            break;
        }
    }
}

void AIManager::gather_resources() {
    build_extractor_if_needed();
}

void AIManager::produce_units() {
    const auto& production_lines = simulation_->production_manager().production_lines();
    
    if (visible_units_.empty()) {
        for (const auto& pair : production_lines) {
            const ProductionLine& line = pair.second;
            
            if (line.active_jobs < line.max_jobs) {
                break;
            }
        }
        return;
    }
    
    const auto& storages = simulation_->production_manager().storages();
    
    for (const auto& pair : storages) {
        const Storage& storage = pair.second;
        
        if (storage.metal_storage >= 200.0f && storage.energy_storage >= 100.0f && storage.metal_storage <= 800.0f) {
            break;
        }
    }
}

void AIManager::attack_enemy() {
    if (enemy_units_.empty()) {
        return;
    }
    
    float best_distance = 1000.0f;
    EntityId target_unit = 0;
    
    for (EntityId enemy_id : enemy_units_) {
        const Position* pos = simulation_->component_manager().get_component<Position>(enemy_id);
        
        if (pos) {
            float dist = std::sqrt(pos->x * pos->x + pos->y * pos->y);
            
            if (dist < best_distance) {
                best_distance = dist;
                target_unit = enemy_id;
            }
        }
    }
    
    if (target_unit != 0) {
        for (EntityId unit_id : visible_units_) {
            const Position* pos = simulation_->component_manager().get_component<Position>(unit_id);
            
            if (pos) {
                // would issue attack command here
            }
        }
    }
}

void AIManager::defend_base() {
    if (enemy_units_.empty()) {
        return;
    }
    
    float threat_distance = 100.0f;
    bool in_threat = false;
    
    for (EntityId enemy_id : enemy_units_) {
        const Position* pos = simulation_->component_manager().get_component<Position>(enemy_id);
        
        if (pos) {
            float dist = std::sqrt(pos->x * pos->x + pos->y * pos->y);
            
            if (dist < threat_distance) {
                in_threat = true;
                break;
            }
        }
    }
    
    if (in_threat) {
        for (EntityId unit_id : visible_units_) {
            const Position* pos = simulation_->component_manager().get_component<Position>(unit_id);
            
            if (pos) {
                // would issue stop/hold position command here
            }
        }
    }
}

} // namespace rts

extern "C" {

void ai_init() {
    static rts::AIManager manager;
}

void ai_update(float delta_ms) {
    static rts::AIManager* manager = []() -> rts::AIManager* {
        static rts::AIManager m;
        return &m;
    }();
    if (manager) {
        manager->update(delta_ms);
    }
}

void ai_reset() {
    static rts::AIManager* manager = []() -> rts::AIManager* {
        static rts::AIManager m;
        return &m;
    }();
    if (manager) {
        manager->reset();
    }
}

void ai_set_faction_id(int faction_id) {
    static rts::AIManager* manager = []() -> rts::AIManager* {
        static rts::AIManager m;
        return &m;
    }();
    if (manager) {
        manager->set_faction_id(rts::FactionId(faction_id));
    }
}

int ai_get_visible_unit_count() {
    static rts::AIManager* manager = []() -> rts::AIManager* {
        static rts::AIManager m;
        return &m;
    }();
    if (manager) {
        return static_cast<int>(manager->get_visible_units().size());
    }
    return 0;
}

int ai_get_enemy_unit_count() {
    static rts::AIManager* manager = []() -> rts::AIManager* {
        static rts::AIManager m;
        return &m;
    }();
    if (manager) {
        return static_cast<int>(manager->get_enemy_units().size());
    }
    return 0;
}

}
