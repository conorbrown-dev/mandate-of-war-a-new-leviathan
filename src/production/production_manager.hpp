#pragma once

#include <unordered_map>
#include <vector>
#include <queue>

#include "ecs/entity.hpp"
#include "ecs/components/production.hpp"
#include "ecs/components/factions.hpp"

namespace rts {

class Simulation;

struct CompletedConstruction {
    EntityId queue_entity_id;
    UnitType unit_type;
    FactionId faction_id;
    float x, y;
};

class ProductionManager {
public:
    ProductionManager() = default;
    
    void add_resource_node(EntityId node_id, const ResourceNode& node);
    void extract_resource(EntityId extractor_id, float delta_ms);
    
    void add_storage(EntityId storage_id, const Storage& storage);
    void update_storage(EntityId storage_id, float metal, float energy, float research);
    
    void add_to_queue(EntityId production_line_id, const ConstructionQueueEntry& entry);
    void complete_construction(EntityId entity_id);
    
    void add_production_line(EntityId line_id, const ProductionLine& line);
    
    // Add unit to production queue by type
    void add_unit_to_queue(EntityId production_line_id, FactionId faction_id, UnitType unit_type);
    
    void add_transport(EntityId transport_id, const Transport& transport);
    void update_transports(float delta_ms);
    
    void update_all(float delta_ms);
    void reset();

    std::unordered_map<EntityId, ResourceNode>& resource_nodes() { return resource_nodes_; }
    std::unordered_map<EntityId, Extractor>& extractors() { return extractors_; }
    std::unordered_map<EntityId, Storage>& storages() { return storages_; }
    const std::unordered_map<EntityId, Storage>& storages() const { return storages_; }
    std::unordered_map<EntityId, ProductionLine>& production_lines() { return production_lines_; }
    const std::unordered_map<EntityId, ProductionLine>& production_lines() const { return production_lines_; }
    std::unordered_map<EntityId, Transport>& transports() { return transports_; }

    bool verify_extraction_rate(EntityId extractor_id, float expected_rate, float tolerance = 0.01f);
    bool verify_storage_capacity(EntityId storage_id, float expected_metal, float expected_energy, float expected_research);
    bool verify_queue_length(EntityId production_line_id, int expected_length);
    
    // Faction-specific methods
    void add_faction_production_line(FactionId faction_id, EntityId line_id);
    bool can_produce_unit(FactionId faction_id, UnitType unit_type);
    bool can_queue_unit(EntityId line, FactionId faction, UnitType type) const;
    bool queue_unit(EntityId line, FactionId faction, UnitType type, float x = 0.0f, float y = 0.0f);
    bool can_queue_structure(EntityId line, FactionId faction, uint8_t structure_type) const;
    bool queue_structure(EntityId line, FactionId faction, uint8_t structure_type, float x = 0.0f, float y = 0.0f);
    EntityId faction_line(FactionId faction) const;
    const FactionResearch& research(FactionId faction) const;
    bool can_research(FactionId faction, const std::string& project) const;
    bool begin_research(FactionId faction, const std::string& project);
    
    // Economy: deduct resources from faction's production line storage
    bool deduct_faction_resources(FactionId faction, float metal, float energy);
    float research_progress(FactionId faction) const;
    float get_unit_metal_cost(FactionId faction_id, UnitType unit_type) const;
    float get_unit_energy_cost(FactionId faction_id, UnitType unit_type) const;
    float get_unit_research_cost(FactionId faction_id, UnitType unit_type) const;
    
    void set_faction_research(FactionId faction_id, const FactionResearch& research);
    
    // Completed constructions since last update
    std::vector<CompletedConstruction>& get_completed_constructions() { return completed_constructions_; }
    void clear_completed_constructions() { completed_constructions_.clear(); }
    
    int get_extraction_count() const { return extraction_count_; }
    int get_construction_count() const { return construction_count_; }
    int get_transport_count() const { return transport_count_; }
    
    // Extractor lookup/creation for HARVEST command
    bool find_or_create_extractor(float x, float y, EntityId& extractor_id, EntityId& node_id);
    bool destroy_resource_site(float x, float y);

private:
    std::unordered_map<EntityId, ResourceNode> resource_nodes_;
    std::unordered_map<EntityId, Extractor> extractors_;
    std::unordered_map<EntityId, Storage> storages_;
    std::unordered_map<EntityId, ProductionLine> production_lines_;
    std::unordered_map<EntityId, Transport> transports_;
    
    // Faction production lines keyed by faction_id
    std::unordered_map<FactionId, EntityId> faction_production_lines_;

    int extraction_count_ = 0;
    int construction_count_ = 0;
    int transport_count_ = 0;
    
    // Research state per faction
    std::unordered_map<FactionId, FactionResearch> faction_research_;
    std::unordered_map<FactionId, float> research_elapsed_;
    
    // Queue of completed constructions (to be spawned by Simulation)
    std::vector<CompletedConstruction> completed_constructions_;

    void update_extractors(float delta_ms);
    void update_construction_queues(float delta_ms);
};

} // namespace rts
