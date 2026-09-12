#include "test_framework.hpp"
#include <cmath>
#include "ecs/entity.hpp"
#include "ecs/component_manager.hpp"
#include "ecs/components/airbase.hpp"
#include "ecs/components/aircraft.hpp"
#include "ecs/components/carrier.hpp"
#include "ecs/components/naval_vessel.hpp"
#include "ecs/components/intelligence.hpp"
#include "ecs/components/recovery_facility.hpp"
#include "ecs/components/recovery.hpp"
#include "logistics/logistics.hpp"
#include "simulation/simulation.hpp"

using namespace rts;
using namespace rts::test;

TEST(airbase_component_registration) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId airbase_id = 1;
    
    Airbase airbase{};
    airbase.x = 100.0f;
    airbase.y = 200.0f;
    airbase.runway_capacity = 10;
    component_manager.add_component<Airbase>(airbase_id, airbase);
    
    auto* retrieved = component_manager.get_component<Airbase>(airbase_id);
    if (!retrieved || retrieved->x != 100.0f || retrieved->runway_capacity != 10)
        throw std::runtime_error("airbase_component_registration");
}

TEST(aircraft_component_registration) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId aircraft_id = 2;
    
    Aircraft aircraft{};
    aircraft.x = 150.0f;
    aircraft.y = 250.0f;
    aircraft.fuel = 80.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    component_manager.add_component<Aircraft>(aircraft_id, aircraft);
    
    auto* retrieved = component_manager.get_component<Aircraft>(aircraft_id);
    if (!retrieved || retrieved->fuel != 80.0f)
        throw std::runtime_error("aircraft_component_registration");
}

TEST(carrier_component_registration) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId carrier_id = 3;
    
    Carrier carrier{};
    carrier.x = 300.0f;
    carrier.y = 400.0f;
    carrier.deck_capacity = 20;
    component_manager.add_component<Carrier>(carrier_id, carrier);
    
    auto* retrieved = component_manager.get_component<Carrier>(carrier_id);
    if (!retrieved || retrieved->deck_capacity != 20)
        throw std::runtime_error("carrier_component_registration");
}

TEST(naval_vessel_component_registration) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId naval_vessel_id = 4;
    
    NavalVessel vessel{};
    vessel.x = 350.0f;
    vessel.y = 450.0f;
    vessel.fuel = 400.0f;
    component_manager.add_component<NavalVessel>(naval_vessel_id, vessel);
    
    auto* retrieved = component_manager.get_component<NavalVessel>(naval_vessel_id);
    if (!retrieved || retrieved->fuel != 400.0f)
        throw std::runtime_error("naval_vessel_component_registration");
}

TEST(intelligence_component_registration) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId intelligence_id = 5;
    
    Intelligence intel;
    intel.entity_id = intelligence_id;
    intel.last_x = 500.0f;
    intel.last_y = 600.0f;
    intel.last_seen_tick = 100;
    intel.freshness = 1.0f;
    intel.confidence = Intelligence::Confidence::HIGH;
    component_manager.add_component<Intelligence>(intelligence_id, intel);
    
    auto* retrieved = component_manager.get_component<Intelligence>(intelligence_id);
    if (!retrieved || retrieved->confidence != Intelligence::Confidence::HIGH)
        throw std::runtime_error("intelligence_component_registration");
    
    component_manager.remove_component<Intelligence>(intelligence_id);
    if (component_manager.get_component<Intelligence>(intelligence_id))
        throw std::runtime_error("intelligence_component_removal");
}

TEST(recovery_facility_component_registration) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId recovery_id = 6;
    
    RecoveryFacility facility;
    facility.x = 700.0f;
    facility.y = 800.0f;
    facility.max_recovery_distance = 150.0f;
    facility.type = RecoveryFacility::Type::NAVAL_BASE;
    component_manager.add_component<RecoveryFacility>(recovery_id, facility);
    
    auto* retrieved = component_manager.get_component<RecoveryFacility>(recovery_id);
    if (!retrieved || retrieved->type != RecoveryFacility::Type::NAVAL_BASE)
        throw std::runtime_error("recovery_facility_component_registration");
}

TEST(logistics_entities_with_filter) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    component_manager.add_component<Aircraft>(1, Aircraft{});
    component_manager.add_component<Aircraft>(2, Aircraft{});
    component_manager.add_component<NavalVessel>(3, NavalVessel{});
    
    auto aircraft = component_manager.entities_with<Aircraft>();
    auto naval = component_manager.entities_with<NavalVessel>();
    
    if (aircraft.size() != 2) throw std::runtime_error("entities_with_filter_aircraft");
    if (naval.size() != 1) throw std::runtime_error("entities_with_filter_naval");
}

