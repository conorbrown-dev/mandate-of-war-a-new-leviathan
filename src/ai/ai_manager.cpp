#include "ai/ai_manager.hpp"

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
        
        // Only attack if we have units
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
    
    // Get all entities with faction component
    const auto& factions = simulation_->component_manager().get_components<Faction>();
    
    for (const auto& faction_pair : factions) {
        EntityId entity_id = faction_pair.first;
        const Faction& faction = faction_pair.second;
        
        // Check if this unit is visible to our faction
        // For now, assume all units within recon radius are visible
        // In real implementation, this would check visibility state
        
        if (faction.faction_id == faction_id_) {
            visible_units_.push_back(entity_id);
        } else if (faction.faction_id != faction_id_) {
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
    // Count how many extractors we currently have
    int extractor_count = 0;
    const auto& extractors = simulation_->production_manager().extractors();
    
    for (const auto& pair : extractors) {
        // Extractor faction is not directly stored, but we could check
        // its position relative to our spawn or check if it's building correctly
        extractor_count++;
    }
    
    return extractor_count;
}

void AIManager::build_extractor_if_needed() {
    // Check resources
    const auto& storages = simulation_->production_manager().storages();
    
    for (const auto& pair : storages) {
        const Storage& storage = pair.second;
        
        if (storage.metal_storage >= 500.0f && storage.energy_storage >= 200.0f) {
            // Find extractor position near spawn
            // For now, use fixed offset from spawn (0,0)
            float spawn_x = 0.0f;
            float spawn_y = 0.0f;
            
            // Create extractor ID
            int extractor_id = static_cast<int>(faction_id_) * 1000 + count_visible_extractor_slots() + 1;
            
            // Find a resource node to connect to
            // In real implementation, would scan for nearby resource nodes
            
            // Call economy API
            extern "C" {
                void economy_add_extractor(int extractor_id, float x, float y, int node_id, float extraction_rate);
            }
            
            // For now, use node_id = 0 (should find actual node)
            economy_add_extractor(extractor_id, spawn_x + 10.0f, spawn_y, 0, 10.0f);
            
            break;
        }
    }
}

void AIManager::gather_resources() {
    // Build extractors if resources are available
    build_extractor_if_needed();
}

void AIManager::produce_units() {
    // Check if we have units and production capacity
    if (visible_units_.empty()) {
        // Spawn some initial units if we don't have any
        const auto& production_lines = simulation_->production_manager().production_lines();
        
        for (const auto& pair : production_lines) {
            const ProductionLine& line = pair.second;
            
            if (line.active_jobs < line.max_jobs) {
                // Produce a scout unit
                extern "C" {
                    void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick);
                }
                
                // Unit type 1 = scout (minimal cost)
                economy_enqueue_construction(pair.first, pair.first, 1, 50.0f, 20.0f, 5.0f, 2.0f);
                break;
            }
        }
        return;
    }
    
    // Produce defense units if we have resources
    const auto& storages = simulation_->production_manager().storages();
    
    for (const auto& pair : storages) {
        Storage& storage = pair.second;
        
        if (storage.metal_storage >= 200.0f && storage.energy_storage >= 100.0f && storage.metal_storage <= 800.0f) {
            // Produce a defender
            extern "C" {
                void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick);
            }
            
            economy_enqueue_construction(pair.first, pair.first, 2, 200.0f, 100.0f, 20.0f, 10.0f);
            break;
        }
    }
}

void AIManager::attack_enemy() {
    if (enemy_units_.empty()) {
        return;
    }
    
    // Find closest enemy to our spawn
    float best_distance = 1000.0f;
    EntityId target_unit = 0;
    
    for (EntityId enemy_id : enemy_units_) {
        const auto& positions = simulation_->component_manager().get_components<Position>();
        auto it = positions.find(enemy_id);
        
        if (it != positions.end()) {
            const Position& pos = it->second;
            float dist = std::sqrt(pos.x * pos.x + pos.y * pos.y);
            
            if (dist < best_distance) {
                best_distance = dist;
                target_unit = enemy_id;
            }
        }
    }
    
    if (target_unit != 0) {
        // Issue attack command to all visible units
        const auto& positions = simulation_->component_manager().get_components<Position>();
        
        for (EntityId unit_id : visible_units_) {
            auto pos_it = positions.find(unit_id);
            if (pos_it != positions.end()) {
                const Position& pos = pos_it->second;
                
                extern "C" {
                    void command_issue_unit_command(int unit_id, int command_type, float x, float y);
                }
                
                // Command type 2 = attack
                command_issue_unit_command(unit_id, 2, pos.x, pos.y);
            }
        }
    }
}

void AIManager::defend_base() {
    // If enemy is close, issue defensive command
    if (enemy_units_.empty()) {
        return;
    }
    
    float threat_distance = 100.0f;
    bool in_threat = false;
    
    for (EntityId enemy_id : enemy_units_) {
        const auto& positions = simulation_->component_manager().get_components<Position>();
        auto it = positions.find(enemy_id);
        
        if (it != positions.end()) {
            const Position& pos = it->second;
            float dist = std::sqrt(pos.x * pos.x + pos.y * pos.y);
            
            if (dist < threat_distance) {
                in_threat = true;
                break;
            }
        }
    }
    
    if (in_threat) {
        // Issue defensive stance to nearby units
        const auto& positions = simulation_->component_manager().get_components<Position>();
        
        for (EntityId unit_id : visible_units_) {
            auto pos_it = positions.find(unit_id);
            if (pos_it != positions.end()) {
                const Position& pos = pos_it->second;
                
                extern "C" {
                    void command_issue_unit_command(int unit_id, int command_type, float x, float y);
                }
                
                // Command type 1 = defend
                command_issue_unit_command(unit_id, 1, pos.x, pos.y);
            }
        }
    }
}

} // namespace rts

// External C API
static rts::AIManager* g_ai_manager = nullptr;

extern "C" {

void ai_init() {
    if (!g_ai_manager) {
        g_ai_manager = new rts::AIManager();
    }
}

void ai_update(float delta_ms) {
    if (g_ai_manager) {
        g_ai_manager->update(delta_ms);
    }
}

void ai_reset() {
    if (g_ai_manager) {
        g_ai_manager->reset();
    }
}

void ai_set_faction_id(int faction_id) {
    if (g_ai_manager) {
        g_ai_manager->set_faction_id(static_cast<rts::FactionId>(faction_id));
    }
}

int ai_get_visible_unit_count() {
    if (g_ai_manager) {
        return static_cast<int>(g_ai_manager->get_visible_units().size());
    }
    return 0;
}

int ai_get_enemy_unit_count() {
    if (g_ai_manager) {
        return static_cast<int>(g_ai_manager->get_enemy_units().size());
    }
    return 0;
}

} // extern "C"
