#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/naval_vessel.hpp"
#include <cmath>

using namespace rts;

TEST(scenario_two_landmasses_separated_by_ocean) {
    Simulation simulation;
    simulation.start();

    constexpr float landmass_width = 500.0f;
    constexpr float ocean_width = 3000.0f;
    constexpr float land_y = 0.0f;

    const Entity land1 = simulation.create_unit(0.0f, land_y);
    const Entity land2 = simulation.create_unit(landmass_width + ocean_width, land_y);

    if (std::abs(simulation.get_unit_x(land2.id) - simulation.get_unit_x(land1.id) - landmass_width - ocean_width) > 10.0f)
        throw std::runtime_error("scenario_two_landmasses_separated_by_ocean");
}

TEST(scenario_airbase_on_each_landmass) {
    Simulation simulation;
    simulation.start();

    constexpr float landmass_width = 500.0f;
    constexpr float ocean_width = 3000.0f;
    constexpr float land_y = 0.0f;
    const float ocean_midpoint = landmass_width + ocean_width / 2.0f;

    Airbase airbase1{};
    airbase1.x = 100.0f;
    airbase1.y = land_y;
    airbase1.runway_usable = true;
    airbase1.runway_capacity = 2;
    simulation.logistics_manager().add_airbase(100, airbase1);

    Airbase airbase2{};
    airbase2.x = landmass_width + ocean_width + 200.0f;
    airbase2.y = land_y;
    airbase2.runway_usable = true;
    airbase2.runway_capacity = 2;
    simulation.logistics_manager().add_airbase(101, airbase2);

    if (!simulation.logistics_manager().find_nearest_recovery_facility(
            landmass_width / 2.0f, land_y, RecoveryFacility::Type::AIRBASE))
        throw std::runtime_error("scenario_airbase_on_each_landmass_land1");
    if (!simulation.logistics_manager().find_nearest_recovery_facility(
            ocean_midpoint + landmass_width / 2.0f, land_y, RecoveryFacility::Type::AIRBASE))
        throw std::runtime_error("scenario_airbase_on_each_landmass_land2");
}

TEST(scenario_normal_fighter_cannot_safely_cross_alone) {
    Simulation simulation;
    simulation.start();

    const float ocean_width = 3000.0f;
    const float mid_ocean = ocean_width / 2.0f;

    const Entity fighter = simulation.create_unit(0.0f, 0.0f);
    simulation.logistics_manager().add_aircraft(fighter.id, 0.0f, 0.0f, 100.0f);
    
    auto* fighter_aircraft = simulation.component_manager().get_component<Aircraft>(fighter.id);
    if (!fighter_aircraft)
        throw std::runtime_error("scenario_normal_fighter_cannot_safely_cross_alone_create");

    fighter_aircraft->fuel = 100.0f;
    fighter_aircraft->max_fuel = 100.0f;
    fighter_aircraft->fuel_consumption_rate = 20.0f;
    fighter_aircraft->material = 10.0f;
    fighter_aircraft->max_material = 10.0f;
    fighter_aircraft->material_consumption_rate = 0.5f;
    fighter_aircraft->cruise_speed = 100.0f;
    fighter_aircraft->max_airborne_time_ms = 5000.0f;
    fighter_aircraft->status = Aircraft::Status::AIRBORNE;
    fighter_aircraft->type = Aircraft::Type::CONVENTIONAL;
    fighter_aircraft->x = mid_ocean;
    fighter_aircraft->y = 0.0f;

    const SafeReturnEstimate estimate = simulation.logistics_manager().estimate_safe_return(
        fighter.id, simulation.pathfinding());
    if (estimate.safe)
        throw std::runtime_error("scenario_normal_fighter_cannot_safely_cross_alone_estimate");
}

