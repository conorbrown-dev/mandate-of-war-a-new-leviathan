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
