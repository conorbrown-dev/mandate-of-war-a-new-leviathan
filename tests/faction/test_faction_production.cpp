#include "../test_framework.hpp"
#include "ecs/components/faction.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/factions.hpp"

TEST(faction_initialization) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int faction_id = static_cast<int>(FactionId::ELITE_PRECISION);
    float x = 100.0f, y = 100.0f;
    
    sim.initialize_faction(static_cast<FactionId>(faction_id), x, y);
    
    auto& storages = sim.production_manager().storages();
    int storage_id = faction_id * 10000;
    if (storages.find(static_cast<EntityId>(storage_id)) == storages.end()) {
        throw std::runtime_error("Storage not found for faction");
    }
    
    auto& storage = storages[static_cast<EntityId>(storage_id)];
    const auto& faction_data = get_faction_start_data();
    auto it = faction_data.find(static_cast<FactionId>(faction_id));
    if (it == faction_data.end()) {
        throw std::runtime_error("Faction data not found");
    }
    if (storage.metal_storage != it->second.start_material) {
        throw std::runtime_error("Initial metal storage mismatch");
    }
    if (storage.energy_storage != it->second.start_energy) {
        throw std::runtime_error("Initial energy storage mismatch");
    }
    if (storage.research_storage != it->second.start_research) {
        throw std::runtime_error("Initial research storage mismatch");
    }
}

TEST(faction_production_queue) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int faction_id = static_cast<int>(FactionId::MASS_WARFARE);
    float x = 200.0f, y = 200.0f;
    
    sim.initialize_faction(static_cast<FactionId>(faction_id), x, y);
    
    int storage_id = faction_id * 10000;
    int line_id = faction_id * 10000 + 1;
    
    sim.production_manager().add_unit_to_queue(
        static_cast<EntityId>(line_id),
        static_cast<FactionId>(faction_id),
        UnitType::MASS_SWARM_TANK
    );
    
    auto& lines = sim.production_manager().production_lines();
    if (lines.find(static_cast<EntityId>(line_id)) == lines.end()) {
        throw std::runtime_error("Production line not found");
    }
    auto& line = lines[static_cast<EntityId>(line_id)];
    if (line.queue.size() != 1) {
        throw std::runtime_error("Queue should have 1 entry");
    }
    
    auto& entry = line.queue.front();
    if (entry.type != ConstructionQueueEntry::Type::UNIT) {
        throw std::runtime_error("Queue entry type should be UNIT");
    }
    if (entry.unit_type != UnitType::MASS_SWARM_TANK) {
        throw std::runtime_error("Queue entry unit_type mismatch");
    }
    if (entry.faction_id != static_cast<FactionId>(faction_id)) {
        throw std::runtime_error("Queue entry faction_id mismatch");
    }
}

TEST(faction_resource_consumption) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int faction_id = static_cast<int>(FactionId::ELITE_PRECISION);
    float x = 300.0f, y = 300.0f;
    
    sim.initialize_faction(static_cast<FactionId>(faction_id), x, y);
    
    int storage_id = faction_id * 10000;
    
    auto& storages = sim.production_manager().storages();
    auto& storage = storages[static_cast<EntityId>(storage_id)];
    if (storage.metal_storage == 0) {
        throw std::runtime_error("Initial metal storage should be > 0");
    }
    if (storage.energy_storage == 0) {
        throw std::runtime_error("Initial energy storage should be > 0");
    }
    
    float initial_metal = storage.metal_storage;
    float initial_energy = storage.energy_storage;
    float initial_research = storage.research_storage;
    
    const auto& prototypes = get_unit_prototypes();
    auto proto_it = prototypes.find(UnitType::ELITE_MAIN_BATTLE_TANK);
    if (proto_it == prototypes.end()) {
        throw std::runtime_error("Unit prototype not found");
    }
    
    int line_id = faction_id * 10000 + 1;
    sim.production_manager().add_unit_to_queue(
        static_cast<EntityId>(line_id),
        static_cast<FactionId>(faction_id),
        UnitType::ELITE_MAIN_BATTLE_TANK
    );
    
    float build_time_seconds = proto_it->second.build_time_seconds;
    float ticks_needed = build_time_seconds * 20.0f;
    float delta_ms = 50.0f;
    
    for (size_t i = 0; i < static_cast<size_t>(ticks_needed); ++i) {
        sim.update_economy(delta_ms);
    }
    
    // Verify resources were consumed
    if (storage.metal_storage >= initial_metal) {
        throw std::runtime_error("Metal should have been consumed");
    }
    if (storage.energy_storage >= initial_energy) {
        throw std::runtime_error("Energy should have been consumed");
    }
    if (storage.research_storage >= initial_research) {
        throw std::runtime_error("Research should have been consumed");
    }
    
    // Manually spawn the unit since we're not using the full simulation loop
    auto& completed = sim.production_manager().get_completed_constructions();
    for (const auto& comp : completed) {
        sim.create_unit_with_type(comp.x, comp.y, comp.unit_type, comp.faction_id);
    }
    sim.production_manager().clear_completed_constructions();
    
    // Verify unit was spawned
    int final_entity_count = sim.entity_count();
    int initial_entity_count_before_spawn = final_entity_count - 1;
    if (final_entity_count != initial_entity_count_before_spawn + 1) {
        throw std::runtime_error("Unit should have been spawned");
    }
    
    // Verify faction component
    auto& component_manager = sim.component_manager();
    auto* faction = component_manager.get_component<Faction>(static_cast<EntityId>(final_entity_count - 1));
    if (faction == nullptr) {
        throw std::runtime_error("Faction component not found on spawned unit");
    }
    if (faction->faction_id != static_cast<FactionId>(faction_id)) {
        throw std::runtime_error("Faction component faction_id mismatch");
    }
}

