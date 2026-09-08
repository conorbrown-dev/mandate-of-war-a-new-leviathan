#include "test_framework.hpp"

#include "ecs/component_manager.hpp"
#include "ecs/components/carrier.hpp"
#include "ecs/components/aircraft.hpp"
#include "logistics/logistics.hpp"
#include "simulation/simulation.hpp"

using namespace rts;

extern "C" {
    int logistics_carrier_deck_occupancy(int facility_id);
    int logistics_takeoff_queue_size(int facility_id);
    int logistics_landing_queue_size(int facility_id);
    int logistics_active_runway_operations(int facility_id);
    bool logistics_is_safe_return(int entity_id);
    int logistics_get_intelligence_age(int entity_id);
    bool logistics_is_intelligence_stale(int entity_id);
    void logistics_add_carrier(int carrier_id, float x, float y, int deck_capacity);
    void logistics_add_aircraft(int aircraft_id, float x, float y, float fuel);
    void logistics_update_all(float delta_ms);
    void simulation_update(float delta_ms);
}

TEST(logistics_visibility_carrier_occupancy) {
    LogisticsManager manager;
    ComponentManager component_manager;
    manager.set_component_manager(&component_manager);
    
    int carrier_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    int deck_capacity = 2;
    
    {
        Carrier carrier{};
        carrier.tier = Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 1;
        carrier.refuel_rate = 100;
        carrier.rearm_rate = 20;
        carrier.max_fuel = 5000.0f;
        carrier.current_fuel = carrier.max_fuel;
        carrier.max_munitions = 1000.0f;
        carrier.current_munitions = carrier.max_munitions;
        carrier.runway_usable = true;
        carrier.deck_capacity = deck_capacity;
        carrier.speed = 0.0f;
        carrier.max_speed = 30.0f;
        manager.add_carrier(carrier_id, carrier);
    }
    
    if (logistics_carrier_deck_occupancy(carrier_id) != 0) {
        throw std::runtime_error("carrier_occupancy_initial_should_be_zero");
    }
    
    {
        Aircraft aircraft{};
        aircraft.x = x;
        aircraft.y = y;
        aircraft.fuel = 100.0f;
        aircraft.status = Aircraft::Status::AIRBORNE;
        manager.add_aircraft(1, x, y, 100.0f);
    }
    
    if (logistics_get_intelligence_age(1) != -1) {
        throw std::runtime_error("intelligence_age_before_update_should_be_minus_one");
    }
}

TEST(logistics_visibility_is_safe_return) {
    LogisticsManager manager;
    ComponentManager component_manager;
    manager.set_component_manager(&component_manager);
    
    int carrier_id = 1;
    int aircraft_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    int deck_capacity = 2;
    
    {
        Carrier carrier{};
        carrier.tier = Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 1;
        carrier.refuel_rate = 100;
        carrier.rearm_rate = 20;
        carrier.max_fuel = 5000.0f;
        carrier.current_fuel = carrier.max_fuel;
        carrier.max_munitions = 1000.0f;
        carrier.current_munitions = carrier.max_munitions;
        carrier.runway_usable = true;
        carrier.deck_capacity = deck_capacity;
        carrier.speed = 0.0f;
        carrier.max_speed = 30.0f;
        manager.add_carrier(carrier_id, carrier);
    }
    
    manager.add_aircraft(aircraft_id, x, y, 50.0f);
    
    Pathfinding pathfinding(32, 32, 10.0f);
    
    if (!manager.is_safe_return(aircraft_id, pathfinding)) {
        throw std::runtime_error("aircraft_at_carrier_should_be_safe_return");
    }
}

TEST(logistics_visibility_intelligence_age) {
    LogisticsManager manager;
    ComponentManager component_manager;
    manager.set_component_manager(&component_manager);
    
    int entity_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    
    manager.update_intelligence(entity_id, x, y, 10);
    
    manager.update_intelligence(entity_id, x, y, 200);
    
    Intelligence* intel = manager.get_intelligence(entity_id);
    
    if (!intel) {
        throw std::runtime_error("intelligence_should_exist_after_update");
    }
    
    int age = intel->last_seen_tick;
    
    if (age < 0 || age > 200) {
        throw std::runtime_error("intelligence_age_should_be_positive_and_reasonable");
    }
}

