#include "test_framework.hpp"
#include "simulation/simulation.hpp"

using namespace rts;

namespace {
constexpr EntityId kDepot = 910;
constexpr float kDepotStock = 1500.0f;
constexpr float kDepotX = 500.0f;
constexpr float kStartMaterials = 1000.0f;

void verify(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

struct FuelTheater {
    Simulation simulation;
    EntityId player_base = INVALID_ENTITY;
    EntityId enemy_base = INVALID_ENTITY;
    EntityId engineer = INVALID_ENTITY;

    FuelTheater() {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.start();
        simulation.enable_ai(false);
        player_base = simulation.create_faction_base(FactionId::ELITE_PRECISION, -2000.0f, 0.0f);
        enemy_base = simulation.create_faction_base(FactionId::MASS_WARFARE, 2000.0f, 0.0f);
        engineer = simulation.create_unit(0.0f, 0.0f).id;
        simulation.set_unit_faction(engineer, FactionId::ELITE_PRECISION);
        simulation.production_manager().add_resource_node(kDepot,
            ResourceNode{kDepotX, 0.0f, kDepotStock, kDepotStock, ResourceNode::Type::ENERGY, false});
        simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, kStartMaterials, 0.0f);
    }

    ~FuelTheater() { simulation.stop(); }

    float fuel(EntityId base) { return simulation.production_manager().tactical_resources(base)->fuel.current; }
    float materials(EntityId base) { return simulation.production_manager().tactical_resources(base)->materials.current; }
    float stock() { return simulation.production_manager().resource_nodes().at(kDepot).amount; }
    int owner() { return simulation.fuel_depots().at(0).owner; }
    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
    bool order_capture() {
        return simulation.issue_harvest_commands({engineer}, FactionId::ELITE_PRECISION, kDepotX, 0.0f) == 1;
    }
    void run_until_captured() {
        for (int tick = 0; tick < 4000 && owner() != 0; ++tick) simulation.update(50.0f);
    }
    void add_airfield() {
        simulation.territorial_control_manager().add_installation(-1000.0f, 0.0f,
            InstallationType::AIRFIELD, FactionId::ELITE_PRECISION);
    }
};
}

TEST(unclaimed_fuel_depot_yields_nothing_on_its_own) {
    FuelTheater theater;
    theater.run(200);
    verify(theater.stock() == kDepotStock, "an unclaimed depot keeps its stock");
    verify(theater.fuel(theater.player_base) == 0.0f, "no fuel appears without capture or shipment");
    verify(theater.owner() == -1, "an unclaimed depot has no owner");
}

TEST(fuel_depot_is_captured_on_arrival_not_on_order) {
    FuelTheater theater;
    verify(theater.order_capture(), "capture order is accepted");
    theater.run(2);
    verify(theater.owner() == -1, "the depot is not captured from 500 m away");
    verify(theater.simulation.fuel_depots().at(0).capturing, "the depot shows a capture in progress");
    verify(theater.stock() == kDepotStock, "stock does not move during the approach");

    theater.run_until_captured();
    verify(theater.owner() == 0, "the unit captures the depot when it arrives");
    const float engineer_gap = std::abs(theater.simulation.get_unit_x(theater.engineer) - kDepotX);
    verify(engineer_gap <= kFuelDepotCaptureRadius + 5.0f, "capture happens at the depot");
}

TEST(captured_fuel_depot_moves_its_finite_stock_and_runs_dry) {
    FuelTheater theater;
    verify(theater.order_capture(), "capture order is accepted");
    theater.run_until_captured();
    theater.run(40);
    const float moved = theater.fuel(theater.player_base);
    verify(moved > 0.0f, "stock reaches the fuel store");
    verify(std::abs(moved + theater.stock() - kDepotStock) < 0.01f, "fuel moved equals stock lost");

    theater.run(static_cast<int>(kDepotStock / kFuelDepotDrawPerTick) + 10);
    verify(theater.stock() == 0.0f, "the depot runs dry");
    const float final_fuel = theater.fuel(theater.player_base);
    verify(std::abs(final_fuel - kDepotStock) < 0.01f, "the whole stock was delivered once");
    theater.run(100);
    verify(theater.fuel(theater.player_base) == final_fuel, "an empty depot never refills");
}

TEST(enemy_fuel_depot_keeps_its_stock_until_it_is_taken) {
    FuelTheater theater;
    verify(theater.simulation.assign_fuel_depot(kDepot, FactionId::MASS_WARFARE), "enemy holds the depot");
    theater.run(100);
    verify(theater.owner() == 1, "the enemy owns the depot");
    verify(theater.stock() == kDepotStock, "a held depot keeps its stock");
    verify(theater.fuel(theater.enemy_base) == 0.0f, "a held depot does not fill the enemy store");

    verify(theater.order_capture(), "capture order is accepted");
    theater.run_until_captured();
    theater.run(40);
    verify(theater.owner() == 0, "the player takes the enemy depot");
    verify(theater.fuel(theater.player_base) > 0.0f, "captured enemy stock reaches the player");
    verify(theater.fuel(theater.enemy_base) == 0.0f, "the enemy gets none of it");
}

TEST(redirected_unit_abandons_a_fuel_capture) {
    FuelTheater redirected;
    verify(redirected.order_capture(), "capture order is accepted");
    redirected.run(2);
    redirected.simulation.move_unit(redirected.engineer, -300.0f, 0.0f);
    redirected.run(400);
    verify(redirected.owner() == -1, "a redirected unit does not capture the depot");
    verify(!redirected.simulation.fuel_depots().at(0).capturing, "the capture is abandoned");
}