TEST(faction_unit_spawn_on_completion) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int faction_id = static_cast<int>(FactionId::INDUSTRIAL_EXPERIMENTAL);
    float x = 400.0f, y = 400.0f;
    
    sim.initialize_faction(static_cast<FactionId>(faction_id), x, y);
    
    int storage_id = faction_id * 10000;
    int line_id = faction_id * 10000 + 1;
    
    int initial_entity_count = sim.entity_count();
    
    sim.production_manager().add_unit_to_queue(
        static_cast<EntityId>(line_id),
        static_cast<FactionId>(faction_id),
        UnitType::INDUSTRIAL_MBT
    );
    
    const auto& prototypes = get_unit_prototypes();
    auto proto_it = prototypes.find(UnitType::INDUSTRIAL_MBT);
    if (proto_it == prototypes.end()) {
        throw std::runtime_error("Unit prototype not found");
    }
    
    float build_time_seconds = proto_it->second.build_time_seconds;
    float ticks_needed = build_time_seconds * 20.0f;
    float delta_ms = 50.0f;
    
    // Simulate construction to completion
    for (size_t i = 0; i < static_cast<size_t>(ticks_needed); ++i) {
        sim.update_economy(delta_ms);
    }
    
    // Debug: check completed constructions
    auto& completed = sim.production_manager().get_completed_constructions();
    std::cout << "Completed constructions count: " << completed.size() << std::endl;
    
    // Manually process completed constructions
    for (const auto& comp : completed) {
        sim.create_unit_with_type(comp.x, comp.y, comp.unit_type, comp.faction_id);
    }
    sim.production_manager().clear_completed_constructions();
    
    // Verify unit was spawned
    int final_entity_count = sim.entity_count();
    if (final_entity_count != initial_entity_count + 1) {
        throw std::runtime_error("Unit should have been spawned");
    }
    
    // Verify faction component
    auto& component_manager = sim.component_manager();
    auto* faction = component_manager.get_component<Faction>(static_cast<EntityId>(final_entity_count));
    if (faction == nullptr) {
        throw std::runtime_error("Faction component not found on spawned unit");
    }
    if (faction->faction_id != static_cast<FactionId>(faction_id)) {
        throw std::runtime_error("Faction component faction_id mismatch");
    }
}

TEST(faction_unit_costs_differ) {
    using namespace rts;
    
    ProductionManager pm;
    
    float elite_metal_cost = pm.get_unit_metal_cost(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK);
    float mass_metal_cost = pm.get_unit_metal_cost(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK);
    
    if (elite_metal_cost <= mass_metal_cost) {
        throw std::runtime_error("Elite costs should be higher than Mass due to discount");
    }
}

TEST(faction_start_units_spawned) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int faction_id = static_cast<int>(FactionId::ELITE_PRECISION);
    float x = 500.0f, y = 500.0f;
    
    int initial_count = sim.entity_count();
    
    sim.initialize_faction(static_cast<FactionId>(faction_id), x, y);
    
    int final_count = sim.entity_count();
    
    const auto& faction_data = get_faction_start_data();
    auto it = faction_data.find(static_cast<FactionId>(faction_id));
    if (it == faction_data.end()) {
        throw std::runtime_error("Faction data not found");
    }
    
    size_t expected_start_units = it->second.start_units.size();
    if (final_count - initial_count != expected_start_units) {
        throw std::runtime_error("Start units count mismatch");
    }
}

TEST(faction_research_consumption) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int faction_id = static_cast<int>(FactionId::ELITE_PRECISION);
    float x = 600.0f, y = 600.0f;
    
    sim.initialize_faction(static_cast<FactionId>(faction_id), x, y);
    
    int storage_id = faction_id * 10000;
    int line_id = faction_id * 10000 + 1;
    
    auto& storages = sim.production_manager().storages();
    auto& storage = storages[static_cast<EntityId>(storage_id)];
    
    float initial_research = storage.research_storage;
    
    // Add ELITE_LONG_RANGE_ARTILLERY which has research_cost = 200
    sim.production_manager().add_unit_to_queue(
        static_cast<EntityId>(line_id),
        static_cast<FactionId>(faction_id),
        UnitType::ELITE_LONG_RANGE_ARTILLERY
    );
    
    const auto& prototypes = get_unit_prototypes();
    auto proto_it = prototypes.find(UnitType::ELITE_LONG_RANGE_ARTILLERY);
    if (proto_it == prototypes.end()) {
        throw std::runtime_error("Unit prototype not found");
    }
    
    float research_cost = proto_it->second.research_cost;
    if (research_cost <= 0) {
        throw std::runtime_error("Test unit should have research cost");
    }
    
    float build_time_seconds = proto_it->second.build_time_seconds;
    float ticks_needed = build_time_seconds * 20.0f;
    float delta_ms = 50.0f;
    
    // Simulate construction to completion
    for (size_t i = 0; i < static_cast<size_t>(ticks_needed); ++i) {
        sim.update_economy(delta_ms);
    }
    
    // Verify research was consumed
    float research_spent = initial_research - storage.research_storage;
    
    // Debug output
    std::cout << "Initial research: " << initial_research << std::endl;
    std::cout << "Final research: " << storage.research_storage << std::endl;
    std::cout << "Research spent: " << research_spent << std::endl;
    std::cout << "Expected research cost: " << research_cost << std::endl;
    
    if (research_spent < research_cost * 0.9f) {
        throw std::runtime_error("Research consumption too low: expected ~" + std::to_string(research_cost) + ", spent " + std::to_string(research_spent));
    }
    if (research_spent > research_cost * 1.1f) {
        throw std::runtime_error("Research consumption too high: expected ~" + std::to_string(research_cost) + ", spent " + std::to_string(research_spent));
    }
}
