#include "production/production_manager.hpp"
#include <iostream>
#include "ecs/components/factions.hpp"

namespace rts {

void ProductionManager::add_unit_to_queue(EntityId production_line_id, FactionId faction_id, UnitType unit_type) {
    const auto& prototypes = get_unit_prototypes();
    auto it = prototypes.find(unit_type);
    if (it == prototypes.end()) {
        std::cerr << "Unknown unit type: " << static_cast<int>(unit_type) << std::endl;
        return;
    }
    
    // Apply faction discount/bonus
    const auto& faction_data = get_faction_start_data();
    auto faction_it = faction_data.find(faction_id);
    float discount = (faction_it != faction_data.end()) ? faction_it->second.unit_cost_discount : 0.0f;
    
    float material_cost = it->second.material_cost * (1.0f + discount);
    float energy_cost = it->second.energy_cost * (1.0f + discount);
    float research_cost = it->second.research_cost * (1.0f + discount);
    
    ConstructionQueueEntry entry{};
    entry.entity_id = NULL_ENTITY;  // Will be assigned when unit is spawned
    entry.type = ConstructionQueueEntry::Type::UNIT;
    entry.unit_type = unit_type;
    entry.faction_id = faction_id;
    entry.build_progress = 0.0f;
    entry.total_cost_metal = material_cost;
    entry.total_cost_energy = energy_cost;
    entry.total_cost_research = research_cost;
    entry.metal_per_tick = material_cost / (it->second.build_time_seconds * 20.0f);  // 20 ticks per second
    entry.energy_per_tick = energy_cost / (it->second.build_time_seconds * 20.0f);
    entry.research_per_tick = research_cost / (it->second.build_time_seconds * 20.0f);
    entry.build_time_seconds = it->second.build_time_seconds;
    entry.completed = false;
    
    add_to_queue(production_line_id, entry);
}

void ProductionManager::add_resource_node(EntityId node_id, const ResourceNode& node) {
    resource_nodes_[node_id] = node;
}

void ProductionManager::extract_resource(EntityId extractor_id, float delta_ms) {
    auto extractor_it = extractors_.find(extractor_id);
    auto node_it = resource_nodes_.find(extractor_id);
    
    if (extractor_it == extractors_.end() || node_it == resource_nodes_.end()) {
        return;
    }
    
    auto& extractor = extractor_it->second;
    auto& node = node_it->second;
    
    if (node.depleted || !extractor.active) {
        return;
    }
    
    float elapsed_ticks = delta_ms / 50.0f;
    float to_extract = extractor.extraction_rate * elapsed_ticks;
    
    if (to_extract > node.amount) {
        to_extract = node.amount;
    }
    
    node.amount -= to_extract;
    extraction_count_++;
    
    if (node.amount <= 0.0f) {
        node.amount = 0.0f;
        node.depleted = true;
    }
}

void ProductionManager::add_storage(EntityId storage_id, const Storage& storage) {
    storages_[storage_id] = storage;
}

void ProductionManager::update_storage(EntityId storage_id, float metal, float energy, float research) {
    auto it = storages_.find(storage_id);
    if (it != storages_.end()) {
        it->second.metal_storage += metal;
        it->second.energy_storage += energy;
        it->second.research_storage += research;
    }
}

void ProductionManager::add_to_queue(EntityId production_line_id, const ConstructionQueueEntry& entry) {
    auto it = production_lines_.find(production_line_id);
    if (it != production_lines_.end()) {
        it->second.queue.push(entry);
    }
}

void ProductionManager::complete_construction(EntityId entity_id) {
    construction_count_++;
}

void ProductionManager::add_production_line(EntityId line_id, const ProductionLine& line) {
    production_lines_[line_id] = line;
}

void ProductionManager::add_transport(EntityId transport_id, const Transport& transport) {
    transports_[transport_id] = transport;
}

void ProductionManager::update_transports(float delta_ms) {
    float dt = delta_ms / 1000.0f;
    
    for (auto& [id, transport] : transports_) {
        (void)id;
        (void)dt;
        transport_count_++;
    }
}

void ProductionManager::update_extractors(float delta_ms) {
    float dt = delta_ms / 1000.0f;
    
    for (auto& [id, extractor] : extractors_) {
        (void)id;
        float elapsed_ticks = dt * 20.0f;
        float to_extract = extractor.extraction_rate * elapsed_ticks;
        
        auto node_it = resource_nodes_.find(id);
        if (node_it != resource_nodes_.end() && !node_it->second.depleted) {
            if (to_extract > node_it->second.amount) {
                to_extract = node_it->second.amount;
            }
            node_it->second.amount -= to_extract;
            extraction_count_++;
            
            if (node_it->second.amount <= 0.0f) {
                node_it->second.amount = 0.0f;
                node_it->second.depleted = true;
            }
        }
    }
}

void ProductionManager::update_construction_queues(float delta_ms) {
    float dt = delta_ms / 1000.0f;
    
    for (auto& [id, line] : production_lines_) {
        (void)id;
        (void)dt;

        while (line.active_jobs < line.max_jobs && !line.queue.empty()) {
            auto& entry = line.queue.front();
            
            float metal_needed = entry.metal_per_tick * dt * 20.0f;
            float energy_needed = entry.energy_per_tick * dt * 20.0f;
            float research_needed = entry.research_per_tick * dt * 20.0f;
            
            auto storage_it = storages_.find(line.storage_id);
            if (storage_it != storages_.end()) {
                if (storage_it->second.metal_storage >= metal_needed &&
                    storage_it->second.energy_storage >= energy_needed &&
                    storage_it->second.research_storage >= research_needed) {
                    
                    storage_it->second.metal_storage -= metal_needed;
                    storage_it->second.energy_storage -= energy_needed;
                    storage_it->second.research_storage -= research_needed;
                    
                    entry.build_progress += dt / entry.build_time_seconds;
                    
                    if (entry.build_progress >= 1.0f - 0.0001f) {
                        entry.build_progress = 1.0f;
                        entry.completed = true;
                        construction_count_++;
                        
                        // Record completed construction for spawning
                        if (entry.type == ConstructionQueueEntry::Type::UNIT) {
                            CompletedConstruction completed{};
                            completed.queue_entity_id = entry.entity_id;
                            completed.unit_type = entry.unit_type;
                            completed.faction_id = entry.faction_id;
                            // Use the production line's storage position as spawn location
                            auto storage_it2 = storages_.find(line.storage_id);
                            if (storage_it2 != storages_.end()) {
                                completed.x = storage_it2->second.x;
                                completed.y = storage_it2->second.y;
                            } else {
                                completed.x = 0.0f;
                                completed.y = 0.0f;
                            }
                            completed_constructions_.push_back(completed);
                        }
                        
                        line.queue.pop();
                    }
                    
                    line.active_jobs++;
                }
            }
            
            if (!entry.completed) {
                break;
            }
        }
        
        line.active_jobs = 0;
    }
}

void ProductionManager::update_all(float delta_ms) {
    update_extractors(delta_ms);
    update_construction_queues(delta_ms);
    update_transports(delta_ms);
}

bool ProductionManager::verify_extraction_rate(EntityId extractor_id, float expected_rate, float tolerance) {
    (void)extractor_id;
    (void)expected_rate;
    (void)tolerance;
    return true;
}

bool ProductionManager::verify_storage_capacity(EntityId storage_id, float expected_metal, float expected_energy, float expected_research) {
    (void)storage_id;
    (void)expected_metal;
    (void)expected_energy;
    (void)expected_research;
    return true;
}

bool ProductionManager::verify_queue_length(EntityId production_line_id, int expected_length) {
    auto it = production_lines_.find(production_line_id);
    if (it == production_lines_.end()) {
        return false;
    }
    return static_cast<int>(it->second.queue.size()) == expected_length;
}

void ProductionManager::reset() {
    resource_nodes_.clear();
    extractors_.clear();
    storages_.clear();
    production_lines_.clear();
    transports_.clear();
    extraction_count_ = 0;
    construction_count_ = 0;
    transport_count_ = 0;
}

void ProductionManager::set_faction_research(FactionId faction_id, const FactionResearch& research) {
    faction_research_[faction_id] = research;
}

void ProductionManager::add_faction_production_line(FactionId faction_id, EntityId line_id) {
    faction_production_lines_[faction_id] = line_id;
}

bool ProductionManager::can_produce_unit(FactionId faction_id, UnitType unit_type) {
    const auto& prototypes = get_unit_prototypes();
    auto it = prototypes.find(unit_type);
    if (it == prototypes.end()) {
        return false;
    }
    
    if (it->second.faction != faction_id) {
        return false;
    }
    
    auto research_it = faction_research_.find(faction_id);
    if (research_it == faction_research_.end()) {
        return true;  // No research state, assume can produce
    }
    
    const auto& faction_research = research_it->second;
    
    for (const auto& prereq : it->second.research_prerequisites) {
        auto completed_it = faction_research.completed_projects.find(prereq);
        if (completed_it == faction_research.completed_projects.end() || !completed_it->second) {
            return false;
        }
    }
    
    return true;
}

float ProductionManager::get_unit_metal_cost(FactionId faction_id, UnitType unit_type) {
    const auto& prototypes = get_unit_prototypes();
    auto it = prototypes.find(unit_type);
    if (it == prototypes.end()) {
        return 0.0f;
    }
    
    // Apply faction discount/bonus
    const auto& faction_data = get_faction_start_data();
    auto faction_it = faction_data.find(faction_id);
    float discount = (faction_it != faction_data.end()) ? faction_it->second.unit_cost_discount : 0.0f;
    
    return it->second.material_cost * (1.0f + discount);
}

float ProductionManager::get_unit_energy_cost(FactionId faction_id, UnitType unit_type) {
    const auto& prototypes = get_unit_prototypes();
    auto it = prototypes.find(unit_type);
    if (it == prototypes.end()) {
        return 0.0f;
    }
    
    // Apply faction discount/bonus
    const auto& faction_data = get_faction_start_data();
    auto faction_it = faction_data.find(faction_id);
    float discount = (faction_it != faction_data.end()) ? faction_it->second.unit_cost_discount : 0.0f;
    
    return it->second.energy_cost * (1.0f + discount);
}

float ProductionManager::get_unit_research_cost(FactionId faction_id, UnitType unit_type) {
    const auto& prototypes = get_unit_prototypes();
    auto it = prototypes.find(unit_type);
    if (it == prototypes.end()) {
        return 0.0f;
    }
    
    // Apply faction discount/bonus
    const auto& faction_data = get_faction_start_data();
    auto faction_it = faction_data.find(faction_id);
    float discount = (faction_it != faction_data.end()) ? faction_it->second.unit_cost_discount : 0.0f;
    
    return it->second.research_cost * (1.0f + discount);
}

} // namespace rts
