#include "gdextension/gd_extension.hpp"
#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>

#include "simulation/simulation.hpp"

using namespace godot;

void initialize_rts_extension_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(rts::RtsExtension);
}

void uninitialize_rts_extension_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT rts_extension_library_init(const GDExtensionInterface *p_interface, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_init) {
    Godot::init(p_interface, p_library, r_init);
    
    r_init->register_initializer(initialize_rts_extension_module);
    r_init->register_terminator(uninitialize_rts_extension_module);
    r_init->set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    
    return GDExtensionBool(false);
}

void GDE_EXPORT rts_extension_library_deinit(GDExtensionClassLibraryPtr p_library) {
    Godot::deinit();
}
}

namespace rts {

void RtsExtension::start_simulation() {
    simulation_.start();
}

void RtsExtension::stop_simulation() {
    simulation_.stop();
}

void RtsExtension::update_simulation(float delta_ms) {
    simulation_.update(delta_ms);
}

int RtsExtension::create_unit(float x, float y) {
    Entity unit = simulation_.create_unit(x, y);
    return static_cast<int>(unit.id);
}

void RtsExtension::move_unit(int entity_id, float x, float y) {
    simulation_.move_unit(static_cast<EntityId>(entity_id), x, y);
}

int RtsExtension::get_entity_count() const {
    return static_cast<int>(simulation_.entity_count());
}

Array RtsExtension::query_units_in_region(float x, float y, float radius) {
    Array units;
    // TODO: Implement spatial query
    return units;
}

void RtsExtension::economy_add_resource_node(int node_id, float x, float y, float amount, int type) {
    rts::ResourceNode node{};
    node.x = x;
    node.y = y;
    node.amount = amount;
    node.max_amount = amount;
    node.depleted = false;
    
    switch (type) {
        case 0: node.type = rts::ResourceNode::Type::METAL; break;
        case 1: node.type = rts::ResourceNode::Type::ENERGY; break;
        case 2: node.type = rts::ResourceNode::Type::RESEARCH; break;
        default: return;
    }
    
    simulation_.production_manager().add_resource_node(static_cast<EntityId>(node_id), node);
}

void RtsExtension::economy_add_extractor(int extractor_id, float x, float y, int node_id, float extraction_rate) {
    rts::Extractor extractor{};
    extractor.x = x;
    extractor.y = y;
    extractor.resource_node_id = static_cast<EntityId>(node_id);
    extractor.extraction_rate = extraction_rate;
    extractor.last_extraction_tick = 0.0f;
    extractor.active = true;
    
    simulation_.production_manager().extractors()[static_cast<EntityId>(extractor_id)] = extractor;
}

void RtsExtension::economy_add_storage(int storage_id, float x, float y, float metal_capacity, float energy_capacity, float research_capacity) {
    rts::Storage storage{};
    storage.x = x;
    storage.y = y;
    storage.metal_capacity = metal_capacity;
    storage.energy_capacity = energy_capacity;
    storage.research_capacity = research_capacity;
    storage.metal_storage = 0.0f;
    storage.energy_storage = 0.0f;
    storage.research_storage = 0.0f;
    
    simulation_.production_manager().add_storage(static_cast<EntityId>(storage_id), storage);
}

void RtsExtension::economy_add_production_line(int line_id, int storage_id, float build_speed_metal, float build_speed_energy, int max_jobs) {
    rts::ProductionLine line{};
    line.storage_id = static_cast<EntityId>(storage_id);
    line.build_speed_metal = build_speed_metal;
    line.build_speed_energy = build_speed_energy;
    line.active_jobs = 0;
    line.max_jobs = max_jobs;
    
    simulation_.production_manager().add_production_line(static_cast<EntityId>(line_id), line);
}

void RtsExtension::economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick) {
    rts::ConstructionQueueEntry entry{};
    entry.entity_id = static_cast<EntityId>(entity_id);
    entry.build_progress = 0.0f;
    entry.total_cost_metal = metal_cost;
    entry.total_cost_energy = energy_cost;
    entry.metal_per_tick = metal_per_tick;
    entry.energy_per_tick = energy_per_tick;
    entry.completed = false;
    
    switch (type) {
        case 0: entry.type = rts::ConstructionQueueEntry::Type::BUILDING; break;
        case 1: entry.type = rts::ConstructionQueueEntry::Type::UNIT; break;
        default: return;
    }
    
    simulation_.production_manager().add_to_queue(static_cast<EntityId>(line_id), entry);
}

void RtsExtension::economy_update_all(float delta_ms) {
    simulation_.update_economy(delta_ms);
}

size_t RtsExtension::issue_move_commands(const Array& entity_ids, int player_id, float x, float y, float spacing) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_move_commands(entities, static_cast<FactionId>(player_id), x, y, spacing);
}

