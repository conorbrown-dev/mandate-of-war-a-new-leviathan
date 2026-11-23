#include <cmath>
#include "test_framework.hpp"
#include "simulation/simulation.hpp"

using namespace rts;

namespace {
void verify(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

// Broken Strait's layout: two landmasses 14 km wide either side of an 11 km
// strait. Ports sit on the west landmass's strait coast; the sea lane enters
// from the north.
constexpr uint32_t kHomePort = 1;
constexpr uint32_t kNeutralPort = 2;
constexpr float kQuayX = -5700.0f;
constexpr float kBerthX = -5300.0f;

struct PortTheater {
    Simulation simulation;
    EntityId player_post = INVALID_ENTITY;
    EntityId enemy_post = INVALID_ENTITY;

    PortTheater() {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f, 12500.0f, 0.0f, 14000.0f, 38000.0f);
        simulation.start();
        simulation.set_forces_by_transport(true);
        player_post = simulation.create_faction_base(FactionId::ELITE_PRECISION, -12500.0f, 0.0f);
        enemy_post = simulation.create_faction_base(FactionId::MASS_WARFARE, -15000.0f, -6500.0f);
        // The home port is 2 km from its entry; the neutral one is further south.
        simulation.add_port(kHomePort, kQuayX, 3000.0f, kBerthX, 3000.0f, kBerthX, 5000.0f, 0);
        simulation.add_port(kNeutralPort, kQuayX, -4000.0f, kBerthX, -4000.0f, kBerthX, -6000.0f, -1);
    }
    ~PortTheater() { simulation.stop(); }
    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
    EntityId tank(float x, float y, FactionId faction) {
        const auto id = static_cast<EntityId>(simulation.create_unit_with_type(x, y,
            faction == FactionId::ELITE_PRECISION ? UnitType::ELITE_MAIN_BATTLE_TANK : UnitType::MASS_SWARM_TANK, faction));
        simulation.stop_unit(id);
        return id;
    }
};
}

TEST(a_port_is_taken_by_a_ground_unit_that_holds_it) {
    PortTheater theater;
    verify(theater.simulation.port_network().port(kNeutralPort)->owner == -1, "the port starts neutral");
    theater.tank(kQuayX - 40.0f, -4000.0f, FactionId::ELITE_PRECISION);
    theater.run(20 * 5);
    verify(theater.simulation.port_network().port(kNeutralPort)->owner == -1, "a short stay does not take it");
    theater.run(20 * 5);
    verify(theater.simulation.port_network().port(kNeutralPort)->owner == 0, "holding it for 8 seconds takes it");
}

TEST(an_enemy_at_the_port_stops_it_being_taken) {
    PortNetwork network;
    Port port;
    port.id = 7;
    port.x = 0.0f;
    port.y = 0.0f;
    verify(network.add(port), "the port is added");
    const std::vector<PortPresence> both{{0, 10.0f, 0.0f}, {1, -10.0f, 0.0f}};
    for (int step = 0; step < 400; ++step) network.update(50.0f, both);
    verify(network.port(7)->owner == -1, "a contested port is taken by neither side");
    const std::vector<PortPresence> alone{{0, 10.0f, 0.0f}};
    for (int step = 0; step < 100; ++step) network.update(50.0f, alone);
    verify(network.port(7)->owner == -1, "5 seconds alone is not enough");
    // A claim fades while nobody holds on, so a drive-by does not add up.
    const std::vector<PortPresence> nobody;
    for (int step = 0; step < 200; ++step) network.update(50.0f, nobody);
    for (int step = 0; step < 100; ++step) network.update(50.0f, alone);
    verify(network.port(7)->owner == -1, "a claim fades when the unit leaves");
    for (int step = 0; step < 200; ++step) network.update(50.0f, alone);
    verify(network.port(7)->owner == 0, "holding on takes it");
}

TEST(a_unit_far_from_the_port_does_not_take_it) {
    PortTheater theater;
    theater.tank(-9000.0f, -4000.0f, FactionId::ELITE_PRECISION);
    theater.run(20 * 20);
    verify(theater.simulation.port_network().port(kNeutralPort)->owner == -1, "only units at the quay count");
}

TEST(sea_delivery_needs_a_held_port) {
    PortTheater theater;
    const auto tank = UnitType::MASS_SWARM_TANK;
    verify(theater.simulation.sea_delivery_refusal(FactionId::MASS_WARFARE, tank, 0) == "NO PORT HELD", "a side with no port cannot ship in");
    verify(theater.simulation.sea_delivery_refusal(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0).empty(),
           "a side with a port can");
    verify(theater.simulation.sea_delivery_refusal(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kNeutralPort) == "PORT NOT HELD",
           "and cannot use a port it does not hold");
}

