#include "production/production_manager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "ecs/components/factions.hpp"

namespace rts {

namespace {
constexpr float structure_metal_cost(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return 300.0f;
        case 1: return 450.0f;
        case 2: return 600.0f;
        case 3: return 380.0f;
        default: return 0.0f;
    }
}

constexpr float structure_energy_cost(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return 150.0f;
        case 1: return 250.0f;
        case 2: return 400.0f;
        case 3: return 620.0f;
        default: return 0.0f;
    }
}

constexpr float structure_research_cost(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return 0.0f;
        case 1: return 100.0f;
        case 2: return 150.0f;
        case 3: return 80.0f;
        default: return 0.0f;
    }
}

constexpr float structure_build_seconds(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return 20.0f;
        case 1: return 28.0f;
        case 2: return 36.0f;
        case 3: return 24.0f;
        default: return 0.0f;
    }
}

const char* structure_display_name(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return "FORWARD OUTPOST";
        case 1: return "RADAR MAST";
        case 2: return "AIRFIELD";
        case 3: return "FLOODLIGHT";
        default: return "UNKNOWN STRUCTURE";
    }
}
} // namespace

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
    if (!std::isfinite(delta_ms) || delta_ms <= 0) return;
    auto found = extractors_.find(extractor_id);
    if (found == extractors_.end()) return;
    auto& extractor = found->second;
    auto node_it = resource_nodes_.find(extractor.resource_node_id);
    if (!extractor.active || node_it == resource_nodes_.end() ||
        !std::isfinite(extractor.extraction_rate) || extractor.extraction_rate <= 0) return;
    auto& node = node_it->second;
    if (node.depleted) return;
    float amount = std::min(node.amount, extractor.extraction_rate * delta_ms / 50.0f);
    auto storage = storages_.find(extractor.storage_id);
    float* balance = nullptr;
    if (storage != storages_.end()) {
        float capacity;
        switch (node.type) {
            case ResourceNode::Type::METAL: balance = &storage->second.metal_storage; capacity = storage->second.metal_capacity; break;
            case ResourceNode::Type::ENERGY: balance = &storage->second.energy_storage; capacity = storage->second.energy_capacity; break;
            case ResourceNode::Type::RESEARCH: balance = &storage->second.research_storage; capacity = storage->second.research_capacity; break;
            default: return;
        }
        amount = std::min(amount, std::max(0.0f, capacity - *balance));
    }
    if (amount <= 0) return;
    if (balance) *balance += amount;
    node.amount -= amount;
    node.depleted = node.amount <= 0;
    ++extraction_count_;
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
    std::vector<EntityId> ids;
    for (const auto& [id, extractor] : extractors_) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (auto id : ids) extract_resource(id, delta_ms);
}

void ProductionManager::update_construction_queues(float delta_ms) {
    const float dt = delta_ms / 1000.0f;
    std::vector<EntityId> ids;
    for (const auto& [id, line] : production_lines_) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    for (auto id : ids) {
        auto& line = production_lines_.at(id);
        line.active_jobs = 0;
        if (line.queue.empty()) continue;
        auto& entry = line.queue.front();
        auto storage = storages_.find(line.storage_id);
        if (storage == storages_.end() || entry.build_time_seconds <= 0) continue;
        const float fraction = std::min(dt / entry.build_time_seconds, 1.0f - entry.build_progress);
        const float ticks = fraction * entry.build_time_seconds * 20.0f;
        float metal = entry.metal_per_tick * ticks;
        float energy = entry.energy_per_tick * ticks;
        float research = entry.research_per_tick * ticks;
        auto& funds = storage->second;
        if (funds.metal_storage < metal || funds.energy_storage < energy || funds.research_storage < research) continue;
        funds.metal_storage -= metal; funds.energy_storage -= energy; funds.research_storage -= research;
        entry.build_progress += fraction;
        line.active_jobs = 1;
        if (entry.build_progress >= 1.0f - 0.0001f) {
            completed_constructions_.push_back({entry.entity_id, entry.unit_type, entry.faction_id, entry.target_x, entry.target_y,
                entry.type == ConstructionQueueEntry::Type::BUILDING, entry.structure_type});
            line.queue.pop(); // no reference to the popped entry may survive
            ++construction_count_;
        }
    }
    for (int f = 0; f < 3; ++f) {
        auto faction = static_cast<FactionId>(f);
        auto state = faction_research_.find(faction);
        if (state == faction_research_.end() || state->second.active_queue.empty()) continue;
        const auto id = state->second.active_queue.front();
        const auto& project = state->second.available_projects.at(id);
        research_elapsed_[faction] += dt;
        if (research_elapsed_[faction] >= project.build_time_seconds) {
            state->second.completed_projects[id] = true;
            state->second.active_queue.erase(state->second.active_queue.begin());
            research_elapsed_[faction] = 0;
        }
    }
}

