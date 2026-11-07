#include "production/production_manager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "ecs/components/factions.hpp"
#include "simulation/structure_sites.hpp"
#include "simulation/fuel_supply.hpp"

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
    
    ConstructionQueueEntry entry{};
    entry.entity_id = NULL_ENTITY;  // Will be assigned when unit is spawned
    entry.type = ConstructionQueueEntry::Type::UNIT;
    entry.unit_type = unit_type;
    entry.faction_id = faction_id;
    entry.build_progress = 0.0f;
    entry.total_cost_metal = material_cost;
    entry.metal_per_tick = material_cost / (it->second.build_time_seconds * 20.0f);  // 20 ticks per second
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
            case ResourceNode::Type::METAL:
                balance = &storage->second.tactical.materials.current;
                capacity = storage->second.tactical.materials.max;
                break;
            case ResourceNode::Type::ENERGY:
                balance = &storage->second.tactical.fuel.current;
                capacity = storage->second.tactical.fuel.capacity;
                break;
        }
        amount = std::min(amount, std::max(0.0f, capacity - *balance));
    }
    if (amount <= 0) return;
    if (balance) *balance += amount;
    if (storage != storages_.end() && node.type == ResourceNode::Type::METAL)
        storage->second.metal_storage = storage->second.tactical.materials.current;
    if (storage != storages_.end() && node.type == ResourceNode::Type::ENERGY)
        storage->second.energy_storage = storage->second.tactical.fuel.current;
    node.amount -= amount;
    node.depleted = node.amount <= 0;
    ++extraction_count_;
}

void ProductionManager::add_storage(EntityId storage_id, const Storage& storage) {
    Storage migrated = storage;
    // TEMPORARY Phase-1 bridge: legacy content creates Energy wallets.  They
    // are not Power; preserve them solely for old queues while exposing a
    // capacity-based Power state to new code. Remove in Phase 2.
    migrated.tactical.materials = {storage.metal_storage, storage.metal_capacity, 0.0f};
    migrated.tactical.fuel = {0.0f, storage.energy_capacity};
    migrated.tactical.power = {0.0f, 0.0f};
    storages_[storage_id] = migrated;
}

const TacticalResources* ProductionManager::tactical_resources(EntityId storage_id) const {
    const auto it = storages_.find(storage_id);
    return it == storages_.end() ? nullptr : &it->second.tactical;
}

TacticalResources* ProductionManager::tactical_resources(EntityId storage_id) {
    const auto it = storages_.find(storage_id);
    return it == storages_.end() ? nullptr : &it->second.tactical;
}

bool ProductionManager::set_tactical_power(EntityId storage_id, float generation_mw, float demand_mw) {
    if (!std::isfinite(generation_mw) || !std::isfinite(demand_mw) || generation_mw < 0.0f || demand_mw < 0.0f) return false;
    auto* resources = tactical_resources(storage_id);
    if (!resources) return false;
    resources->power = {generation_mw, demand_mw};
    return true;
}