TEST(a_cargo_ship_brings_the_unit_to_the_port) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    const float before = simulation.production_manager().storages().at(theater.player_post).tactical.materials.current;
    const int units_before = simulation.unit_count(FactionId::ELITE_PRECISION);
    verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0), "the order is accepted");
    const float after = simulation.production_manager().storages().at(theater.player_post).tactical.materials.current;
    verify(after < before, "the unit is paid for at once");
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 1, "the order is inbound");
    // Build time, then the voyage: a ship appears and sails.
    theater.run(20 * 20);
    verify(simulation.unit_count(FactionId::ELITE_PRECISION) == units_before + 1, "the cargo ship is on the water");
    EntityId ship = INVALID_ENTITY;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == 21) ship = id;
    }
    verify(ship != INVALID_ENTITY, "the ship is an Elite cargo ship");
    verify(simulation.get_unit_y(ship) < 5000.0f, "it has left the start of the lane");
    theater.run(20 * 120);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 0, "the voyage is over");
    verify(simulation.unit_count(FactionId::ELITE_PRECISION) == units_before + 1, "the ship is gone and the cargo is ashore");
    bool ashore = false;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == static_cast<int>(UnitType::ELITE_MAIN_BATTLE_TANK) &&
            std::hypot(simulation.get_unit_x(id) - kQuayX, simulation.get_unit_y(id) - 3000.0f) < 250.0f) ashore = true;
    }
    verify(ashore, "the tank is on land beside the quay");
}

TEST(a_sunk_cargo_ship_loses_its_load) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    const int units_before = simulation.unit_count(FactionId::ELITE_PRECISION);
    verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort), "ordered");
    const float paid = simulation.production_manager().storages().at(theater.player_post).tactical.materials.current;
    theater.run(20 * 20);
    EntityId ship = INVALID_ENTITY;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == 21) ship = id;
    }
    verify(ship != INVALID_ENTITY, "the ship sails");
    simulation.destroy_unit(ship);
    theater.run(20 * 5);
    verify(simulation.ships_lost(FactionId::ELITE_PRECISION) == 1, "the loss is counted");
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 0, "the order is over");
    theater.run(20 * 120);
    verify(simulation.unit_count(FactionId::ELITE_PRECISION) == units_before, "nothing is delivered");
    verify(simulation.production_manager().storages().at(theater.player_post).tactical.materials.current <= paid, "there is no refund");
}

TEST(a_ship_waits_while_its_port_belongs_to_the_enemy) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    const int units_before = simulation.unit_count(FactionId::ELITE_PRECISION);
    verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort), "ordered");
    theater.run(20 * 20);
    simulation.port_network().set_owner(kHomePort, 1);
    theater.run(20 * 120);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 1, "the cargo is still on the ship");
    verify(simulation.unit_count(FactionId::ELITE_PRECISION) == units_before + 1, "only the ship exists, the unit is not ashore");
    simulation.port_network().set_owner(kHomePort, 0);
    theater.run(20 * 10);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 0, "it unloads once the port is held again");
}

TEST(sea_delivery_is_a_recorded_command) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    verify(simulation.issue_sea_delivery_command(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort) == 1, "the command is accepted");
    theater.run(2);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 1, "the command orders the delivery");
    verify(simulation.issue_sea_delivery_command(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, kHomePort) == 0, "a side cannot use another's port");
}

TEST(an_armed_enemy_ship_in_the_lane_blocks_sea_delivery) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    simulation.begin_match(theater.player_post, theater.enemy_post);
    simulation.port_network().set_owner(kNeutralPort, 1); // the Mass side holds it
    const auto scout = static_cast<EntityId>(simulation.create_unit_with_type(kQuayX, -4600.0f, static_cast<UnitType>(15), FactionId::MASS_WARFARE));
    simulation.stop_unit(scout);
    theater.run(40);
    verify(simulation.sea_delivery_refusal(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 0).empty(), "a clear lane lets it ship");
    const auto boat = static_cast<EntityId>(simulation.create_unit_with_type(-5400.0f, -4800.0f, UnitType::ELITE_PATROL_BOAT, FactionId::ELITE_PRECISION));
    simulation.stop_unit(boat);
    theater.run(40);
    verify(simulation.sea_delivery_refusal(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 0) == "SEA LANE CONTESTED",
           "an armed enemy ship it can see blocks the order");
    verify(!simulation.order_sea_delivery(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 0), "the order is refused");
    simulation.destroy_unit(boat);
    theater.run(40);
    verify(simulation.sea_delivery_refusal(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 0).empty(), "the lane reopens when it is gone");
}