TEST(aircraft_endurance_update) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId aircraft_id = 2;
    EntityId airbase_id = 1;
    
    Aircraft aircraft{};
    aircraft.fuel = 50.0f;
    aircraft.fuel_consumption_rate = 0.5f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    component_manager.add_component<Aircraft>(aircraft_id, aircraft);
    component_manager.add_component<Airbase>(airbase_id, Airbase{});
    component_manager.add_component<Recovered>(airbase_id, Recovered{});
    
    auto* aircraft_ptr = component_manager.get_component<Aircraft>(aircraft_id);
    
    manager.update_aircraft_endurance(aircraft_id, 1000.0f);
    
    aircraft_ptr = component_manager.get_component<Aircraft>(aircraft_id);
    if (aircraft_ptr->fuel != 49.5f)
        throw std::runtime_error("aircraft_endurance_update");
}

TEST(aircraft_endurance_crash) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId aircraft_id = 2;
    EntityId airbase_id = 1;
    
    Aircraft aircraft{};
    aircraft.fuel = 0.0f;
    aircraft.fuel_consumption_rate = 1.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    component_manager.add_component<Aircraft>(aircraft_id, aircraft);
    component_manager.add_component<Airbase>(airbase_id, Airbase{});
    component_manager.add_component<Recovered>(airbase_id, Recovered{});
    
    auto* aircraft_ptr = component_manager.get_component<Aircraft>(aircraft_id);
    
    manager.update_aircraft_endurance(aircraft_id, 1000.0f);
    
    aircraft_ptr = component_manager.get_component<Aircraft>(aircraft_id);
    if (aircraft_ptr->status != Aircraft::Status::CRASHED)
        throw std::runtime_error("aircraft_endurance_crash");
}

TEST(naval_vessel_endurance_stranded) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId naval_vessel_id = 4;
    EntityId recovery_id = 6;
    
    NavalVessel vessel{};
    vessel.fuel = 1.0f;
    vessel.fuel_consumption_rate = 2.0f;
    component_manager.add_component<NavalVessel>(naval_vessel_id, vessel);
    component_manager.add_component<Recovered>(recovery_id, Recovered{});
    
    auto* vessel_ptr = component_manager.get_component<NavalVessel>(naval_vessel_id);
    
    manager.update_naval_endurance(naval_vessel_id, 1000.0f);
    
    vessel_ptr = component_manager.get_component<NavalVessel>(naval_vessel_id);
    if (!vessel_ptr->is_stranded)
        throw std::runtime_error("naval_vessel_endurance_stranded");
}

TEST(naval_vessel_resupply) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId naval_vessel_id = 4;
    EntityId recovery_id = 6;
    EntityId carrier_id = 3;
    
    NavalVessel vessel{};
    vessel.fuel = 100.0f;
    vessel.max_fuel = 500.0f;
    component_manager.add_component<NavalVessel>(naval_vessel_id, vessel);
    component_manager.add_component<Recovered>(recovery_id, Recovered{});
    component_manager.add_component<Carrier>(carrier_id, Carrier{});
    
    auto* vessel_ptr = component_manager.get_component<NavalVessel>(naval_vessel_id);
    
    manager.resupply_naval_vessel(naval_vessel_id, 100.0f);
    
    vessel_ptr = component_manager.get_component<NavalVessel>(naval_vessel_id);
    if (vessel_ptr->fuel != 200.0f)
        throw std::runtime_error("naval_vessel_resupply");
}

TEST(intelligence_decay) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId intelligence_id = 5;
    
    component_manager.add_component<Intelligence>(intelligence_id, Intelligence{});
    auto* intel = component_manager.get_component<Intelligence>(intelligence_id);
    intel->last_seen_tick = 100;
    
    manager.update_intelligence(intelligence_id, 500.0f, 600.0f, 110);
    
    intel = component_manager.get_component<Intelligence>(intelligence_id);
    if (intel->last_seen_tick != 110)
        throw std::runtime_error("intelligence_decay");
}

TEST(multiple_facility_types) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId recovery_id = 6;
    
    RecoveryFacility nav_base;
    nav_base.type = RecoveryFacility::Type::NAVAL_BASE;
    RecoveryFacility air_base;
    air_base.type = RecoveryFacility::Type::AIRBASE;
    
    component_manager.add_component<RecoveryFacility>(recovery_id, nav_base);
    component_manager.add_component<RecoveryFacility>(100, air_base);
    
    auto facilities = component_manager.entities_with<RecoveryFacility>();
    if (facilities.size() != 2)
        throw std::runtime_error("multiple_facility_types");
    
    auto* retrieved = component_manager.get_component<RecoveryFacility>(recovery_id);
    if (retrieved->type != RecoveryFacility::Type::NAVAL_BASE)
        throw std::runtime_error("multiple_facility_types_nav");
    
    retrieved = component_manager.get_component<RecoveryFacility>(100);
    if (retrieved->type != RecoveryFacility::Type::AIRBASE)
        throw std::runtime_error("multiple_facility_types_air");
}