TEST(dead_unit_abandons_a_fuel_capture) {
    FuelTheater killed;
    verify(killed.order_capture(), "capture order is accepted");
    killed.run(2);
    killed.simulation.component_manager().get_component<Health>(killed.engineer)->is_dead = true;
    killed.run(2);
    verify(!killed.simulation.fuel_depots().at(0).capturing, "a dead unit captures nothing");
    verify(killed.owner() == -1, "a dead unit leaves the depot unclaimed");
}

TEST(fuel_depot_can_only_be_demolished_on_site) {
    FuelTheater theater;
    verify(!theater.simulation.destroy_resource_site(theater.engineer, kDepotX, 0.0f),
        "a distant unit cannot demolish a depot");
    verify(theater.simulation.fuel_depots().size() == 1, "the depot survives a distant order");
    verify(theater.order_capture(), "capture order is accepted");
    theater.run_until_captured();
    verify(theater.simulation.destroy_resource_site(theater.engineer, kDepotX, 0.0f),
        "a unit at the depot can demolish it");
    verify(theater.simulation.fuel_depots().empty(), "the depot and its stock are gone");
}

TEST(fuel_shipment_needs_an_airfield_and_arrives_after_its_flight) {
    FuelTheater theater;
    verify(!theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "no airfield, no shipment");
    verify(theater.materials(theater.player_base) == kStartMaterials, "a refused shipment costs nothing");

    theater.add_airfield();
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "airfield accepts a shipment");
    verify(theater.materials(theater.player_base) == kStartMaterials - kFuelShipmentMaterialCost, "the shipment costs Materials");
    verify(!theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "one shipment at a time");
    verify(theater.materials(theater.player_base) == kStartMaterials - kFuelShipmentMaterialCost, "a second request costs nothing");

    theater.run(static_cast<int>(kFuelShipmentDurationMs / 50.0f) - 2);
    verify(theater.fuel(theater.player_base) == 0.0f, "fuel does not arrive early");
    theater.run(4);
    verify(theater.fuel(theater.player_base) == kFuelShipmentLoad, "the whole load arrives");
    verify(theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION) == nullptr, "the flight is over");
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "another shipment can follow");
}

TEST(fuel_shipment_is_lost_without_a_surviving_airfield) {
    FuelTheater theater;
    theater.add_airfield();
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "airfield accepts a shipment");
    theater.simulation.territorial_control_manager().remove_installation(-1000.0f, 0.0f, InstallationType::AIRFIELD);
    theater.run(static_cast<int>(kFuelShipmentDurationMs / 50.0f) + 2);
    verify(theater.fuel(theater.player_base) == 0.0f, "no airfield means no delivery");
    verify(theater.simulation.fuel_supply_reason(FactionId::ELITE_PRECISION).find("LOST") != std::string::npos,
        "the player is told the shipment was lost");
}

TEST(fuel_shipment_is_refused_when_storage_cannot_hold_it) {
    FuelTheater theater;
    theater.add_airfield();
    theater.simulation.production_manager().update_storage(theater.player_base, 0.0f, 9000.0f);
    verify(!theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "a full store refuses a shipment");
    verify(theater.materials(theater.player_base) == kStartMaterials, "a refused shipment costs nothing");
}

TEST(unit_inside_a_large_footprint_walks_out) {
    FuelTheater theater;
    theater.simulation.pathfinding().block_world_rectangle(0.0f, 0.0f, 300.0f, 900.0f);
    verify(theater.order_capture(), "capture order is accepted from inside the footprint");
    theater.run_until_captured();
    verify(theater.owner() == 0, "the unit leaves the footprint and captures the depot");
}

TEST(demolishing_a_site_leaves_other_claimed_sites_with_their_owners) {
    FuelTheater theater;
    auto& production = theater.simulation.production_manager();
    production.add_resource_node(900, ResourceNode{-50.0f, 0.0f, 1000.0f, 1000.0f, ResourceNode::Type::METAL, false});
    production.add_resource_node(901, ResourceNode{-60.0f, 200.0f, 1000.0f, 1000.0f, ResourceNode::Type::METAL, false});
    production.add_resource_node(911, ResourceNode{-70.0f, 400.0f, 1000.0f, 1000.0f, ResourceNode::Type::ENERGY, false});
    verify(theater.simulation.assign_fuel_depot(kDepot, FactionId::MASS_WARFARE), "enemy holds the first depot");
    verify(theater.simulation.assign_fuel_depot(911, FactionId::MASS_WARFARE), "enemy holds the second depot");
    verify(theater.simulation.issue_harvest_commands({theater.engineer}, FactionId::ELITE_PRECISION, -50.0f, 0.0f) == 1, "first site claimed");
    theater.run(1);
    verify(theater.simulation.destroy_resource_site(theater.enemy_base, -50.0f, 0.0f), "first site demolished");
    verify(theater.simulation.issue_harvest_commands({theater.engineer}, FactionId::ELITE_PRECISION, -60.0f, 200.0f) == 1, "second site claimed");
    theater.run(1);
    const auto depots = theater.simulation.fuel_depots();
    verify(depots.size() == 2 && depots[0].owner == 1 && depots[1].owner == 1, "a new claim keeps both depots with their owner");
}