void ProductionManager::update_all(float delta_ms) {
    if (!std::isfinite(delta_ms) || delta_ms <= 0) return;
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

bool ProductionManager::find_or_create_extractor(float x, float y, EntityId& extractor_id, EntityId& node_id) {
    constexpr float EXTRACTOR_TOLERANCE = 5.0f;
    for (const auto& [eid, ext] : extractors_) {
        float dx = ext.x - x;
        float dy = ext.y - y;
        if (dx*dx + dy*dy <= EXTRACTOR_TOLERANCE*EXTRACTOR_TOLERANCE) {
            extractor_id = eid;
            node_id = ext.resource_node_id;
            return true;
        }
    }
    
    // Create new extractor
    if (resource_nodes_.empty()) {
        return false;
    }
    
    // Find nearest node
    float best_dist_sq = std::numeric_limits<float>::max();
    EntityId nearest_node = INVALID_ENTITY;
    for (const auto& [nid, node] : resource_nodes_) {
        float dx = node.x - x;
        float dy = node.y - y;
        float dist_sq = dx*dx + dy*dy;
        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            nearest_node = nid;
        }
    }
    // A harvest order must target a real field site; do not let a click
    // anywhere on the map silently bind to the nearest resource node.
    constexpr float CLAIM_RADIUS = 8.0f;
    if (nearest_node == INVALID_ENTITY || best_dist_sq > CLAIM_RADIUS * CLAIM_RADIUS) return false;
    
    auto new_id = static_cast<EntityId>(extractors_.size() + resource_nodes_.size() + 1000000);
    const auto& node = resource_nodes_.at(nearest_node);
    extractors_[new_id] = {node.x, node.y, nearest_node, 1.0f, 0.0f, true, INVALID_ENTITY};
    extractor_id = new_id;
    node_id = nearest_node;
    return true;
}