TEST(component_removal) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId aircraft_id = 2;
    
    component_manager.add_component<Aircraft>(aircraft_id, Aircraft{});
    
    auto before = component_manager.entities_with<Aircraft>();
    component_manager.remove_component<Aircraft>(aircraft_id);
    auto after = component_manager.entities_with<Aircraft>();
    
    if (before.size() != 1 || after.size() != 0)
        throw std::runtime_error("component_removal");
}

TEST(component_mask_update_on_removal) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    
    EntityId aircraft_id = 2;
    
    component_manager.add_component<Aircraft>(aircraft_id, Aircraft{});
    component_manager.add_component<Recovered>(1, Recovered{});
    
    auto mask = component_manager.get_mask(aircraft_id);
    component_manager.remove_component<Aircraft>(aircraft_id);
    
    mask = component_manager.get_mask(aircraft_id);
    if (mask != 0)
        throw std::runtime_error("component_mask_update_on_removal");
}

TEST(fixed_wing_airbase_lifecycle) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);

    constexpr EntityId airbase_id = 20;
    constexpr EntityId aircraft_id = 21;

    Airbase airbase{};
    airbase.x = 100.0f;
    airbase.y = 200.0f;
    airbase.runway_capacity = 1;
    airbase.refuel_rate = 100;
    airbase.rearm_rate = 20;
    airbase.max_fuel = 1000.0f;
    airbase.current_fuel = 500.0f;
    airbase.max_munitions = 100.0f;
    airbase.current_munitions = 50.0f;
    airbase.runway_usable = false;
    manager.add_airbase(airbase_id, airbase);

    Aircraft aircraft{};
    aircraft.x = airbase.x;
    aircraft.y = airbase.y;
    aircraft.fuel = 100.0f;
    aircraft.max_fuel = 100.0f;
    aircraft.fuel_consumption_rate = 1.0f;
    aircraft.material = 10.0f;
    aircraft.max_material = 10.0f;
    aircraft.ammunition = 10.0f;
    aircraft.max_ammunition = 10.0f;
    aircraft.status = Aircraft::Status::ON_GROUND;
    aircraft.mission = Aircraft::Mission::REFUEL;
    aircraft.type = Aircraft::Type::CONVENTIONAL;
    aircraft.target_base = INVALID_ENTITY;
    aircraft.queue_slot = -1;
    component_manager.add_component<Aircraft>(aircraft_id, aircraft);

    if (!manager.queue_aircraft_for_takeoff(aircraft_id, airbase_id))
        throw std::runtime_error("fixed_wing_takeoff_queue_rejected");
    manager.update_all(1000.0f);
    auto* aircraft_ptr = component_manager.get_component<Aircraft>(aircraft_id);
    if (!aircraft_ptr || aircraft_ptr->status != Aircraft::Status::QUEUED_FOR_TAKEOFF ||
        manager.takeoff_queue_size(airbase_id) != 1 || manager.active_runway_operations(airbase_id) != 0 ||
        aircraft_ptr->fuel != 100.0f)
        throw std::runtime_error("unusable_runway_did_not_hold_takeoff");

    if (!manager.set_airbase_runway_usable(airbase_id, true))
        throw std::runtime_error("runway_repair_update_failed");
    manager.update_all(50.0f);
    if (aircraft_ptr->status != Aircraft::Status::TAKING_OFF ||
        manager.active_runway_operations(airbase_id) != 1)
        throw std::runtime_error("takeoff_did_not_start");
    manager.update_all(1000.0f);
    if (aircraft_ptr->status != Aircraft::Status::AIRBORNE ||
        aircraft_ptr->mission != Aircraft::Mission::PATROL)
        throw std::runtime_error("takeoff_did_not_reach_mission_state");

    aircraft_ptr->fuel = 20.0f;
    aircraft_ptr->material = 0.0f;
    aircraft_ptr->ammunition = 0.0f;
    if (!manager.order_aircraft_return(aircraft_id, airbase_id) ||
        aircraft_ptr->status != Aircraft::Status::RETURNING)
        throw std::runtime_error("aircraft_return_order_failed");

    aircraft_ptr->x = 1000.0f;
    aircraft_ptr->y = 1000.0f;
    if (manager.queue_aircraft_for_landing(aircraft_id, airbase_id))
        throw std::runtime_error("out_of_range_aircraft_entered_landing_queue");
    aircraft_ptr->x = 110.0f;
    aircraft_ptr->y = 200.0f;
    if (!manager.queue_aircraft_for_landing(aircraft_id, airbase_id) ||
        aircraft_ptr->status != Aircraft::Status::QUEUED_FOR_LANDING)
        throw std::runtime_error("aircraft_landing_queue_failed");

    manager.update_all(50.0f);
    if (aircraft_ptr->status != Aircraft::Status::LANDING)
        throw std::runtime_error("landing_did_not_start");
    manager.update_all(1000.0f);
    if (aircraft_ptr->status != Aircraft::Status::RECOVERING ||
        aircraft_ptr->x != airbase.x || aircraft_ptr->y != airbase.y)
        throw std::runtime_error("landing_did_not_enter_recovery");

    const float fuel_before_service = aircraft_ptr->fuel;
    manager.update_all(1000.0f);
    auto* airbase_ptr = component_manager.get_component<Airbase>(airbase_id);
    if (aircraft_ptr->status != Aircraft::Status::ON_GROUND ||
        aircraft_ptr->fuel != aircraft_ptr->max_fuel ||
        aircraft_ptr->material != aircraft_ptr->max_material ||
        aircraft_ptr->ammunition != aircraft_ptr->max_ammunition ||
        std::abs(airbase_ptr->current_fuel - (500.0f - (100.0f - fuel_before_service))) > 0.001f ||
        airbase_ptr->current_munitions != 30.0f)
        throw std::runtime_error("aircraft_refuel_rearm_failed");
}

