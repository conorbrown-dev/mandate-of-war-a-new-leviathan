#include "simulation/simulation.hpp"
#include "production/production_manager.hpp"
#include "catch.hpp"

TEST_CASE("Economy - Resource Nodes", "[economy]") {
    rts::Simulation sim;
    sim.start();
    
    int node_id = 1001;
    float x = 100.0f, y = 100.0f;
    float amount = 1000.0f;
    int type = 0; // METAL
    
    sim.production_manager().add_resource_node(static_cast<rts::EntityId>(node_id), 
        rts::ResourceNode{x, y, amount, amount, 
            static_cast<rts::ResourceNode::Type>(type), false});
    
    auto& nodes = sim.production_manager().resource_nodes();
    REQUIRE(nodes.find(static_cast<rts::EntityId>(node_id)) != nodes.end());
    REQUIRE(nodes[static_cast<rts::EntityId>(node_id)].amount == amount);
}

TEST_CASE("Economy - Extractors", "[economy]") {
    rts::Simulation sim;
    sim.start();
    
    int node_id = 2001;
    sim.production_manager().add_resource_node(static_cast<rts::EntityId>(node_id),
        rts::ResourceNode{100.0f, 100.0f, 1000.0f, 1000.0f, 
            rts::ResourceNode::Type::METAL, false});
    
    int extractor_id = 2002;
    float extraction_rate = 10.0f;
    
    auto& extractors = sim.production_manager().extractors();
    extractors[static_cast<rts::EntityId>(extractor_id)] = rts::Extractor{
        100.0f, 100.0f, static_cast<rts::EntityId>(node_id), extraction_rate, 0.0f, true
    };
    
    REQUIRE(extractors.find(static_cast<rts::EntityId>(extractor_id)) != extractors.end());
    REQUIRE(extractors[static_cast<rts::EntityId>(extractor_id)].extraction_rate == extraction_rate);
}

TEST_CASE("Economy - Storage", "[economy]") {
    rts::Simulation sim;
    sim.start();
    
    int storage_id = 3001;
    float metal_capacity = 5000.0f;
    float energy_capacity = 5000.0f;
    float research_capacity = 1000.0f;
    
    sim.production_manager().add_storage(static_cast<rts::EntityId>(storage_id),
        rts::Storage{100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 
            metal_capacity, energy_capacity, research_capacity});
    
    auto& storages = sim.production_manager().storages();
    REQUIRE(storages.find(static_cast<rts::EntityId>(storage_id)) != storages.end());
    REQUIRE(storages[static_cast<rts::EntityId>(storage_id)].metal_capacity == metal_capacity);
}

TEST_CASE("Economy - Production Lines", "[economy]") {
    rts::Simulation sim;
    sim.start();
    
    int storage_id = 4001;
    sim.production_manager().add_storage(static_cast<rts::EntityId>(storage_id),
        rts::Storage{100.0f, 100.0f, 5000.0f, 5000.0f, 1000.0f, 
            5000.0f, 5000.0f, 1000.0f});
    
    int line_id = 4002;
    sim.production_manager().add_production_line(static_cast<rts::EntityId>(line_id),
        rts::ProductionLine{static_cast<rts::EntityId>(storage_id), std::queue<rts::ConstructionQueueEntry>{}, 
            100.0f, 50.0f, 0, 3});
    
    auto& lines = sim.production_manager().production_lines();
    REQUIRE(lines.find(static_cast<rts::EntityId>(line_id)) != lines.end());
    REQUIRE(lines[static_cast<rts::EntityId>(line_id)].max_jobs == 3);
}

TEST_CASE("Economy - Construction Queue", "[economy]") {
    rts::Simulation sim;
    sim.start();
    
    int storage_id = 5001;
    sim.production_manager().add_storage(static_cast<rts::EntityId>(storage_id),
        rts::Storage{100.0f, 100.0f, 10000.0f, 10000.0f, 2000.0f, 
            10000.0f, 10000.0f, 2000.0f});
    
    int line_id = 5002;
    sim.production_manager().add_production_line(static_cast<rts::EntityId>(line_id),
        rts::ProductionLine{static_cast<rts::EntityId>(storage_id), std::queue<rts::ConstructionQueueEntry>{}, 
            100.0f, 50.0f, 0, 3});
    
    rts::ConstructionQueueEntry entry{};
    entry.entity_id = static_cast<rts::EntityId>(5003);
    entry.type = rts::ConstructionQueueEntry::Type::UNIT;
    entry.build_progress = 0.0f;
    entry.total_cost_metal = 1000.0f;
    entry.total_cost_energy = 500.0f;
    entry.metal_per_tick = 20.0f;
    entry.energy_per_tick = 10.0f;
    entry.completed = false;
    
    sim.production_manager().add_to_queue(static_cast<rts::EntityId>(line_id), entry);
    
    auto& lines = sim.production_manager().production_lines();
    REQUIRE(lines[static_cast<rts::EntityId>(line_id)].queue.size() == 1);
}

TEST_CASE("Economy - Construction Progress", "[economy]") {
    rts::Simulation sim;
    sim.start();
    
    int storage_id = 6001;
    sim.production_manager().add_storage(static_cast<rts::EntityId>(storage_id),
        rts::Storage{100.0f, 100.0f, 10000.0f, 10000.0f, 2000.0f, 
            10000.0f, 10000.0f, 1000.0f});
    
    int line_id = 6002;
    sim.production_manager().add_production_line(static_cast<rts::EntityId>(line_id),
        rts::ProductionLine{static_cast<rts::EntityId>(storage_id), std::queue<rts::ConstructionQueueEntry>{}, 
            100.0f, 50.0f, 0, 3});
    
    rts::ConstructionQueueEntry entry{};
    entry.entity_id = static_cast<rts::EntityId>(6003);
    entry.type = rts::ConstructionQueueEntry::Type::UNIT;
    entry.build_progress = 0.0f;
    entry.total_cost_metal = 100.0f;
    entry.total_cost_energy = 50.0f;
    entry.metal_per_tick = 5.0f;
    entry.energy_per_tick = 2.5f;
    entry.completed = false;
    
    sim.production_manager().add_to_queue(static_cast<rts::EntityId>(line_id), entry);
    
    sim.update_economy(50.0f);
    
    auto& lines = sim.production_manager().production_lines();
    REQUIRE(lines[static_cast<rts::EntityId>(line_id)].queue.size() == 0);
    REQUIRE(entry.completed == true);
}