TEST(logistics_visibility_stale_intelligence) {
    Simulation simulation;
    simulation.start();
    
    simulation.update(50.0f);
    
    LogisticsManager& manager = simulation.logistics_manager();
    
    int entity_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    
    manager.update_intelligence(entity_id, x, y, simulation.simulation_tick());
    
    for (int i = 0; i < 150; ++i) {
        simulation.update(50.0f);
    }
    
    Intelligence* intel = manager.get_intelligence(entity_id);
    
    if (!intel) {
        throw std::runtime_error("intelligence_should_exist");
    }
    
    int age = simulation.simulation_tick() - intel->last_seen_tick;
    
    if (age <= 100) {
        throw std::runtime_error("intelligence_150_ticks_old_should_be_stale");
    }
}

TEST(logistics_visibility_takeoff_queue) {
    LogisticsManager manager;
    ComponentManager component_manager;
    manager.set_component_manager(&component_manager);
    
    int carrier_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    int deck_capacity = 2;
    
    {
        Carrier carrier{};
        carrier.tier = Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 1;
        carrier.refuel_rate = 100;
        carrier.rearm_rate = 20;
        carrier.max_fuel = 5000.0f;
        carrier.current_fuel = carrier.max_fuel;
        carrier.max_munitions = 1000.0f;
        carrier.current_munitions = carrier.max_munitions;
        carrier.runway_usable = true;
        carrier.deck_capacity = deck_capacity;
        carrier.speed = 0.0f;
        carrier.max_speed = 30.0f;
        manager.add_carrier(carrier_id, carrier);
    }
    
    if (logistics_takeoff_queue_size(carrier_id) != 0) {
        throw std::runtime_error("takeoff_queue_should_be_empty_initially");
    }
}

TEST(logistics_visibility_landing_queue) {
    LogisticsManager manager;
    ComponentManager component_manager;
    manager.set_component_manager(&component_manager);
    
    int carrier_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    int deck_capacity = 2;
    
    {
        Carrier carrier{};
        carrier.tier = Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 1;
        carrier.refuel_rate = 100;
        carrier.rearm_rate = 20;
        carrier.max_fuel = 5000.0f;
        carrier.current_fuel = carrier.max_fuel;
        carrier.max_munitions = 1000.0f;
        carrier.current_munitions = carrier.max_munitions;
        carrier.runway_usable = true;
        carrier.deck_capacity = deck_capacity;
        carrier.speed = 0.0f;
        carrier.max_speed = 30.0f;
        manager.add_carrier(carrier_id, carrier);
    }
    
    if (logistics_landing_queue_size(carrier_id) != 0) {
        throw std::runtime_error("landing_queue_should_be_empty_initially");
    }
}

TEST(logistics_visibility_active_runway_operations) {
    LogisticsManager manager;
    ComponentManager component_manager;
    manager.set_component_manager(&component_manager);
    
    int carrier_id = 1;
    float x = 100.0f;
    float y = 100.0f;
    int deck_capacity = 2;
    
    {
        Carrier carrier{};
        carrier.tier = Carrier::Tier::T1_LIGHT;
        carrier.x = x;
        carrier.y = y;
        carrier.runway_capacity = 1;
        carrier.refuel_rate = 100;
        carrier.rearm_rate = 20;
        carrier.max_fuel = 5000.0f;
        carrier.current_fuel = carrier.max_fuel;
        carrier.max_munitions = 1000.0f;
        carrier.current_munitions = carrier.max_munitions;
        carrier.runway_usable = true;
        carrier.deck_capacity = deck_capacity;
        carrier.speed = 0.0f;
        carrier.max_speed = 30.0f;
        manager.add_carrier(carrier_id, carrier);
    }
    
    if (logistics_active_runway_operations(carrier_id) != 0) {
        throw std::runtime_error("active_runway_operations_should_be_zero_initially");
    }
}