TEST(scenario_t1_carrier_enables_staged_crossing) {
    Simulation simulation;
    simulation.start();

    const float ocean_width = 3000.0f;
    const float carrier_x = ocean_width / 2.0f;

    Carrier carrier{};
    carrier.tier = Carrier::Tier::T1_LIGHT;
    carrier.x = carrier_x;
    carrier.y = 0.0f;
    carrier.runway_capacity = 1;
    carrier.runway_usable = true;
    carrier.deck_capacity = 2;
    carrier.refuel_rate = 100;
    carrier.rearm_rate = 20;
    carrier.max_fuel = 1000.0f;
    carrier.current_fuel = 1000.0f;
    carrier.max_munitions = 100.0f;
    carrier.current_munitions = 100.0f;
    carrier.max_speed = 50.0f;
    simulation.logistics_manager().add_carrier(200, carrier);

    const Entity fighter1 = simulation.create_unit(carrier_x - 100.0f, 0.0f);
    std::cerr << "fighter1.id=" << fighter1.id << "\n";
    simulation.logistics_manager().add_aircraft(fighter1.id, carrier_x - 100.0f, 0.0f, 100.0f);
    auto* fighter1_aircraft = simulation.component_manager().get_component<Aircraft>(fighter1.id);
    if (!fighter1_aircraft)
        throw std::runtime_error("scenario_t1_carrier_enables_staged_crossing_fighter1");
    fighter1_aircraft->status = Aircraft::Status::ON_GROUND;
    fighter1_aircraft->type = Aircraft::Type::CONVENTIONAL;
    fighter1_aircraft->fuel = 100.0f;
    fighter1_aircraft->max_fuel = 100.0f;
    fighter1_aircraft->material = 10.0f;
    fighter1_aircraft->max_material = 10.0f;
    fighter1_aircraft->ammunition = 10.0f;
    fighter1_aircraft->max_ammunition = 10.0f;

    auto* aircraft = simulation.component_manager().get_component<Aircraft>(fighter1.id);
    auto* facility = simulation.component_manager().get_component<RecoveryFacility>(200);
    std::cerr << "PRE-EMBARK: x=" << aircraft->x << ", y=" << aircraft->y << ", carrier=" << carrier.x << "," << carrier.y << "\n";
    std::cerr << "EMBARK RESULT: " << simulation.logistics_manager().embark_aircraft(fighter1.id, 200) << "\n";
    std::cerr << "AFTER EMBARK: x=" << aircraft->x << ", y=" << aircraft->y << "\n";

    std::cerr << "PRE-TAKEOFF INFO: fuel=" << aircraft->fuel << "/" << aircraft->max_fuel
              << ", mat=" << aircraft->material << "/" << aircraft->max_material
              << ", ammo=" << aircraft->ammunition << "/" << aircraft->max_ammunition
              << ", status=" << (int)aircraft->status
              << ", type=" << (int)aircraft->type
              << ", pos=(" << aircraft->x << "," << aircraft->y << ")\n";
    std::cerr << "FACILITY INFO: pos=(" << facility->x << "," << facility->y << "), max_dist=" << facility->max_recovery_distance << "\n";
    std::cerr << "CARRIER INFO: pos=(" << carrier.x << "," << carrier.y << ")\n";
    if (!simulation.logistics_manager().queue_aircraft_for_takeoff(fighter1.id, 200))
        throw std::runtime_error("scenario_t1_carrier_enables_staged_crossing_takeoff");
    std::cerr << "AFTER TAKEOFF QUEUE: status=" << (int)fighter1_aircraft->status << ", queue_slot=" << fighter1_aircraft->queue_slot << "\n";
    std::cerr << "CARRIER (runway) INFO: usable=" << carrier.runway_usable << ", capacity=" << carrier.runway_capacity << "\n";
    for (int i = 0; i < 25; i++) {
        simulation.update(50.0f);
        std::cerr << "Tick " << (i+1) << ": status=" << (int)fighter1_aircraft->status << ", fuel=" << fighter1_aircraft->fuel << "\n";
        if (fighter1_aircraft->status == Aircraft::Status::AIRBORNE) {
            std::cerr << "TAKEOFF COMPLETED at tick " << (i+1) << "!\n";
            break;
        }
    }
    if (fighter1_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_t1_carrier_enables_staged_crossing_airborne");

    fighter1_aircraft->x = carrier.x;
    fighter1_aircraft->y = carrier.y;
    fighter1_aircraft->fuel = 50.0f;
    if (!simulation.logistics_manager().order_aircraft_return(fighter1.id, 200)) {
        std::cerr << "RETURN ORDER FAIL: status=" << (int)fighter1_aircraft->status << "\n";
        throw std::runtime_error("scenario_t1_carrier_enables_staged_crossing_return");
    }
    auto* fighter1_before_landing = simulation.component_manager().get_component<Aircraft>(fighter1.id);
    std::cerr << "BEFORE LANDING: status=" << (int)fighter1_before_landing->status << ", pos=(" << fighter1_before_landing->x << "," << fighter1_before_landing->y << ")\n";
    if (!simulation.logistics_manager().queue_aircraft_for_landing(fighter1.id, 200)) {
        std::cerr << "LANDED QUEUE FAIL: status=" << (int)fighter1_before_landing->status << "\n";
        throw std::runtime_error("scenario_t1_carrier_enables_staged_crossing_landing_queue");
    }
    auto* fighter1_after_queue = simulation.component_manager().get_component<Aircraft>(fighter1.id);
    std::cerr << "AFTER QUEUE: status=" << (int)fighter1_after_queue->status << ", pos=(" << fighter1_after_queue->x << "," << fighter1_after_queue->y << "), fuel=" << fighter1_after_queue->fuel << "\n";
    for (int i = 0; i < 33; i++) {
        std::cerr << "=== Before update i=" << i << " ===\n";
        simulation.update(50.0f);
        std::cerr << "=== After update i=" << i << " ===\n";
        auto* fighter1_check = simulation.component_manager().get_component<Aircraft>(fighter1.id);
        float fuel = fighter1_check ? fighter1_check->fuel : -1;
        Aircraft::Status status = fighter1_check ? fighter1_check->status : Aircraft::Status::CRASHED;
        std::cerr << "Tick " << (i+1) << " LANDING: status=" << (int)status << ", fuel=" << fuel << ", fighter1_aircraft_addr=" << (void*)fighter1_aircraft << ", fighter1_check_addr=" << (void*)fighter1_check << "\n";
        if (fighter1_aircraft->status == Aircraft::Status::ON_GROUND) {
            std::cerr << "RECOVERY COMPLETED at tick " << (i+1) << "!\n";
            break;
        }
    }
    std::cerr << "END OF LOOP: status=" << (int)fighter1_aircraft->status << ", fuel=" << fighter1_aircraft->fuel << ", max_fuel=" << fighter1_aircraft->max_fuel << "\n";
    if (fighter1_aircraft->status != Aircraft::Status::ON_GROUND ||
        fighter1_aircraft->fuel != fighter1_aircraft->max_fuel)
        throw std::runtime_error("scenario_t1_carrier_enables_staged_crossing_refuel");
}

TEST(scenario_fighter_refuel_recover_on_carrier) {
    Simulation simulation;
    simulation.start();

    Carrier carrier{};
    carrier.tier = Carrier::Tier::T1_LIGHT;
    carrier.x = 0.0f;
    carrier.y = 0.0f;
    carrier.runway_capacity = 1;
    carrier.runway_usable = true;
    carrier.deck_capacity = 1;
    carrier.refuel_rate = 100;
    carrier.rearm_rate = 20;
    carrier.max_fuel = 1000.0f;
    carrier.current_fuel = 500.0f;
    carrier.max_munitions = 100.0f;
    carrier.current_munitions = 50.0f;
    carrier.max_speed = 50.0f;
    simulation.logistics_manager().add_carrier(210, carrier);

    const Entity fighter = simulation.create_unit(-50.0f, 0.0f);
    simulation.logistics_manager().add_aircraft(fighter.id, -50.0f, 0.0f, 100.0f);
    auto* fighter_aircraft = simulation.component_manager().get_component<Aircraft>(fighter.id);
    if (!fighter_aircraft)
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_create");
    fighter_aircraft->status = Aircraft::Status::ON_GROUND;
    fighter_aircraft->type = Aircraft::Type::CONVENTIONAL;
    fighter_aircraft->fuel = 100.0f;
    fighter_aircraft->max_fuel = 100.0f;
    fighter_aircraft->material = 10.0f;
    fighter_aircraft->max_material = 10.0f;
    fighter_aircraft->ammunition = 10.0f;
    fighter_aircraft->max_ammunition = 10.0f;

    if (!simulation.logistics_manager().embark_aircraft(fighter.id, 210))
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_embark");

    if (!simulation.logistics_manager().queue_aircraft_for_takeoff(fighter.id, 210))
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_launch");

    for (int tick = 0; tick < 22; ++tick) {
        simulation.update(50.0f);
        if (tick == 21 && fighter_aircraft->status == Aircraft::Status::AIRBORNE) {
            std::cerr << "Fighter airborne after tick 22\n";
        }
    }
    if (fighter_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_airborne");

    fighter_aircraft->fuel = 5.0f;
    fighter_aircraft->material = 0.5f;
    fighter_aircraft->ammunition = 0.5f;

    if (!simulation.logistics_manager().order_aircraft_return(fighter.id, 210)) {
        std::cerr << "order_aircraft_return FAILED\n";
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_return");
    }
    if (!simulation.logistics_manager().queue_aircraft_for_landing(fighter.id, 210)) {
        std::cerr << "queue_aircraft_for_landing FAILED\n";
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_return");
    }
    std::cerr << "Landing queued for aircraft " << fighter.id << "\n";
    
    std::cerr << "AFTER LANDING QUEUE DEBUG: queue_size=" << simulation.logistics_manager().landing_queue_size(210) << "\n";
    
    for (int tick = 0; tick < 10; ++tick) {
        simulation.update(50.0f);
    }
    std::cerr << "AFTER LANDING QUEUE: status=" << static_cast<int>(fighter_aircraft->status)
              << " fuel=" << fighter_aircraft->fuel << "\n";
    
    for (int tick = 0; tick < 32; ++tick) {
        simulation.update(50.0f);
        if (tick % 10 == 0) {
            std::cerr << "RECOVERY TICK " << tick << ": status=" << static_cast<int>(fighter_aircraft->status)
                      << " fuel=" << fighter_aircraft->fuel << "\n";
        }
    }
    std::cerr << "BEFORE CHECK: status=" << static_cast<int>(fighter_aircraft->status)
              << " fuel=" << fighter_aircraft->fuel << "\n";
    std::cerr << "DEBUG: status=" << static_cast<int>(fighter_aircraft->status)
              << " fuel=" << fighter_aircraft->fuel
              << " max_fuel=" << fighter_aircraft->max_fuel
              << " material=" << fighter_aircraft->material
              << " max_material=" << fighter_aircraft->max_material
              << " ammo=" << fighter_aircraft->ammunition
              << " max_ammo=" << fighter_aircraft->max_ammunition << "\n";
    if (fighter_aircraft->status != Aircraft::Status::ON_GROUND ||
        std::abs(fighter_aircraft->fuel - fighter_aircraft->max_fuel) > 0.001f ||
        std::abs(fighter_aircraft->material - fighter_aircraft->max_material) > 0.001f ||
        std::abs(fighter_aircraft->ammunition - fighter_aircraft->max_ammunition) > 0.001f)
        throw std::runtime_error("scenario_fighter_refuel_recover_on_carrier_refuel");
}

TEST(scenario_conventional_aircraft_requires_runway_deck_recovery) {
    Simulation simulation;
    simulation.start();

    const Entity fighter = simulation.create_unit(0.0f, 0.0f);
    simulation.logistics_manager().add_aircraft(fighter.id, 0.0f, 0.0f, 100.0f);
    auto* fighter_aircraft = simulation.component_manager().get_component<Aircraft>(fighter.id);
    if (!fighter_aircraft)
        throw std::runtime_error("scenario_conventional_aircraft_requires_runway_create");
    fighter_aircraft->status = Aircraft::Status::AIRBORNE;
    fighter_aircraft->type = Aircraft::Type::CONVENTIONAL;

    if (simulation.logistics_manager().launch_vtol(fighter.id))
        throw std::runtime_error("scenario_conventional_aircraft_requires_runway_vtol");

    if (simulation.logistics_manager().land_vtol(fighter.id))
        throw std::runtime_error("scenario_conventional_aircraft_requires_runway_land_vtol");
}

TEST(scenario_vtol_does_not_require_runway) {
    Simulation simulation;
    simulation.start();

    const Entity vtol = simulation.create_unit(100.0f, 0.0f);
    simulation.logistics_manager().add_aircraft(vtol.id, 100.0f, 0.0f, 100.0f);
    auto* vtol_aircraft = simulation.component_manager().get_component<Aircraft>(vtol.id);
    if (!vtol_aircraft)
        throw std::runtime_error("scenario_vtol_does_not_require_runway_create");
    vtol_aircraft->status = Aircraft::Status::ON_GROUND;
    vtol_aircraft->type = Aircraft::Type::VTOL;
    vtol_aircraft->fuel = 100.0f;
    vtol_aircraft->max_fuel = 100.0f;
    vtol_aircraft->material = 10.0f;
    vtol_aircraft->max_material = 10.0f;

    if (!simulation.logistics_manager().launch_vtol(vtol.id) ||
        vtol_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_vtol_does_not_require_runway_launch");

    if (!simulation.logistics_manager().land_vtol(vtol.id) ||
        vtol_aircraft->status != Aircraft::Status::ON_GROUND)
        throw std::runtime_error("scenario_vtol_does_not_require_runway_land");
}

TEST(scenario_overextended_conventional_aircraft_crashes) {
    Simulation simulation;
    simulation.start();

    const Entity aircraft = simulation.create_unit(1000.0f, 0.0f);
    simulation.logistics_manager().add_aircraft(aircraft.id, 1000.0f, 0.0f, 1.0f);
    auto* aircraft_ptr = simulation.component_manager().get_component<Aircraft>(aircraft.id);
    if (!aircraft_ptr)
        throw std::runtime_error("scenario_overextended_conventional_aircraft_crashes_create");
    aircraft_ptr->fuel = 1.0f;
    aircraft_ptr->max_fuel = 100.0f;
    aircraft_ptr->fuel_consumption_rate = 2.0f;
    aircraft_ptr->material = 1.0f;
    aircraft_ptr->max_material = 10.0f;
    aircraft_ptr->material_consumption_rate = 0.5f;
    aircraft_ptr->cruise_speed = 100.0f;
    aircraft_ptr->max_airborne_time_ms = 25.0f;
    aircraft_ptr->status = Aircraft::Status::AIRBORNE;
    aircraft_ptr->type = Aircraft::Type::CONVENTIONAL;
    aircraft_ptr->closest_recovery_facility = INVALID_ENTITY;

    simulation.update(50.0f);
    if (aircraft_ptr->status == Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_overextended_conventional_aircraft_crashes_not_crashed");

    if (simulation.entity_count() != 0 ||
        simulation.component_manager().get_component<Aircraft>(aircraft.id) != nullptr)
        throw std::runtime_error("scenario_overextended_conventional_aircraft_crashes_not_removed");
}

TEST(scenario_destroyer_has_finite_endurance) {
    Simulation simulation;
    simulation.start();

    const Entity vessel = simulation.create_unit(0.0f, 0.0f);
    simulation.logistics_manager().add_naval_vessel(vessel.id, 0.0f, 0.0f, 100.0f);
    auto* vessel_ptr = simulation.component_manager().get_component<NavalVessel>(vessel.id);
    if (!vessel_ptr)
        throw std::runtime_error("scenario_destroyer_has_finite_endurance_create");
    vessel_ptr->fuel = 100.0f;
    vessel_ptr->max_fuel = 1000.0f;
    vessel_ptr->fuel_consumption_rate = 2.0f;

    simulation.update(50.0f);
    auto* vessel_after = simulation.component_manager().get_component<NavalVessel>(vessel.id);
    if (!vessel_after || vessel_after->fuel >= 100.0f)
        throw std::runtime_error("scenario_destroyer_has_finite_endurance_consumption");
}

TEST(scenario_destroyer_becomes_stranded_when_depleted) {
    Simulation simulation;
    simulation.start();

    const Entity vessel = simulation.create_unit(0.0f, 0.0f);
    simulation.logistics_manager().add_naval_vessel(vessel.id, 0.0f, 0.0f, 50.0f);
    auto* vessel_ptr = simulation.component_manager().get_component<NavalVessel>(vessel.id);
    if (!vessel_ptr)
        throw std::runtime_error("scenario_destroyer_becomes_stranded_create");
    vessel_ptr->fuel = 50.0f;
    vessel_ptr->max_fuel = 1000.0f;
    vessel_ptr->fuel_consumption_rate = 200.0f;

    simulation.update(250.0f);
    auto* vessel_after = simulation.component_manager().get_component<NavalVessel>(vessel.id);
    if (!vessel_after || !vessel_after->is_stranded)
        throw std::runtime_error("scenario_destroyer_becomes_stranded_not_stranded");
}

TEST(scenario_naval_base_restores_it) {
    Simulation simulation;
    simulation.start();

    const Entity vessel = simulation.create_unit(0.0f, 0.0f);
    simulation.logistics_manager().add_naval_vessel(vessel.id, 0.0f, 0.0f, 10.0f);
    auto* vessel_ptr = simulation.component_manager().get_component<NavalVessel>(vessel.id);
    if (!vessel_ptr)
        throw std::runtime_error("scenario_naval_base_restores_it_create");
    vessel_ptr->fuel = 10.0f;
    vessel_ptr->max_fuel = 1000.0f;
    vessel_ptr->is_stranded = true;
    vessel_ptr->fuel_consumption_rate = 100.0f;

    simulation.logistics_manager().add_naval_base(300, 10.0f, 0.0f, 50.0f);

    const float fuel_before = vessel_ptr->fuel;
    simulation.logistics_manager().resupply_naval_vessel(vessel.id, 100.0f);
    auto* vessel_after = simulation.component_manager().get_component<NavalVessel>(vessel.id);
    if (!vessel_after || vessel_after->fuel <= fuel_before || vessel_after->is_stranded)
        throw std::runtime_error("scenario_naval_base_restores_it_not_unstranded");
}

TEST(scenario_complete_logistics_test_map) {
    Simulation simulation;
    simulation.start();
    std::cerr << "=== scenario_complete_logistics_test_map STARTED ===\n";

    constexpr float landmass_width = 500.0f;
    constexpr float ocean_width = 3000.0f;
    constexpr float land_y = 0.0f;

    Airbase airbase1{};
    airbase1.x = 100.0f;
    airbase1.y = land_y;
    airbase1.runway_usable = true;
    airbase1.runway_capacity = 2;
    simulation.logistics_manager().add_airbase(100, airbase1);

    Airbase airbase2{};
    airbase2.x = landmass_width + ocean_width + 200.0f;
    airbase2.y = land_y;
    airbase2.runway_usable = true;
    airbase2.runway_capacity = 2;
    simulation.logistics_manager().add_airbase(101, airbase2);

    Carrier carrier{};
    carrier.tier = Carrier::Tier::T1_LIGHT;
    carrier.x = ocean_width / 2.0f;
    carrier.y = land_y;
    carrier.runway_capacity = 1;
    carrier.runway_usable = true;
    carrier.deck_capacity = 2;
    carrier.refuel_rate = 100;
    carrier.rearm_rate = 20;
    carrier.max_fuel = 1000.0f;
    carrier.current_fuel = 1000.0f;
    carrier.max_munitions = 100.0f;
    carrier.current_munitions = 100.0f;
    carrier.max_speed = 50.0f;
    simulation.logistics_manager().add_carrier(200, carrier);

    simulation.logistics_manager().add_naval_base(300, 100.0f, land_y + 200.0f, 500.0f);

    Entity fighter1 = simulation.create_unit(1500.0f, land_y);
    simulation.logistics_manager().add_aircraft(fighter1.id, 1500.0f, land_y, 100.0f);
    auto* fighter1_aircraft = simulation.component_manager().get_component<Aircraft>(fighter1.id);
    if (!fighter1_aircraft)
        throw std::runtime_error("scenario_complete_logistics_test_map_fighter1");
    fighter1_aircraft->status = Aircraft::Status::ON_GROUND;
    fighter1_aircraft->type = Aircraft::Type::CONVENTIONAL;
    fighter1_aircraft->fuel = 100.0f;
    fighter1_aircraft->max_fuel = 100.0f;
    fighter1_aircraft->material = 10.0f;
    fighter1_aircraft->max_material = 10.0f;
    fighter1_aircraft->ammunition = 10.0f;
    fighter1_aircraft->max_ammunition = 10.0f;

    Entity fighter2 = simulation.create_unit(1500.0f, land_y);
    simulation.logistics_manager().add_aircraft(fighter2.id, 1500.0f, land_y, 100.0f);
    auto* fighter2_aircraft = simulation.component_manager().get_component<Aircraft>(fighter2.id);
    if (!fighter2_aircraft)
        throw std::runtime_error("scenario_complete_logistics_test_map_fighter2");
    fighter2_aircraft->status = Aircraft::Status::ON_GROUND;
    fighter2_aircraft->type = Aircraft::Type::CONVENTIONAL;
    fighter2_aircraft->fuel = 100.0f;
    fighter2_aircraft->max_fuel = 100.0f;
    fighter2_aircraft->material = 10.0f;
    fighter2_aircraft->max_material = 10.0f;
    fighter2_aircraft->ammunition = 10.0f;
    fighter2_aircraft->max_ammunition = 10.0f;

    Entity vtol = simulation.create_unit(1500.0f, land_y + 100.0f);
    simulation.logistics_manager().add_aircraft(vtol.id, 1500.0f, land_y + 100.0f, 100.0f);
    auto* vtol_aircraft = simulation.component_manager().get_component<Aircraft>(vtol.id);
    if (!vtol_aircraft)
        throw std::runtime_error("scenario_complete_logistics_test_map_vtol");
    vtol_aircraft->status = Aircraft::Status::ON_GROUND;
    vtol_aircraft->type = Aircraft::Type::VTOL;
    vtol_aircraft->fuel = 100.0f;
    vtol_aircraft->max_fuel = 100.0f;
    vtol_aircraft->material = 10.0f;
    vtol_aircraft->max_material = 10.0f;

    Entity destroyer = simulation.create_unit(1500.0f, land_y + 300.0f);
    simulation.logistics_manager().add_naval_vessel(destroyer.id, 1500.0f, land_y + 300.0f, 500.0f);
    auto* destroyer_ptr = simulation.component_manager().get_component<NavalVessel>(destroyer.id);
    if (!destroyer_ptr)
        throw std::runtime_error("scenario_complete_logistics_test_map_destroyer");
    destroyer_ptr->fuel = 500.0f;
    destroyer_ptr->max_fuel = 1000.0f;
    destroyer_ptr->fuel_consumption_rate = 200.0f;
    const float initial_destroyer_fuel = destroyer_ptr->fuel;

    Entity recon = simulation.create_unit(1500.0f, land_y + 400.0f);
    simulation.logistics_manager().add_aircraft(recon.id, 1500.0f, land_y + 400.0f, 200.0f);
    auto* recon_aircraft = simulation.component_manager().get_component<Aircraft>(recon.id);
    if (!recon_aircraft)
        throw std::runtime_error("scenario_complete_logistics_test_map_recon");
    recon_aircraft->fuel = 200.0f;
    recon_aircraft->max_fuel = 200.0f;
    recon_aircraft->material = 20.0f;
    recon_aircraft->max_material = 20.0f;
    recon_aircraft->status = Aircraft::Status::ON_GROUND;

    if (!simulation.logistics_manager().embark_aircraft(fighter1.id, 200))
        throw std::runtime_error("scenario_complete_logistics_test_map_fighter1_embark");
    if (!simulation.logistics_manager().queue_aircraft_for_takeoff(fighter1.id, 200))
        throw std::runtime_error("scenario_complete_logistics_test_map_fighter1_takeoff");
    for (int tick = 0; tick < 21; ++tick) {
        simulation.update(50.0f);
        if (tick == 20) {
            std::cerr << "Tick 21: status=" << (int)fighter1_aircraft->status << ", fuel=" << fighter1_aircraft->fuel << "\n";
        }
    }
    if (fighter1_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_complete_logistics_test_map_fighter1_airborne");
    fighter1_aircraft->x = ocean_width;
    fighter1_aircraft->y = land_y;

    if (!simulation.logistics_manager().embark_aircraft(fighter2.id, 200))
        throw std::runtime_error("scenario_complete_logistics_test_map_fighter2_embark");

    if (!simulation.logistics_manager().launch_vtol(vtol.id) ||
        vtol_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_complete_logistics_test_map_vtol_launch");

    vtol_aircraft->x = ocean_width;
    vtol_aircraft->y = land_y;
    vtol_aircraft->fuel = 50.0f;

    if (!simulation.logistics_manager().land_vtol(vtol.id) ||
        vtol_aircraft->status != Aircraft::Status::ON_GROUND)
        throw std::runtime_error("scenario_complete_logistics_test_map_vtol_land");

    if (destroyer_ptr->fuel >= initial_destroyer_fuel)
        throw std::runtime_error("scenario_complete_logistics_test_map_destroyer_consumption");
    destroyer_ptr->fuel = 10.0f;
    simulation.update(50.0f);
    auto* destroyer_after = simulation.component_manager().get_component<NavalVessel>(destroyer.id);
    if (!destroyer_after || !destroyer_after->is_stranded)
        throw std::runtime_error("scenario_complete_logistics_test_map_destroyer_stranded");

    simulation.logistics_manager().resupply_naval_vessel(destroyer.id, 100.0f);
    destroyer_after = simulation.component_manager().get_component<NavalVessel>(destroyer.id);
    if (!destroyer_after || destroyer_after->is_stranded)
        throw std::runtime_error("scenario_complete_logistics_test_map_destroyer_resupplied");

    recon_aircraft->x = 100.0f;
    recon_aircraft->y = land_y;
    if (!simulation.logistics_manager().queue_aircraft_for_takeoff(recon.id, 100))
        throw std::runtime_error("scenario_complete_logistics_test_map_recon_takeoff");
    for (int tick = 0; tick < 21; ++tick) {
        simulation.update(50.0f);
        if (tick == 20 && recon_aircraft->status == Aircraft::Status::AIRBORNE) {
            std::cerr << "Recon airborne after tick 21\n";
        }
    }
    if (recon_aircraft->status != Aircraft::Status::AIRBORNE)
        throw std::runtime_error("scenario_complete_logistics_test_map_recon_airborne");

    recon_aircraft->x = ocean_width;
    recon_aircraft->y = land_y;
    simulation.logistics_manager().update_intelligence(recon.id, 1500.0f, land_y, 2000);

     simulation.destroy_unit(recon.id);
     auto* intel = simulation.logistics_manager().get_intelligence(recon.id);
     if (!intel || std::abs(intel->last_x - 1500.0f) > 0.001f ||
         std::abs(intel->last_y - land_y) > 0.001f || intel->last_seen_tick != 2000)
         throw std::runtime_error("scenario_complete_logistics_test_map_intel_not_preserved");
 }

TEST(scenario_t3_recon_aircraft_crosses_theater_and_returns) {
     Simulation simulation;
     simulation.start();
     std::cerr << "=== scenario_t3_recon_aircraft_crosses_theater_and_returns STARTED ===\n";

     constexpr float landmass_width = 500.0f;
     constexpr float ocean_width = 3000.0f;
     constexpr float land_y = 0.0f;

     Airbase airbase1{};
     airbase1.x = 100.0f;
     airbase1.y = land_y;
     airbase1.runway_usable = true;
     airbase1.runway_capacity = 2;
     airbase1.refuel_rate = 100;
     airbase1.rearm_rate = 20;
     airbase1.max_fuel = 1000.0f;
     airbase1.current_fuel = 1000.0f;
     airbase1.max_munitions = 100.0f;
     airbase1.current_munitions = 100.0f;
     simulation.logistics_manager().add_airbase(100, airbase1);

     const Entity recon = simulation.create_unit(100.0f, land_y);
     simulation.logistics_manager().add_aircraft(recon.id, 100.0f, land_y, 200.0f);
     auto* recon_aircraft = simulation.component_manager().get_component<Aircraft>(recon.id);
     if (!recon_aircraft)
         throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_create");
     recon_aircraft->status = Aircraft::Status::ON_GROUND;
     recon_aircraft->type = Aircraft::Type::CONVENTIONAL;
     recon_aircraft->fuel = 200.0f;
     recon_aircraft->max_fuel = 200.0f;
     recon_aircraft->material = 20.0f;
     recon_aircraft->max_material = 20.0f;

     if (!simulation.logistics_manager().queue_aircraft_for_takeoff(recon.id, 100))
         throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_takeoff");
     for (int tick = 0; tick < 22; ++tick) {
         simulation.update(50.0f);
         if (tick == 21 && recon_aircraft->status == Aircraft::Status::AIRBORNE) {
             std::cerr << "Recon airborne after tick 22\n";
         }
     }
     if (recon_aircraft->status != Aircraft::Status::AIRBORNE)
         throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_airborne");

     recon_aircraft->x = ocean_width;
     recon_aircraft->y = land_y;

     std::cerr << "Before return: recon at (" << recon_aircraft->x << ", " << recon_aircraft->y 
               << "), airbase at (" << airbase1.x << ", " << airbase1.y << ")\n";

     if (!simulation.logistics_manager().order_aircraft_return(recon.id, 100)) {
         std::cerr << "order_aircraft_return failed\n";
         throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_return");
     }

     recon_aircraft->x = airbase1.x + 400.0f;
     recon_aircraft->y = airbase1.y;
     std::cerr << "Recon moved near airbase: (" << recon_aircraft->x << ", " << recon_aircraft->y << ")\n";

     if (!simulation.logistics_manager().queue_aircraft_for_landing(recon.id, 100)) {
         std::cerr << "queue_aircraft_for_landing failed\n";
         throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_landing");
     }

      for (int tick = 0; tick < 33; ++tick) {
          simulation.update(50.0f);
      }
      std::cerr << "After updates: recon at (" << recon_aircraft->x << ", " << recon_aircraft->y << "), status="<<(int)recon_aircraft->status
                << ", target_base=" << recon_aircraft->target_base << ", fuel=" << recon_aircraft->fuel << ", material=" << recon_aircraft->material << "\n";
      if (recon_aircraft->status != Aircraft::Status::ON_GROUND)
          throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_landed");
      if (std::abs(recon_aircraft->fuel - recon_aircraft->max_fuel) > 0.001f)
          throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_refueled");
      if (std::abs(recon_aircraft->material - recon_aircraft->max_material) > 0.001f)
          throw std::runtime_error("scenario_t3_recon_aircraft_crosses_theater_and_returns_rearmed");
  }

TEST(scenario_intel_becomes_stale_over_time) {
     Simulation simulation;
     simulation.start();

     const Entity enemy = simulation.create_unit(100.0f, 0.0f);

     simulation.logistics_manager().update_intelligence(enemy.id, 100.0f, 0.0f, 1000);
     auto* intel = simulation.component_manager().get_component<Intelligence>(enemy.id);
     if (!intel || intel->last_seen_tick != 1000)
         throw std::runtime_error("scenario_intel_becomes_stale_over_time_initial");

     for (int i = 0; i < 150; ++i) {
         simulation.update(50.0f);
     }

     uint32_t current_tick = 1000 + 150;
     intel = simulation.component_manager().get_component<Intelligence>(enemy.id);
     uint32_t age = current_tick - intel->last_seen_tick;
     bool stale = age > 100;

     std::cerr << "After 150 updates: current_tick=" << current_tick
               << " last_seen=" << intel->last_seen_tick
               << " age=" << age
               << " stale=" << stale << "\n";

     if (!stale)
         throw std::runtime_error("scenario_intel_becomes_stale_over_time_not_stale");
 }
