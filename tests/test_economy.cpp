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
    
    ::economy_add_storage(1, 100.0f, 100.0f, 5000.0f, 5000.0f, 1000.0f);
    
    float metal_storage, energy_storage, research_storage;
    float metal_capacity, energy_capacity, research_capacity;
    
    bool result = ::economy_get_storage_info(1, 
        &metal_storage, &energy_storage, &research_storage,
        &metal_capacity, &energy_capacity, &research_capacity);
    
    if (!result) {
        throw std::runtime_error("economy_get_storage_info should return true");
    }
    
    if (metal_capacity != 5000.0f) {
        throw std::runtime_error("Metal capacity should be 5000");
    }
    if (energy_capacity != 5000.0f) {
        throw std::runtime_error("Energy capacity should be 5000");
    }
    if (research_capacity != 1000.0f) {
        throw std::runtime_error("Research capacity should be 1000");
    }
    
    sim.stop();
}

TEST(economy_queue_size) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    ::economy_add_storage(1, 100.0f, 100.0f, 5000.0f, 5000.0f, 1000.0f);
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