size_t RtsExtension::issue_stop_commands(const Array& entity_ids, int player_id) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_stop_commands(entities, static_cast<FactionId>(player_id));
}

size_t RtsExtension::issue_attack_commands(const Array& entity_ids, int player_id, int target_entity_id) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_attack_commands(entities, static_cast<FactionId>(player_id), static_cast<EntityId>(target_entity_id));
}

size_t RtsExtension::issue_patrol_commands(const Array& entity_ids, int player_id, float x, float y) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_patrol_commands(entities, static_cast<FactionId>(player_id), x, y);
}

size_t RtsExtension::issue_return_commands(const Array& entity_ids, int player_id) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_return_commands(entities, static_cast<FactionId>(player_id));
}

size_t RtsExtension::issue_build_commands(const Array& entity_ids, int player_id, float x, float y, int64_t unit_type) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_build_commands(entities, static_cast<FactionId>(player_id), x, y, unit_type);
}

size_t RtsExtension::issue_harvest_commands(const Array& entity_ids, int player_id, float x, float y) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_harvest_commands(entities, static_cast<FactionId>(player_id), x, y);
}

size_t RtsExtension::issue_defend_commands(const Array& entity_ids, int player_id, float x, float y) {
    if (entity_ids.is_empty()) {
        return 0;
    }
    
    std::vector<EntityId> entities;
    entities.reserve(static_cast<size_t>(entity_ids.size()));
    for (int i = 0; i < entity_ids.size(); ++i) {
        entities.push_back(static_cast<EntityId>(entity_ids[i]));
    }
    
    return simulation_.issue_defend_commands(entities, static_cast<FactionId>(player_id), x, y);
}

void RtsExtension::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_simulation"), &RtsExtension::start_simulation);
    ClassDB::bind_method(D_METHOD("stop_simulation"), &RtsExtension::stop_simulation);
    ClassDB::bind_method(D_METHOD("update_simulation", "delta_ms"), &RtsExtension::update_simulation);
    
    ClassDB::bind_method(D_METHOD("create_unit", "x", "y"), &RtsExtension::create_unit);
    ClassDB::bind_method(D_METHOD("move_unit", "entity_id", "x", "y"), &RtsExtension::move_unit);
    ClassDB::bind_method(D_METHOD("get_entity_count"), &RtsExtension::get_entity_count);
    
    ClassDB::bind_method(D_METHOD("query_units_in_region", "x", "y", "radius"), &RtsExtension::query_units_in_region);
    
    ClassDB::bind_method(D_METHOD("economy_add_resource_node", "node_id", "x", "y", "amount", "type"), &RtsExtension::economy_add_resource_node);
    ClassDB::bind_method(D_METHOD("economy_add_extractor", "extractor_id", "x", "y", "node_id", "extraction_rate"), &RtsExtension::economy_add_extractor);
    ClassDB::bind_method(D_METHOD("economy_add_storage", "storage_id", "x", "y", "metal_capacity", "energy_capacity", "research_capacity"), &RtsExtension::economy_add_storage);
    ClassDB::bind_method(D_METHOD("economy_add_production_line", "line_id", "storage_id", "build_speed_metal", "build_speed_energy", "max_jobs"), &RtsExtension::economy_add_production_line);
    ClassDB::bind_method(D_METHOD("economy_enqueue_construction", "line_id", "entity_id", "type", "metal_cost", "energy_cost", "metal_per_tick", "energy_per_tick"), &RtsExtension::economy_enqueue_construction);
    ClassDB::bind_method(D_METHOD("economy_update_all", "delta_ms"), &RtsExtension::economy_update_all);
    
    ClassDB::bind_method(D_METHOD("issue_move_commands", "entity_ids", "player_id", "x", "y", "spacing"), &RtsExtension::issue_move_commands);
    ClassDB::bind_method(D_METHOD("issue_stop_commands", "entity_ids", "player_id"), &RtsExtension::issue_stop_commands);
    ClassDB::bind_method(D_METHOD("issue_attack_commands", "entity_ids", "player_id", "target_entity_id"), &RtsExtension::issue_attack_commands);
    ClassDB::bind_method(D_METHOD("issue_patrol_commands", "entity_ids", "player_id", "x", "y"), &RtsExtension::issue_patrol_commands);
    ClassDB::bind_method(D_METHOD("issue_return_commands", "entity_ids", "player_id"), &RtsExtension::issue_return_commands);
    ClassDB::bind_method(D_METHOD("issue_build_commands", "entity_ids", "player_id", "x", "y", "unit_type"), &RtsExtension::issue_build_commands);
    ClassDB::bind_method(D_METHOD("issue_harvest_commands", "entity_ids", "player_id", "x", "y"), &RtsExtension::issue_harvest_commands);
    ClassDB::bind_method(D_METHOD("issue_defend_commands", "entity_ids", "player_id", "x", "y"), &RtsExtension::issue_defend_commands);
}

} // namespace rts