TEST(airbase_runway_capacity_is_fifo) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);

    constexpr EntityId airbase_id = 30;
    Airbase airbase{};
    airbase.runway_capacity = 1;
    airbase.runway_usable = true;
    manager.add_airbase(airbase_id, airbase);

    for (EntityId aircraft_id : {EntityId{31}, EntityId{32}}) {
        Aircraft aircraft{};
        aircraft.fuel = 100.0f;
        aircraft.max_fuel = 100.0f;
        aircraft.fuel_consumption_rate = 0.0f;
        aircraft.type = Aircraft::Type::CONVENTIONAL;
        aircraft.status = Aircraft::Status::ON_GROUND;
        aircraft.target_base = INVALID_ENTITY;
        aircraft.queue_slot = -1;
        component_manager.add_component<Aircraft>(aircraft_id, aircraft);
        if (!manager.queue_aircraft_for_takeoff(aircraft_id, airbase_id))
            throw std::runtime_error("fifo_aircraft_queue_failed");
    }

    manager.update_all(50.0f);
    auto* first = component_manager.get_component<Aircraft>(31);
    auto* second = component_manager.get_component<Aircraft>(32);
    if (first->status != Aircraft::Status::TAKING_OFF ||
        second->status != Aircraft::Status::QUEUED_FOR_TAKEOFF || second->queue_slot != 0)
        throw std::runtime_error("runway_queue_not_fifo");

    manager.update_all(1000.0f);
    if (first->status != Aircraft::Status::AIRBORNE ||
        second->status != Aircraft::Status::TAKING_OFF ||
        manager.active_runway_operations(airbase_id) != 1)
        throw std::runtime_error("runway_capacity_not_enforced");
}