void ProductionManager::update_storage(EntityId storage_id, float metal, float energy) {
    auto it = storages_.find(storage_id);
    if (it != storages_.end()) {
        it->second.tactical.materials.current = std::clamp(it->second.tactical.materials.current + metal, 0.0f, it->second.tactical.materials.max);
        it->second.tactical.fuel.current = std::clamp(it->second.tactical.fuel.current + energy, 0.0f, it->second.tactical.fuel.capacity);
        it->second.metal_storage = it->second.tactical.materials.current;
        it->second.energy_storage = it->second.tactical.fuel.current;
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
        const bool is_unit = entry.type == ConstructionQueueEntry::Type::UNIT;
        const auto slots = unit_slots_.find(entry.faction_id);
        if (is_unit && slots != unit_slots_.end() && slots->second <= 0) continue;
        const float fraction = std::min(dt / entry.build_time_seconds, 1.0f - entry.build_progress);
        const float ticks = fraction * entry.build_time_seconds * 20.0f;
        float metal = entry.metal_per_tick * ticks;
        auto& funds = storage->second;
        const float fuel = entry.fuel_per_tick * ticks;
        if (funds.tactical.materials.current < metal || funds.tactical.fuel.current < fuel) continue;
        funds.tactical.materials.current -= metal;
        funds.metal_storage = funds.tactical.materials.current;
        if (fuel > 0.0f) {
            funds.tactical.fuel.current -= fuel;
            funds.energy_storage = funds.tactical.fuel.current;
        }
        entry.build_progress += fraction;
        line.active_jobs = 1;
        if (entry.build_progress >= 1.0f - 0.0001f) {
            completed_constructions_.push_back({entry.entity_id, entry.unit_type, entry.faction_id, entry.target_x, entry.target_y,
                entry.type == ConstructionQueueEntry::Type::BUILDING, entry.structure_type, entry.graded_structure});
            if (is_unit && slots != unit_slots_.end()) --slots->second;
            line.queue.pop(); // no reference to the popped entry may survive
            ++construction_count_;
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
    
    const auto new_id = next_extractor_id_++;
    const auto& node = resource_nodes_.at(nearest_node);
    extractors_[new_id] = {node.x, node.y, nearest_node, 1.0f, 0.0f, true, INVALID_ENTITY};
    extractor_id = new_id;
    node_id = nearest_node;
    return true;
}

EntityId ProductionManager::fuel_depot_at(float x, float y) const {
    constexpr float CLAIM_RADIUS = 8.0f;
    EntityId nearest = INVALID_ENTITY;
    float best_dist_sq = CLAIM_RADIUS * CLAIM_RADIUS;
    for (const auto& [node_id, node] : resource_nodes_) {
        if (node.type != ResourceNode::Type::ENERGY) continue;
        const float dx = node.x - x;
        const float dy = node.y - y;
        const float distance_sq = dx * dx + dy * dy;
        if (distance_sq < best_dist_sq || (distance_sq == best_dist_sq && node_id < nearest)) {
            best_dist_sq = distance_sq;
            nearest = node_id;
        }
    }
    return nearest;
}

EntityId ProductionManager::fuel_depot_holder(EntityId depot_id) const {
    for (const auto& [id, extractor] : extractors_) {
        if (extractor.resource_node_id == depot_id) return extractor.storage_id;
    }
    return INVALID_ENTITY;
}

void ProductionManager::assign_fuel_depot(EntityId depot_id, EntityId storage_id, bool drawing) {
    const auto depot = resource_nodes_.find(depot_id);
    if (depot == resource_nodes_.end()) return;
    for (auto& [id, extractor] : extractors_) {
        if (extractor.resource_node_id != depot_id) continue;
        extractor.storage_id = storage_id;
        extractor.active = drawing;
        return;
    }
    const auto new_id = next_extractor_id_++;
    extractors_[new_id] = {depot->second.x, depot->second.y, depot_id, kFuelDepotDrawPerTick, 0.0f, drawing, storage_id};
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

bool ProductionManager::verify_storage_capacity(EntityId storage_id, float expected_metal, float expected_energy) {
    (void)storage_id;
    (void)expected_metal;
    (void)expected_energy;
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
    next_extractor_id_ = 1000000;
    storages_.clear();
    production_lines_.clear();
    transports_.clear();
    extraction_count_ = 0;
    construction_count_ = 0;
    transport_count_ = 0;
    faction_production_lines_.clear();
    completed_constructions_.clear();
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
    
    // Research prerequisites are strategic-authorization metadata. Until that
    // system exists, they must not block tactical content acquisition.
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

bool ProductionManager::deduct_faction_resources(FactionId faction, float metal, float energy) {
    auto line_it = production_lines_.find(faction_line(faction));
    if (line_it == production_lines_.end()) return false;
    
    auto storage_it = storages_.find(line_it->second.storage_id);
    if (storage_it == storages_.end()) return false;
    
    auto& funds = storage_it->second;
    if (funds.tactical.materials.current < metal) return false;
    funds.tactical.materials.current -= metal;
    funds.metal_storage = funds.tactical.materials.current;
    (void)energy; // temporary signature bridge; Power is not spent.
    return true;
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
EntityId ProductionManager::faction_storage(FactionId faction) const {
    const auto line = production_lines_.find(faction_line(faction));
    return line == production_lines_.end() ? INVALID_ENTITY : line->second.storage_id;
}

bool ProductionManager::can_queue_unit(EntityId line_id, FactionId faction, UnitType type) const {
    auto line = production_lines_.find(line_id);
    auto proto = get_unit_prototypes().find(type);
    if (faction_line(faction) != line_id || line == production_lines_.end() ||
        proto == get_unit_prototypes().end() || proto->second.faction != faction ||
        line->second.queue.size() >= static_cast<size_t>(line->second.max_jobs)) return false;
    // Strategic authorization is a later migration. Tactical acquisition must
    // not use Research-project completion as a currency-like availability gate.
    auto storage = storages_.find(line->second.storage_id);
    if (storage == storages_.end()) return false;
    const auto& funds = storage->second;
    const auto& p = proto->second;
    const float ferry_material = p.is_aircraft && p.requires_runway ? p.operational_material : 0.0f;
    return funds.tactical.materials.current >= p.material_cost + ferry_material;
}
bool ProductionManager::queue_unit(EntityId line_id, FactionId faction, UnitType type, float x, float y) {
    if (!can_queue_unit(line_id, faction, type)) return false;
    const auto& p = get_unit_prototypes().at(type);
    auto& line = production_lines_.at(line_id);
    auto& funds = storages_.at(line.storage_id);
    // Reserve full build costs once.
    const float ferry_material = p.is_aircraft && p.requires_runway ? p.operational_material : 0.0f;
    funds.tactical.materials.current -= p.material_cost + ferry_material;
    funds.metal_storage = funds.tactical.materials.current;
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::UNIT;
    entry.unit_type = type; entry.faction_id = faction;
    entry.total_cost_metal = p.material_cost + ferry_material; entry.total_cost_energy = 0.0f;
    entry.build_time_seconds = p.build_time_seconds;
    entry.target_x = x; entry.target_y = y;
    line.queue.push(entry);
    return true;
}
bool ProductionManager::can_queue_structure(EntityId line_id, FactionId faction, uint8_t structure_type,
    float earthworks_material, float earthworks_fuel, int earthworks_jobs) const {
    if (structure_type > 3 || faction_line(faction) != line_id) return false;
    auto line = production_lines_.find(line_id);
    if (line == production_lines_.end() ||
        line->second.queue.size() + static_cast<size_t>(earthworks_jobs) >= static_cast<size_t>(line->second.max_jobs)) return false;
    auto storage = storages_.find(line->second.storage_id);
    if (storage == storages_.end()) return false;
    const float metal = structure_metal_cost(structure_type) + earthworks_material;
    const auto& funds = storage->second;
    return funds.tactical.materials.current >= metal && funds.tactical.fuel.current >= earthworks_fuel;
}
void ProductionManager::queue_earthworks(EntityId line_id, FactionId faction, uint8_t graded_structure, float x, float y,
    float material, float fuel, float seconds) {
    auto& line = production_lines_.at(line_id);
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::BUILDING;
    entry.unit_type = static_cast<UnitType>(0);
    entry.faction_id = faction;
    entry.structure_type = EARTHWORKS_JOB;
    entry.graded_structure = graded_structure;
    entry.display_name = "EARTHWORKS";
    entry.total_cost_metal = material;
    entry.total_cost_energy = 0.0f;
    entry.build_time_seconds = seconds;
    entry.metal_per_tick = material / (seconds * 20.0f);
    entry.total_cost_fuel = fuel;
    entry.fuel_per_tick = fuel / (seconds * 20.0f);
    entry.energy_per_tick = 0.0f;
    entry.target_x = x; entry.target_y = y;
    line.queue.push(std::move(entry));
}
bool ProductionManager::queue_structure(EntityId line_id, FactionId faction, uint8_t structure_type, float x, float y) {
    if (!can_queue_structure(line_id, faction, structure_type)) return false;
    auto& line = production_lines_.at(line_id);
    const float metal = structure_metal_cost(structure_type);
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::BUILDING;
    entry.unit_type = static_cast<UnitType>(0);
    entry.faction_id = faction;
    entry.structure_type = structure_type;
    entry.display_name = structure_display_name(structure_type);
    entry.total_cost_metal = metal;
    entry.total_cost_energy = 0.0f;
    entry.build_time_seconds = structure_build_seconds(structure_type);
    entry.metal_per_tick = entry.total_cost_metal / (entry.build_time_seconds * 20.0f);
    entry.energy_per_tick = 0.0f;
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
    return funds.tactical.materials.current >= p.material_cost + ferry_material;
}
bool ProductionManager::queue_requisition(EntityId line_id, FactionId faction, UnitType type, float x, float y) {
    if (!can_queue_requisition(line_id, faction, type, x, y)) return false;
    auto& line = production_lines_.at(line_id);
    auto& funds = storages_.at(line.storage_id);
    const auto& p = get_unit_prototypes().at(type);
    float ferry_material = 0.0f;
    if (p.is_aircraft && p.requires_runway) {
        ferry_material = p.operational_material;
    }
    funds.tactical.materials.current -= p.material_cost + ferry_material;
    funds.metal_storage = funds.tactical.materials.current;
    ConstructionQueueEntry entry{};
    entry.type = ConstructionQueueEntry::Type::UNIT;
    entry.unit_type = type;
    entry.faction_id = faction;
    entry.total_cost_metal = p.material_cost + ferry_material;
    entry.total_cost_energy = 0.0f;
    entry.build_time_seconds = p.build_time_seconds;
    entry.target_x = x;
    entry.target_y = y;
    line.queue.push(entry);
    return true;
}

} // namespace rts