TEST(a_hidden_enemy_ship_does_not_block_the_lane) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    simulation.begin_match(theater.player_post, theater.enemy_post);
    simulation.port_network().set_owner(kNeutralPort, 1);
    const auto boat = static_cast<EntityId>(simulation.create_unit_with_type(-5400.0f, -4800.0f, UnitType::ELITE_PATROL_BOAT, FactionId::ELITE_PRECISION));
    simulation.stop_unit(boat);
    theater.run(40);
    verify(simulation.sea_delivery_refusal(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 0).empty(),
           "a ship nobody can see does not give itself away by blocking the lane");
}

TEST(the_ai_sends_a_squad_to_take_a_port_and_holds_it) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    simulation.begin_match(theater.player_post, theater.enemy_post);
    simulation.ai_manager().configure_assault(-19500.0f, -19000.0f, -5500.0f, 19000.0f, 3, 4, 200);
    // The first wave takes whatever is ready. Fresh units arriving after it has
    // gone are the army the AI can spare for a port.
    for (int unit = 0; unit < 4; ++unit) theater.tank(-15000.0f + unit * 12.0f, -6460.0f, FactionId::MASS_WARFARE);
    theater.run(20 * 10);
    verify(simulation.ai_manager().waves_launched() >= 1, "the first wave has gone");
    for (int unit = 0; unit < 5; ++unit) theater.tank(-15000.0f + unit * 12.0f, -6420.0f, FactionId::MASS_WARFARE);
    theater.run(20 * 10);
    verify(simulation.ai_manager().port_target() != 0, "the AI picks a port to take");
    verify(simulation.ai_manager().port_squad_size() == 2, "it sends two units");
    // The neutral port is taken before the one the player holds.
    verify(simulation.ai_manager().port_target() == kNeutralPort, "a neutral port comes before an enemy one");
    for (int tick = 0; tick < 20 * 60 * 12 && simulation.port_network().port(kNeutralPort)->owner != 1; ++tick) simulation.update(50.0f);
    verify(simulation.port_network().port(kNeutralPort)->owner == 1, "the squad takes the port");
}

TEST(the_ai_ships_forces_in_through_a_port_it_holds) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    simulation.port_network().set_owner(kNeutralPort, 1);
    simulation.begin_match(theater.player_post, theater.enemy_post);
    simulation.ai_manager().configure_assault(-19500.0f, -19000.0f, -5500.0f, 19000.0f, 100, 4, 200);
    int by_sea = 0;
    for (int tick = 0; tick < 20 * 120; ++tick) {
        simulation.update(50.0f);
        for (const auto& force : simulation.inbound_forces()) {
            if (force.faction == FactionId::MASS_WARFARE && force.route_mode == static_cast<uint8_t>(Simulation::GroundTransport::SEA)) ++by_sea;
        }
    }
    verify(simulation.ai_manager().sea_orders() > 0, "the AI orders forces by sea");
    verify(by_sea > 0, "its orders are on the sea route");
}

TEST(orders_for_one_port_sail_together_in_one_ship) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    for (int order = 0; order < 3; ++order)
        verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort), "ordered");
    theater.run(20 * 25);
    int ships = 0;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == 21) ++ships;
    }
    verify(ships == 1, "three orders share one cargo ship");
    theater.run(20 * 150);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 0, "all three come ashore");
    int tanks = 0;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == static_cast<int>(UnitType::ELITE_MAIN_BATTLE_TANK)) ++tanks;
    }
    verify(tanks == 3, "three tanks are ashore");
}

TEST(a_ship_holds_four_orders_and_a_fifth_takes_the_next_ship) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    for (int order = 0; order < 5; ++order)
        verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort), "ordered");
    theater.run(20 * 40);
    int ships = 0;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == 21) ++ships;
    }
    verify(ships == 2, "four orders fill a ship and the fifth sails in a second");
}

TEST(a_sunk_convoy_ship_loses_every_order_aboard_and_counts_once) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    for (int order = 0; order < 3; ++order)
        verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort), "ordered");
    theater.run(20 * 25);
    EntityId ship = INVALID_ENTITY;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == 21) ship = id;
    }
    verify(ship != INVALID_ENTITY, "the ship sails");
    simulation.destroy_unit(ship);
    theater.run(20 * 5);
    verify(simulation.ships_lost(FactionId::ELITE_PRECISION) == 1, "one ship is lost");
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 0, "all three orders are gone with it");
}