TEST(safe_return_tracks_multi_resource_endurance_and_cache) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);
    Pathfinding pathfinding(32, 32, 10.0f);

    Airbase airbase{};
    airbase.x = 300.0f;
    airbase.runway_usable = true;
    manager.add_airbase(40, airbase);

    Carrier carrier{};
    carrier.x = 100.0f;
    carrier.runway_capacity = 1;
    carrier.runway_usable = true;
    carrier.deck_capacity = 1;
    manager.add_carrier(41, carrier);

    Aircraft aircraft{};
    aircraft.x = 0.0f;
    aircraft.y = 0.0f;
    aircraft.fuel = 10.0f;
    aircraft.max_fuel = 10.0f;
    aircraft.fuel_consumption_rate = 2.0f;
    aircraft.material = 2.0f;
    aircraft.max_material = 2.0f;
    aircraft.material_consumption_rate = 0.5f;
    aircraft.cruise_speed = 100.0f;
    aircraft.max_airborne_time_ms = 5000.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    aircraft.type = Aircraft::Type::CONVENTIONAL;
    component_manager.add_component<Aircraft>(42, aircraft);

    pathfinding.reset_flow_field_generation_count();
    const SafeReturnEstimate initial = manager.estimate_safe_return(42, pathfinding);
    auto* aircraft_ptr = component_manager.get_component<Aircraft>(42);
    if (initial.facility_id != 41 || !initial.safe ||
        std::abs(initial.distance - 125.0f) > 0.001f ||
        std::abs(initial.energy_cost - 2.5f) > 0.001f ||
        std::abs(initial.material_cost - 0.625f) > 0.001f ||
        std::abs(initial.flight_time_ms - 1250.0f) > 0.001f ||
        aircraft_ptr->closest_recovery_facility != 41 || !aircraft_ptr->safe_return ||
        pathfinding.flow_field_generation_count() != 0 || manager.safe_return_cache_misses() != 1)
        throw std::runtime_error("initial_safe_return_estimate_incorrect");

    aircraft_ptr->fuel = 2.0f;
    const SafeReturnEstimate cached_unsafe = manager.estimate_safe_return(42, pathfinding);
    if (cached_unsafe.safe || aircraft_ptr->safe_return || manager.safe_return_cache_hits() != 1)
        throw std::runtime_error("cached_safe_return_did_not_recheck_reserves");

    aircraft_ptr->fuel = 10.0f;
    aircraft_ptr->x = 10.0f;
    const SafeReturnEstimate nearby_cached = manager.estimate_safe_return(42, pathfinding);
    if (!nearby_cached.safe || manager.safe_return_cache_hits() != 2)
        throw std::runtime_error("nearby_safe_return_cache_missed");

    if (!manager.update_recovery_facility_position(41, 500.0f, 0.0f))
        throw std::runtime_error("moving_recovery_facility_update_failed");
    const SafeReturnEstimate after_move = manager.estimate_safe_return(42, pathfinding);
    if (after_move.facility_id != 40 || manager.safe_return_cache_misses() != 2)
        throw std::runtime_error("facility_move_did_not_invalidate_safe_return_cache");

    const size_t lookup_hits_before = manager.facility_lookup_cache_hits();
    if (manager.find_nearest_recovery_facility(11.0f, 0.0f, RecoveryFacility::Type::AIRBASE) != 40 ||
        manager.facility_lookup_cache_hits() != lookup_hits_before + 1)
        throw std::runtime_error("facility_sector_lookup_cache_missed");
}

TEST(simulation_updates_aircraft_endurance_and_safe_return_diagnostics) {
    Simulation simulation;
    simulation.start();

    Airbase airbase{};
    airbase.x = 100.0f;
    airbase.y = 0.0f;
    airbase.runway_usable = true;
    simulation.logistics_manager().add_airbase(50, airbase);

    const Entity aircraft_entity = simulation.create_unit(0.0f, 0.0f);
    Aircraft aircraft{};
    aircraft.x = 0.0f;
    aircraft.y = 0.0f;
    aircraft.fuel = 20.0f;
    aircraft.max_fuel = 20.0f;
    aircraft.fuel_consumption_rate = 2.0f;
    aircraft.material = 10.0f;
    aircraft.max_material = 10.0f;
    aircraft.material_consumption_rate = 1.0f;
    aircraft.cruise_speed = 100.0f;
    aircraft.max_airborne_time_ms = 10000.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    aircraft.type = Aircraft::Type::CONVENTIONAL;
    simulation.component_manager().add_component<Aircraft>(aircraft_entity.id, aircraft);

    simulation.update(1000.0f);
    auto* aircraft_ptr = simulation.component_manager().get_component<Aircraft>(aircraft_entity.id);
    if (!aircraft_ptr || std::abs(aircraft_ptr->fuel - 19.5f) > 0.001f ||
        std::abs(aircraft_ptr->material - 9.75f) > 0.001f ||
        std::abs(aircraft_ptr->airborne_time_ms - 250.0f) > 0.001f ||
        aircraft_ptr->closest_recovery_facility != 50 || !aircraft_ptr->safe_return ||
        simulation.logistics_manager().safe_return_cache_hits() == 0)
        throw std::runtime_error("simulation_aircraft_range_update_failed");

    const float airborne_fuel = aircraft_ptr->fuel;
    const float airborne_material = aircraft_ptr->material;
    const float airborne_time = aircraft_ptr->airborne_time_ms;
    aircraft_ptr->status = Aircraft::Status::ON_GROUND;
    simulation.update(50.0f);
    if (aircraft_ptr->fuel != airborne_fuel || aircraft_ptr->material != airborne_material ||
        aircraft_ptr->airborne_time_ms != airborne_time)
        throw std::runtime_error("grounded_aircraft_consumed_endurance");
}

TEST(simulation_movement_keeps_carrier_recovery_position_in_sync) {
    Simulation simulation;
    simulation.start();

    const Entity carrier_entity = simulation.create_unit(0.0f, 0.0f);
    Carrier carrier{};
    carrier.x = 0.0f;
    carrier.y = 0.0f;
    carrier.runway_capacity = 1;
    carrier.runway_usable = true;
    carrier.deck_capacity = 2;
    simulation.logistics_manager().add_carrier(carrier_entity.id, carrier);

    simulation.move_unit(carrier_entity.id, 100.0f, 0.0f);
    simulation.update(50.0f);

    const auto* position = simulation.component_manager().get_component<Position>(carrier_entity.id);
    const auto* facility = simulation.component_manager().get_component<RecoveryFacility>(carrier_entity.id);
    if (!position || !facility || position->x <= 0.0f ||
        std::abs(position->x - facility->x) > 0.0001f ||
        std::abs(position->y - facility->y) > 0.0001f) {
        throw std::runtime_error("moving_carrier_recovery_position_desynchronized");
    }
}

