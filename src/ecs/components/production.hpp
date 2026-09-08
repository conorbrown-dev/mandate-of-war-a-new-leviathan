#pragma once

#include <cstdint>
#include <vector>
#include <queue>

#include "ecs/entity.hpp"
#include "factions.hpp"

namespace rts {

struct ResourceNode {
    float x, y;
    float amount;
    float max_amount;
    enum class Type : uint8_t {
        METAL,
        ENERGY,
        RESEARCH
    } type;
    bool depleted;
};

struct Extractor {
    float x, y;
    EntityId resource_node_id;
    float extraction_rate;
    float last_extraction_tick;
    bool active;
};

struct Storage {
    float x, y;
    float metal_storage;
    float energy_storage;
    float research_storage;
    float metal_capacity;
    float energy_capacity;
    float research_capacity;
};

struct ConstructionQueueEntry {
    EntityId entity_id;
    enum class Type : uint8_t {
        BUILDING,
        UNIT
    } type;
    UnitType unit_type;  // Valid when type == UNIT
    FactionId faction_id; // Faction producing the unit
    float build_progress;
    float total_cost_metal;
    float total_cost_energy;
    float total_cost_research;
    float metal_per_tick;
    float energy_per_tick;
    float research_per_tick;
    float build_time_seconds;
    bool completed;
};

struct ProductionLine {
    EntityId storage_id;
    std::queue<ConstructionQueueEntry> queue;
    float build_speed_metal;
    float build_speed_energy;
    int active_jobs;
    int max_jobs;
};

struct Transport {
    float x, y;
    float target_x, target_y;
    float metal_cargo;
    float energy_cargo;
    float cargo_capacity;
    float speed;
    enum class State : uint8_t {
        IDLE,
        TO_STORAGE,
        TO_CONSTRUCTION,
        WAITING
    } state;
    EntityId source_storage_id;
    EntityId destination_id;
};

} // namespace rts
