#include "test_framework.hpp"
#include "simulation/economy_api.h"

#include "simulation/simulation.hpp"
TEST(economy_resource_node_count) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    ::economy_add_resource_node(1, 100.0f, 100.0f, 1000.0f, 0);
    
    if (::economy_get_resource_node_count() != 1) {
        throw std::runtime_error("Expected 1 resource node");
    }
    
    sim.stop();
}

TEST(economy_storage_info) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    ::economy_add_storage(1, 100.0f, 100.0f, 5000.0f, 5000.0f);
    
    float metal_storage, energy_storage;
    float metal_capacity, energy_capacity;
    
    bool result = ::economy_get_storage_info(1, 
        &metal_storage, &energy_storage,
        &metal_capacity, &energy_capacity);
    
    if (!result) {
        throw std::runtime_error("economy_get_storage_info should return true");
    }
    
    if (metal_capacity != 5000.0f) {
        throw std::runtime_error("Metal capacity should be 5000");
    }
    if (energy_capacity != 5000.0f) {
        throw std::runtime_error("Energy capacity should be 5000");
    }
    
    sim.stop();
}

TEST(economy_tactical_storage_info) {
    using namespace rts;
    Simulation sim;
    sim.start();
    ::economy_add_tactical_storage(9, 10.0f, 20.0f, 500.0f, 300.0f, 100.0f, 70.0f);
    float materials, materials_capacity, fuel, fuel_capacity, generation, demand, reserve;
    if (!::economy_get_tactical_resources(9, &materials, &materials_capacity, &fuel, &fuel_capacity,
            &generation, &demand, &reserve)) throw std::runtime_error("Expected tactical storage info");
    if (materials != 0.0f || materials_capacity != 500.0f || fuel != 0.0f || fuel_capacity != 300.0f)
        throw std::runtime_error("Tactical storage capacity state mismatch");
    if (generation != 100.0f || demand != 70.0f || reserve != 30.0f)
        throw std::runtime_error("Tactical power snapshot mismatch");
    sim.stop();
}

TEST(economy_queue_size) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    ::economy_add_storage(1, 100.0f, 100.0f, 5000.0f, 5000.0f);
    ::economy_add_production_line(1, 1, 10.0f, 10.0f, 2);
    
    int queue_size = ::economy_get_queue_size(1);
    if (queue_size != 0) {
        throw std::runtime_error("Queue should be empty initially");
    }
    
    sim.stop();
}

TEST(economy_get_completed_build_count) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    int completed = ::economy_get_completed_build_count();
    if (completed < 0) {
        throw std::runtime_error("Completed build count should be non-negative");
    }
    
    sim.stop();
}