TEST(unusable_runway_exhaustion_removes_aircraft_and_cleans_queue) {
    Simulation simulation;
    simulation.start();

    constexpr EntityId airbase_id = 60;
    Airbase airbase{};
    airbase.x = 0.0f;
    airbase.y = 0.0f;
    airbase.runway_capacity = 1;
    airbase.runway_usable = true;
    simulation.logistics_manager().add_airbase(airbase_id, airbase);

    const Entity aircraft_entity = simulation.create_unit(0.0f, 0.0f);
    Aircraft aircraft{};
    aircraft.x = 0.0f;
    aircraft.y = 0.0f;
    aircraft.fuel = 0.05f;
    aircraft.max_fuel = 1.0f;
    aircraft.fuel_consumption_rate = 2.0f;
    aircraft.material = 1.0f;
    aircraft.max_material = 1.0f;
    aircraft.cruise_speed = 100.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    aircraft.type = Aircraft::Type::CONVENTIONAL;
    aircraft.target_base = INVALID_ENTITY;
    aircraft.queue_slot = -1;
    simulation.component_manager().add_component<Aircraft>(aircraft_entity.id, aircraft);

    if (!simulation.logistics_manager().order_aircraft_return(aircraft_entity.id, airbase_id) ||
        !simulation.logistics_manager().queue_aircraft_for_landing(aircraft_entity.id, airbase_id) ||
        simulation.logistics_manager().landing_queue_size(airbase_id) != 1)
        throw std::runtime_error("crash_fixture_landing_queue_failed");
    if (!simulation.logistics_manager().set_airbase_runway_usable(airbase_id, false))
        throw std::runtime_error("crash_fixture_runway_disable_failed");

    simulation.update(50.0f);
    if (simulation.entity_count() != 0 ||
        simulation.component_manager().get_component<Aircraft>(aircraft_entity.id) != nullptr ||
        simulation.logistics_manager().landing_queue_size(airbase_id) != 0 ||
        simulation.logistics_manager().active_runway_operations(airbase_id) != 0 ||
        simulation.logistics_manager().crashed_aircraft_count() != 1)
        throw std::runtime_error("crashed_aircraft_was_not_removed_cleanly");
}

TEST(aircraft_without_recovery_crashes_when_time_expires) {
    Simulation simulation;
    simulation.start();

    const Entity aircraft_entity = simulation.create_unit(0.0f, 0.0f);
    Aircraft aircraft{};
    aircraft.fuel = 100.0f;
    aircraft.max_fuel = 100.0f;
    aircraft.material = 100.0f;
    aircraft.max_material = 100.0f;
    aircraft.cruise_speed = 100.0f;
    aircraft.max_airborne_time_ms = 100.0f;
    aircraft.status = Aircraft::Status::AIRBORNE;
    aircraft.type = Aircraft::Type::CONVENTIONAL;
    simulation.component_manager().add_component<Aircraft>(aircraft_entity.id, aircraft);

    simulation.update(50.0f);
    auto* aircraft_ptr = simulation.component_manager().get_component<Aircraft>(aircraft_entity.id);
    if (!aircraft_ptr || aircraft_ptr->safe_return ||
        aircraft_ptr->closest_recovery_facility != INVALID_ENTITY)
        throw std::runtime_error("unreachable_aircraft_was_not_marked_unsafe");

    simulation.update(50.0f);
    if (simulation.entity_count() != 0 ||
        simulation.component_manager().get_component<Aircraft>(aircraft_entity.id) != nullptr ||
        simulation.logistics_manager().crashed_aircraft_count() != 1)
        throw std::runtime_error("time_exhausted_aircraft_was_not_destroyed");
}