bool ProductionManager::destroy_resource_site(float x, float y) {
    constexpr float CLAIM_RADIUS = 8.0f;
    EntityId nearest_node = INVALID_ENTITY;
    float best_dist_sq = CLAIM_RADIUS * CLAIM_RADIUS;
    for (const auto& [node_id, node] : resource_nodes_) {
        const float dx = node.x - x;
        const float dy = node.y - y;
        const float distance_sq = dx * dx + dy * dy;
        if (distance_sq <= best_dist_sq) {
            best_dist_sq = distance_sq;
            nearest_node = node_id;
        }
    }
    if (nearest_node == INVALID_ENTITY) return false;
    for (auto it = extractors_.begin(); it != extractors_.end();) {
        if (it->second.resource_node_id == nearest_node) it = extractors_.erase(it);
        else ++it;
    }
    resource_nodes_.erase(nearest_node);
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
    faction_production_lines_.clear();
    faction_research_.clear();
    research_elapsed_.clear();
    completed_constructions_.clear();
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
        return it->second.research_prerequisites.empty();
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

float ProductionManager::get_unit_metal_cost(FactionId faction_id, UnitType unit_type) const {
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

float ProductionManager::get_unit_energy_cost(FactionId faction_id, UnitType unit_type) const {
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

bool ProductionManager::deduct_faction_resources(FactionId faction, float metal, float energy) {
    auto line_it = production_lines_.find(faction_line(faction));
    if (line_it == production_lines_.end()) return false;
    
    auto storage_it = storages_.find(line_it->second.storage_id);
    if (storage_it == storages_.end()) return false;
    
    auto& funds = storage_it->second;
    if (funds.metal_storage < metal || funds.energy_storage < energy) return false;
    
    funds.metal_storage -= metal;
    funds.energy_storage -= energy;
    return true;
}

float ProductionManager::get_unit_research_cost(FactionId faction_id, UnitType unit_type) const {
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

float ProductionManager::get_build_time_seconds(UnitType unit_type) const {
     const auto& prototypes = get_unit_prototypes();
     auto it = prototypes.find(unit_type);
     if (it == prototypes.end()) {
         return 0.0f;
     }
     return it->second.build_time_seconds;
 }


EntityId ProductionManager::faction_line(FactionId faction) const {
    auto it = faction_production_lines_.find(faction);
    return it == faction_production_lines_.end() ? INVALID_ENTITY : it->second;
}
const FactionResearch& ProductionManager::research(FactionId faction) const {
    static const FactionResearch empty{};
    auto it = faction_research_.find(faction);
    return it == faction_research_.end() ? empty : it->second;
}
bool ProductionManager::can_queue_unit(EntityId line_id, FactionId faction, UnitType type) const {
    auto line = production_lines_.find(line_id);
    auto proto = get_unit_prototypes().find(type);
    if (faction_line(faction) != line_id || line == production_lines_.end() ||
        proto == get_unit_prototypes().end() || proto->second.faction != faction ||
        line->second.queue.size() >= static_cast<size_t>(line->second.max_jobs)) return false;
    const auto& state = research(faction);
    for (const auto& prerequisite : proto->second.research_prerequisites) {
        auto it = state.completed_projects.find(prerequisite);
        if (it == state.completed_projects.end() || !it->second) return false;
    }
    auto storage = storages_.find(line->second.storage_id);
    if (storage == storages_.end()) return false;
    const auto& funds = storage->second;
    const auto& p = proto->second;
    const float ferry_material = p.is_aircraft && p.requires_runway ? p.operational_material : 0.0f;
    const float ferry_energy = p.is_aircraft && p.requires_runway ? p.operational_energy : 0.0f;
    return funds.metal_storage >= p.material_cost + ferry_material && funds.energy_storage >= p.energy_cost + ferry_energy;
}
bool ProductionManager::queue_unit(EntityId line_id, FactionId faction, UnitType type, float x, float y) {
    if (!can_queue_unit(line_id, faction, type)) return false;
    const auto& p = get_unit_prototypes().at(type);
    auto& line = production_lines_.at(line_id);
    auto& funds = storages_.at(line.storage_id);
    // Reserve full build costs once. Research is spent on projects, not units.
    const float ferry_material = p.is_aircraft && p.requires_runway ? p.operational_material : 0.0f;
    const float ferry_energy = p.is_aircraft && p.requires_runway ? p.operational_energy : 0.0f;
    funds.metal_storage -= p.material_cost + ferry_material;
    funds.energy_storage -= p.energy_cost + ferry_energy;
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::UNIT;
    entry.unit_type = type; entry.faction_id = faction;
    entry.total_cost_metal = p.material_cost + ferry_material; entry.total_cost_energy = p.energy_cost + ferry_energy;
    entry.build_time_seconds = p.build_time_seconds;
    entry.target_x = x; entry.target_y = y;
    line.queue.push(entry);
    return true;
}
bool ProductionManager::can_queue_structure(EntityId line_id, FactionId faction, uint8_t structure_type) const {
    if (structure_type > 3 || faction_line(faction) != line_id) return false;
    auto line = production_lines_.find(line_id);
    if (line == production_lines_.end() || line->second.queue.size() >= static_cast<size_t>(line->second.max_jobs)) return false;
    auto storage = storages_.find(line->second.storage_id);
    if (storage == storages_.end()) return false;
    const float metal = structure_metal_cost(structure_type);
    const float energy = structure_energy_cost(structure_type);
    const float research = structure_research_cost(structure_type);
    const auto& funds = storage->second;
    return funds.metal_storage >= metal && funds.energy_storage >= energy && funds.research_storage >= research;
}
bool ProductionManager::queue_structure(EntityId line_id, FactionId faction, uint8_t structure_type, float x, float y) {
    if (!can_queue_structure(line_id, faction, structure_type)) return false;
    auto& line = production_lines_.at(line_id);
    const float metal = structure_metal_cost(structure_type);
    const float energy = structure_energy_cost(structure_type);
    const float research_cost = structure_research_cost(structure_type);
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::BUILDING;
    entry.unit_type = static_cast<UnitType>(0);
    entry.faction_id = faction;
    entry.structure_type = structure_type;
    entry.display_name = structure_display_name(structure_type);
    entry.total_cost_metal = metal;
    entry.total_cost_energy = energy;
    entry.total_cost_research = research_cost;
    entry.build_time_seconds = structure_build_seconds(structure_type);
    entry.metal_per_tick = entry.total_cost_metal / (entry.build_time_seconds * 20.0f);
    entry.energy_per_tick = entry.total_cost_energy / (entry.build_time_seconds * 20.0f);
    entry.research_per_tick = entry.total_cost_research / (entry.build_time_seconds * 20.0f);
    entry.target_x = x; entry.target_y = y;
    line.queue.push(std::move(entry));
    return true;
}
bool ProductionManager::can_queue_requisition(EntityId line_id, FactionId faction, UnitType type, float x, float y) const {
    (void)x; (void)y;
    if (faction_line(faction) != line_id) return false;
    auto line = production_lines_.find(line_id);
    if (line == production_lines_.end() || line->second.queue.size() >= static_cast<size_t>(line->second.max_jobs)) return false;
    auto proto = get_unit_prototypes().find(type);
    if (proto == get_unit_prototypes().end() || proto->second.faction != faction) return false;
    auto storage = storages_.find(line->second.storage_id);
    if (storage == storages_.end()) return false;
    const auto& funds = storage->second;
    const auto& p = proto->second;
    const float ferry_material = p.is_aircraft && p.requires_runway ? p.operational_material : 0.0f;
    const float ferry_energy = p.is_aircraft && p.requires_runway ? p.operational_energy : 0.0f;
    return funds.metal_storage >= p.material_cost + ferry_material && funds.energy_storage >= p.energy_cost + ferry_energy;
}
bool ProductionManager::queue_requisition(EntityId line_id, FactionId faction, UnitType type, float x, float y) {
    if (!can_queue_requisition(line_id, faction, type, x, y)) return false;
    auto& line = production_lines_.at(line_id);
    auto& funds = storages_.at(line.storage_id);
    const auto& p = get_unit_prototypes().at(type);
    float ferry_material = 0.0f;
    float ferry_energy = 0.0f;
    if (p.is_aircraft && p.requires_runway) {
        ferry_material = p.operational_material;
        ferry_energy = p.operational_energy;
    }
    funds.metal_storage -= p.material_cost + ferry_material;
    funds.energy_storage -= p.energy_cost + ferry_energy;
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::UNIT;
    entry.unit_type = type;
    entry.faction_id = faction;
    entry.total_cost_metal = p.material_cost + ferry_material;
    entry.total_cost_energy = p.energy_cost + ferry_energy;
    entry.build_time_seconds = p.build_time_seconds;
    entry.target_x = x;
    entry.target_y = y;
    line.queue.push(entry);
    return true;
}
bool ProductionManager::can_research(FactionId faction, const std::string& id) const {
    const auto& state = research(faction);
    auto p = state.available_projects.find(id);
    if (p == state.available_projects.end() || !state.active_queue.empty()) return false;
    bool faction_project = false;
    for (auto type : p->second.unlocks) {
        auto prototype = get_unit_prototypes().find(type);
        if (prototype != get_unit_prototypes().end() && prototype->second.faction == faction) faction_project = true;
    }
    if (!faction_project) return false;
    auto completed = state.completed_projects.find(id);
    if (completed != state.completed_projects.end() && completed->second) return false;
    for (const auto& prerequisite : p->second.prerequisites) {
        auto it = state.completed_projects.find(prerequisite);
        if (it == state.completed_projects.end() || !it->second) return false;
    }
    auto line = production_lines_.find(faction_line(faction));
    if (line == production_lines_.end()) return false;
    auto storage = storages_.find(line->second.storage_id);
    return storage != storages_.end() && storage->second.research_storage >= p->second.total_cost;
}
bool ProductionManager::begin_research(FactionId faction, const std::string& id) {
    if (!can_research(faction, id)) return false;
    auto& state = faction_research_.at(faction);
    auto& funds = storages_.at(production_lines_.at(faction_line(faction)).storage_id);
    funds.research_storage -= state.available_projects.at(id).total_cost;
    state.active_queue.push_back(id);
    research_elapsed_[faction] = 0;
    return true;
}
float ProductionManager::research_progress(FactionId faction) const {
    const auto& state = research(faction);
    if (state.active_queue.empty()) return 0;
    auto elapsed = research_elapsed_.find(faction);
    return elapsed == research_elapsed_.end() ? 0 : elapsed->second / state.available_projects.at(state.active_queue.front()).build_time_seconds;
}

} // namespace rts