TEST(fuel_comes_by_sea_through_a_held_port_and_a_sunk_ship_loses_it) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    const auto fuel = [&]() { return simulation.production_manager().storages().at(theater.player_post).tactical.fuel.current; };
    const float fuel_before = fuel();
    verify(!simulation.request_sea_fuel_shipment(FactionId::MASS_WARFARE), "a side with no port cannot");
    verify(simulation.request_sea_fuel_shipment(FactionId::ELITE_PRECISION), "a side with a port can");
    verify(simulation.fuel_shipment(FactionId::ELITE_PRECISION) != nullptr, "the shipment is inbound");
    theater.run(20 * 150);
    verify(simulation.fuel_shipment(FactionId::ELITE_PRECISION) == nullptr, "the voyage is over");
    verify(fuel() >= fuel_before + 1999.0f, "the fuel is delivered");
    const float fuel_after = fuel();
    verify(simulation.request_sea_fuel_shipment(FactionId::ELITE_PRECISION), "a second shipment is accepted");
    theater.run(20 * 5);
    EntityId ship = INVALID_ENTITY;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == 21) ship = id;
    }
    verify(ship != INVALID_ENTITY, "the fuel ship sails");
    simulation.destroy_unit(ship);
    theater.run(20 * 150);
    verify(fuel() <= fuel_after + 0.5f, "a sunk fuel ship delivers nothing");
    verify(simulation.fuel_supply_reason(FactionId::ELITE_PRECISION) == "FUEL SHIPMENT SUNK", "the loss is reported");
}

TEST(a_sea_fuel_shipment_is_a_recorded_command) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    verify(simulation.issue_fuel_shipment_command(FactionId::ELITE_PRECISION, true) == 1, "the command is accepted");
    theater.run(2);
    verify(simulation.fuel_shipment(FactionId::ELITE_PRECISION) != nullptr, "the command sends the shipment");
}

TEST(a_patrol_boat_is_built_at_a_held_port_and_launched_at_its_berth) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    const auto boat = UnitType::ELITE_PATROL_BOAT;
    verify(simulation.naval_order_refusal(FactionId::MASS_WARFARE, static_cast<UnitType>(24)) == "NO PORT HELD", "no port, no boat");
    verify(simulation.naval_order_refusal(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK) == "UNKNOWN WARSHIP", "a tank is not a warship");
    verify(simulation.naval_order_refusal(FactionId::ELITE_PRECISION, boat).empty(), "a side with a port can order one");
    const float before = simulation.production_manager().storages().at(theater.player_post).tactical.materials.current;
    verify(simulation.issue_naval_command(FactionId::ELITE_PRECISION, boat) == 1, "the order is a recorded command");
    theater.run(2);
    verify(simulation.production_manager().storages().at(theater.player_post).tactical.materials.current < before, "it is paid for at once");
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 1, "the boat is being built");
    theater.run(20 * 20);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 0, "the boat is launched");
    bool afloat = false;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (data && data->unit_type == static_cast<int>(boat) &&
            std::hypot(simulation.get_unit_x(id) - kBerthX, simulation.get_unit_y(id) - 3000.0f) < 300.0f) afloat = true;
    }
    verify(afloat, "it is on the water beside the home port's berth");
}

TEST(a_warship_is_never_carried_as_cargo) {
    PortTheater theater;
    auto& simulation = theater.simulation;
    theater.run(1);
    verify(simulation.order_naval_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_PATROL_BOAT), "a boat is ordered");
    verify(simulation.order_sea_delivery(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, kHomePort), "and a tank by sea");
    theater.run(20 * 25);
    int ships = 0, boats = 0;
    for (const auto id : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(id);
        if (!data) continue;
        ships += data->unit_type == 21 ? 1 : 0;
        boats += data->unit_type == static_cast<int>(UnitType::ELITE_PATROL_BOAT) ? 1 : 0;
    }
    verify(boats == 1, "the boat was launched on its own");
    verify(ships == 1, "the tank sails in its own cargo ship");
}

TEST(each_faction_has_a_patrol_boat_to_contest_a_lane) {
    for (const auto faction : {FactionId::ELITE_PRECISION, FactionId::MASS_WARFARE, FactionId::INDUSTRIAL_EXPERIMENTAL}) {
        int boats = 0;
        for (const auto& [type, prototype] : get_unit_prototypes()) {
            if (prototype.faction == faction && prototype.is_naval && !prototype.cargo_transport && prototype.weapon.damage > 0.0f) ++boats;
        }
        verify(boats >= 1, "every faction has an armed warship");
    }
}