TEST(vtol_prototype_is_runway_independent_with_data_driven_tradeoffs) {
    const auto& prototypes = get_unit_prototypes();
    const auto fighter_it = prototypes.find(UnitType::ELITE_T1_FIGHTER);
    const auto vtol_it = prototypes.find(UnitType::ELITE_T1_VTOL);
    if (fighter_it == prototypes.end() || vtol_it == prototypes.end())
        throw std::runtime_error("air_prototypes_missing");

    const UnitPrototype& fighter = fighter_it->second;
    const UnitPrototype& vtol = vtol_it->second;
    if (!fighter.is_aircraft || !fighter.requires_runway || !vtol.is_aircraft || vtol.requires_runway ||
        vtol.operational_range >= fighter.operational_range || vtol.payload >= fighter.payload ||
        vtol.energy_cost <= fighter.energy_cost || vtol.research_cost <= fighter.research_cost ||
        vtol.energy_consumption_rate <= fighter.energy_consumption_rate ||
        vtol.build_time_seconds <= fighter.build_time_seconds)
        throw std::runtime_error("vtol_tradeoffs_not_preserved");

    ProductionManager production;
    FactionResearch research{};
    research.completed_projects["vtol_flight_systems"] = false;
    production.set_faction_research(FactionId::ELITE_PRECISION, research);
    if (production.can_produce_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_T1_VTOL))
        throw std::runtime_error("vtol_produced_without_research");
    research.completed_projects["vtol_flight_systems"] = true;
    production.set_faction_research(FactionId::ELITE_PRECISION, research);
    if (!production.can_produce_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_T1_VTOL))
        throw std::runtime_error("vtol_research_did_not_unlock_production");

    Simulation simulation;
    simulation.start();
    const int fighter_id = simulation.create_unit_with_type(
        0.0f,
        0.0f,
        UnitType::ELITE_T1_FIGHTER,
        FactionId::ELITE_PRECISION
    );
    const int vtol_id = simulation.create_unit_with_type(
        20.0f,
        0.0f,
        UnitType::ELITE_T1_VTOL,
        FactionId::ELITE_PRECISION
    );
    auto* fighter_aircraft = simulation.component_manager().get_component<Aircraft>(fighter_id);
    auto* vtol_aircraft = simulation.component_manager().get_component<Aircraft>(vtol_id);
    if (!fighter_aircraft || !vtol_aircraft ||
        fighter_aircraft->type != Aircraft::Type::CONVENTIONAL ||
        vtol_aircraft->type != Aircraft::Type::VTOL ||
        vtol_aircraft->range != vtol.operational_range ||
        vtol_aircraft->max_ammunition != vtol.payload)
        throw std::runtime_error("air_prototype_spawn_mapping_failed");

    if (simulation.logistics_manager().launch_vtol(fighter_id) ||
        !simulation.logistics_manager().launch_vtol(vtol_id) ||
        vtol_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("vtol_runway_independent_launch_failed");
    const SafeReturnEstimate estimate = simulation.logistics_manager().estimate_safe_return(
        vtol_id,
        simulation.pathfinding()
    );
    if (!estimate.safe || estimate.facility_id != INVALID_ENTITY ||
        !simulation.logistics_manager().land_vtol(vtol_id) ||
        vtol_aircraft->status != Aircraft::Status::ON_GROUND)
        throw std::runtime_error("vtol_runway_independent_recovery_failed");
}

TEST(t1_carrier_is_a_bounded_mobile_airbase) {
    ComponentManager component_manager;
    LogisticsManager manager;
    manager.set_component_manager(&component_manager);

    constexpr EntityId carrier_id = 70;
    constexpr EntityId first_aircraft_id = 71;
    constexpr EntityId second_aircraft_id = 72;

    Carrier carrier{};
    carrier.tier = Carrier::Tier::T1_LIGHT;
    carrier.x = 100.0f;
    carrier.y = 200.0f;
    carrier.runway_capacity = 1;
    carrier.refuel_rate = 100;
    carrier.rearm_rate = 20;
    carrier.max_fuel = 1000.0f;
    carrier.current_fuel = 500.0f;
    carrier.max_munitions = 100.0f;
    carrier.current_munitions = 50.0f;
    carrier.runway_usable = true;
    carrier.deck_capacity = 1;
    carrier.max_speed = 30.0f;
    manager.add_carrier(carrier_id, carrier);

    Aircraft first{};
    first.x = carrier.x;
    first.y = carrier.y;
    first.fuel = 100.0f;
    first.max_fuel = 100.0f;
    first.fuel_consumption_rate = 1.0f;
    first.material = 10.0f;
    first.max_material = 10.0f;
    first.ammunition = 10.0f;
    first.max_ammunition = 10.0f;
    first.cruise_speed = 100.0f;
    first.max_airborne_time_ms = 10000.0f;
    first.status = Aircraft::Status::ON_GROUND;
    first.mission = Aircraft::Mission::REFUEL;
    first.type = Aircraft::Type::CONVENTIONAL;
    first.target_base = INVALID_ENTITY;
    first.queue_slot = -1;
    component_manager.add_component<Aircraft>(first_aircraft_id, first);
    component_manager.add_component<Position>(first_aircraft_id, {first.x, first.y, 0.0f});

    Aircraft second = first;
    second.status = Aircraft::Status::AIRBORNE;
    component_manager.add_component<Aircraft>(second_aircraft_id, second);

    if (!manager.embark_aircraft(first_aircraft_id, carrier_id) ||
        manager.embark_aircraft(first_aircraft_id, carrier_id) ||
        manager.carrier_deck_occupancy(carrier_id) != 1)
        throw std::runtime_error("carrier_initial_storage_failed");
    auto* carrier_ptr = component_manager.get_component<Carrier>(carrier_id);
    if (!carrier_ptr || carrier_ptr->tier != Carrier::Tier::T1_LIGHT ||
        carrier_ptr->deck_occupancy != 1)
        throw std::runtime_error("carrier_t1_capacity_diagnostics_failed");

    second.status = Aircraft::Status::ON_GROUND;
    component_manager.add_component<Aircraft>(second_aircraft_id, second);
    if (manager.embark_aircraft(second_aircraft_id, carrier_id))
        throw std::runtime_error("carrier_storage_capacity_exceeded");

    if (!manager.update_recovery_facility_position(carrier_id, 120.0f, 220.0f))
        throw std::runtime_error("carrier_move_failed");
    auto* first_ptr = component_manager.get_component<Aircraft>(first_aircraft_id);
    auto* first_position = component_manager.get_component<Position>(first_aircraft_id);
    if (!first_ptr || !first_position || first_ptr->x != 120.0f || first_ptr->y != 220.0f ||
        first_position->x != 120.0f || first_position->y != 220.0f)
        throw std::runtime_error("embarked_aircraft_did_not_follow_carrier");

    if (!manager.queue_aircraft_for_takeoff(first_aircraft_id, carrier_id) ||
        manager.takeoff_queue_size(carrier_id) != 1 || carrier_ptr->launch_queue_size != 1)
        throw std::runtime_error("carrier_launch_queue_failed");
    manager.update_all(50.0f);
    if (first_ptr->status != Aircraft::Status::TAKING_OFF ||
        manager.active_runway_operations(carrier_id) != 1 || carrier_ptr->deck_occupancy != 1)
        throw std::runtime_error("carrier_launch_did_not_start");
    manager.update_all(1000.0f);
    if (first_ptr->status != Aircraft::Status::AIRBORNE ||
        manager.carrier_deck_occupancy(carrier_id) != 0 || carrier_ptr->deck_occupancy != 0)
        throw std::runtime_error("carrier_launch_did_not_free_capacity");

    first_ptr->fuel = 20.0f;
    first_ptr->material = 0.0f;
    first_ptr->ammunition = 0.0f;
    first_ptr->x = carrier_ptr->x;
    first_ptr->y = carrier_ptr->y;
    auto* second_ptr = component_manager.get_component<Aircraft>(second_aircraft_id);
    second_ptr->status = Aircraft::Status::AIRBORNE;
    second_ptr->x = carrier_ptr->x;
    second_ptr->y = carrier_ptr->y;

    if (!manager.order_aircraft_return(first_aircraft_id, carrier_id) ||
        !manager.queue_aircraft_for_landing(first_aircraft_id, carrier_id) ||
        manager.carrier_recovery_reservations(carrier_id) != 1)
        throw std::runtime_error("carrier_recovery_queue_failed");
    Pathfinding pathfinding(32, 32, 10.0f);
    const SafeReturnEstimate reserved_return = manager.estimate_safe_return(
        first_aircraft_id,
        pathfinding
    );
    if (reserved_return.facility_id != carrier_id || !reserved_return.safe)
        throw std::runtime_error("carrier_reservation_not_honored_by_safe_return");
    if (!manager.order_aircraft_return(second_aircraft_id, carrier_id) ||
        manager.queue_aircraft_for_landing(second_aircraft_id, carrier_id))
        throw std::runtime_error("carrier_recovery_overbooked");

    manager.update_all(50.0f);
    if (first_ptr->status != Aircraft::Status::LANDING)
        throw std::runtime_error("carrier_recovery_did_not_start");
    manager.update_all(1000.0f);
    if (first_ptr->status != Aircraft::Status::RECOVERING ||
        manager.carrier_deck_occupancy(carrier_id) != 1 ||
        manager.carrier_recovery_reservations(carrier_id) != 0)
        throw std::runtime_error("carrier_recovery_did_not_store_aircraft");

    const float fuel_before_service = first_ptr->fuel;
    manager.update_all(1000.0f);
    if (first_ptr->status != Aircraft::Status::ON_GROUND ||
        first_ptr->fuel != first_ptr->max_fuel ||
        first_ptr->material != first_ptr->max_material ||
        first_ptr->ammunition != first_ptr->max_ammunition ||
        std::abs(carrier_ptr->current_fuel - (500.0f - (100.0f - fuel_before_service))) > 0.001f ||
        carrier_ptr->current_munitions != 30.0f)
        throw std::runtime_error("carrier_refuel_rearm_failed");
}
