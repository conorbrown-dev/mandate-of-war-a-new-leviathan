#include <cmath>
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

    // The load flies in on a cargo aircraft from the west map edge.
    const auto* shipment = theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION);
    verify(shipment && shipment->transport != INVALID_ENTITY, "a cargo aircraft carries it");
    verify(theater.simulation.get_unit_x(shipment->transport) < theater.simulation.pathfinding().min_world_x_center(), "starting off the west map edge");
    const float flight_ms = shipment->total_ms;
    verify(flight_ms > 100000.0f, "so the flight takes as long as the trip from the edge");
    theater.run(static_cast<int>(flight_ms / 50.0f) - 40);
    verify(theater.fuel(theater.player_base) == 0.0f, "fuel does not arrive early");
    for (int tick = 0; tick < 20 * 10 && theater.fuel(theater.player_base) == 0.0f; ++tick) theater.run(1);
    verify(theater.fuel(theater.player_base) == kFuelShipmentLoad, "the whole load arrives");
    verify(theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION) == nullptr, "the flight is over");
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "another shipment can follow");
}

TEST(fuel_shipment_is_lost_without_a_surviving_airfield) {
    FuelTheater theater;
    theater.add_airfield();
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "airfield accepts a shipment");
    theater.simulation.territorial_control_manager().remove_installation(-1000.0f, 0.0f, InstallationType::AIRFIELD);
    for (int tick = 0; tick < 20 * 400 && theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION); ++tick) theater.run(1);
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

TEST(key_site_victory_requires_a_continuous_hold_and_keeps_posts_intact) {
    FuelTheater theater;
    auto& sim = theater.simulation;
    auto& production = sim.production_manager();
    for (EntityId id : {EntityId{911}, EntityId{912}})
        production.add_resource_node(id, ResourceNode{600.0f + float(id - 911) * 100.0f, 0.0f,
            1500.0f, 1500.0f, ResourceNode::Type::ENERGY, false});
    sim.begin_match(theater.player_base, theater.enemy_base);
    verify(!sim.configure_key_site_victory({910, 910, 911}, 2, 3), "duplicate key sites are rejected");
    verify(!sim.configure_key_site_victory({910, 911, 999}, 2, 3), "missing key sites are rejected");
    verify(sim.configure_key_site_victory({910, 911, 912}, 2, 3), "three real sites configure the hold rule");
    verify(sim.assign_fuel_depot(910, FactionId::ELITE_PRECISION) &&
           sim.assign_fuel_depot(911, FactionId::ELITE_PRECISION), "player holds two sites");
    theater.run(2);
    verify(sim.match_result() == -1 && sim.key_sites_held(FactionId::ELITE_PRECISION) == 2 &&
           sim.key_site_hold_ticks(FactionId::ELITE_PRECISION) == 2, "hold is counted by native ticks");
    verify(sim.assign_fuel_depot(911, FactionId::MASS_WARFARE), "enemy interrupts the hold");
    theater.run(1);
    verify(sim.match_result() == -1 && sim.key_site_hold_ticks(FactionId::ELITE_PRECISION) == 0,
           "losing a key site resets the timer");
    verify(sim.assign_fuel_depot(911, FactionId::ELITE_PRECISION), "player retakes the site");
    theater.run(2);
    verify(sim.match_result() == -1, "the previous partial hold cannot win");
    theater.run(1);
    verify(sim.match_result() == 0 && sim.match_victory_reason() == 1, "continuous hold wins the match");
    verify(!sim.get_unit_is_dead(theater.player_base) && !sim.get_unit_is_dead(theater.enemy_base),
           "site victory does not destroy either command post");
}

TEST(key_site_victory_can_lose_and_command_post_loss_takes_precedence) {
    FuelTheater enemy;
    auto& sim = enemy.simulation;
    sim.production_manager().add_resource_node(911,
        ResourceNode{600.0f, 0.0f, 1500.0f, 1500.0f, ResourceNode::Type::ENERGY, false});
    sim.begin_match(enemy.player_base, enemy.enemy_base);
    verify(sim.configure_key_site_victory({910, 911}, 2, 3), "enemy victory rule configures");
    verify(sim.assign_fuel_depot(910, FactionId::MASS_WARFARE) &&
           sim.assign_fuel_depot(911, FactionId::MASS_WARFARE), "enemy holds the sites");
    enemy.run(3);
    verify(sim.match_result() == 1 && sim.match_victory_reason() == 1, "enemy hold causes defeat");

    FuelTheater posts;
    auto& other = posts.simulation;
    other.production_manager().add_resource_node(911,
        ResourceNode{600.0f, 0.0f, 1500.0f, 1500.0f, ResourceNode::Type::ENERGY, false});
    other.begin_match(posts.player_base, posts.enemy_base);
    verify(other.configure_key_site_victory({910, 911}, 2, 1), "post-priority rule configures");
    verify(other.assign_fuel_depot(910, FactionId::ELITE_PRECISION) &&
           other.assign_fuel_depot(911, FactionId::ELITE_PRECISION), "player holds the sites");
    other.apply_damage(posts.player_base, 100000.0f);
    posts.run(1);
    verify(other.match_result() == 1 && other.match_victory_reason() == 0,
           "destroyed command post takes precedence over site hold");
}

TEST(a_fuel_shipment_shot_down_on_the_way_never_arrives) {
    for (const bool defended : {false, true}) {
        FuelTheater theater;
        theater.add_airfield();
        theater.simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, kStartMaterials, 0.0f);
        if (defended) {
            // A line of enemy anti-air under the flight path, which runs along y = 0.
            for (int index = 0; index < 8; ++index) {
                const EntityId gun = static_cast<EntityId>(theater.simulation.create_unit_with_type(
                    -9000.0f + static_cast<float>(index) * 60.0f, 30.0f, UnitType::MASS_ANTI_AIR, FactionId::MASS_WARFARE));
                theater.simulation.stop_unit(gun);
            }
        }
        verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "the shipment is ordered");
        for (int tick = 0; tick < 20 * 400 && theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION); ++tick) theater.run(1);
        if (defended) {
            verify(theater.fuel(theater.player_base) == 0.0f, "anti-air under the route shoots the cargo aircraft down");
            verify(theater.simulation.fuel_supply_reason(FactionId::ELITE_PRECISION) == "FUEL SHIPMENT SHOT DOWN", "and the player is told");
        } else {
            verify(theater.fuel(theater.player_base) == kFuelShipmentLoad, "an unopposed shipment arrives");
        }
    }
}

TEST(a_cargo_aircraft_cannot_be_diverted) {
    FuelTheater theater;
    theater.add_airfield();
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "the shipment is ordered");
    const EntityId cargo = theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION)->transport;
    theater.simulation.issue_move_commands({cargo}, FactionId::ELITE_PRECISION, -15000.0f, 15000.0f, 12.0f);
    theater.run(20 * 20);
    verify(std::abs(theater.simulation.get_unit_y(cargo)) < 50.0f, "a move order does not turn it off its route");
}

namespace {
size_t shots_by(Simulation& simulation, FactionId faction) {
    size_t count = 0;
    for (const auto& shot : simulation.combat_manager().projectile_manager().projectiles()) {
        count += shot.active && shot.faction_id == faction ? 1 : 0;
    }
    return count;
}

float health_of(Simulation& simulation, EntityId unit) {
    float current = 0.0f, maximum = 0.0f;
    simulation.get_unit_health(unit, current, maximum);
    return current;
}
}

TEST(tank_guns_cannot_engage_an_aircraft_in_flight) {
    FuelTheater theater;
    theater.add_airfield();
    theater.simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, kStartMaterials, 0.0f);
    // The same line as the anti-air test, but main battle tanks.
    std::vector<EntityId> tanks;
    for (int index = 0; index < 8; ++index) {
        tanks.push_back(static_cast<EntityId>(theater.simulation.create_unit_with_type(
            -9000.0f + static_cast<float>(index) * 60.0f, 30.0f, UnitType::MASS_ASSAULT_VEHICLE, FactionId::MASS_WARFARE)));
        theater.simulation.stop_unit(tanks.back());
    }
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "the shipment is ordered");
    const EntityId cargo = theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION)->transport;
    float lowest = health_of(theater.simulation, cargo);
    size_t shots = 0;
    bool ordered = false;
    for (int tick = 0; tick < 20 * 400 && theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION); ++tick) {
        if (!ordered && std::abs(theater.simulation.get_unit_x(cargo) + 8800.0f) < 300.0f) {
            // Even ordered onto it, a tank gun cannot engage a plane.
            theater.simulation.issue_attack_commands(tanks, FactionId::MASS_WARFARE, cargo);
            ordered = true;
        }
        theater.run(1);
        shots += shots_by(theater.simulation, FactionId::MASS_WARFARE);
        if (theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION)) lowest = std::min(lowest, health_of(theater.simulation, cargo));
    }
    verify(shots == 0, "the tanks do not even fire at it");
    verify(ordered, "the cargo aircraft passes over the tanks");
    verify(lowest == get_unit_prototypes().at(static_cast<UnitType>(17)).hp, "it takes no damage from them");
    verify(theater.fuel(theater.player_base) == kFuelShipmentLoad, "and the fuel arrives");
    verify(theater.simulation.combat_manager().explicit_attack_targets().empty(), "the attack order on it was refused");
}

TEST(a_surface_to_air_missile_cannot_engage_a_ground_target) {
    FuelTheater theater;
    const EntityId sam = static_cast<EntityId>(theater.simulation.create_unit_with_type(0.0f, 500.0f, UnitType::MASS_ANTI_AIR, FactionId::MASS_WARFARE));
    const EntityId tank = static_cast<EntityId>(theater.simulation.create_unit_with_type(40.0f, 500.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
    theater.simulation.stop_unit(sam);
    theater.simulation.stop_unit(tank);
    const float tank_health = health_of(theater.simulation, tank);
    const float sam_health = health_of(theater.simulation, sam);
    size_t sam_shots = 0;
    for (int tick = 0; tick < 20 * 10; ++tick) {
        theater.run(1);
        sam_shots += shots_by(theater.simulation, FactionId::MASS_WARFARE);
    }
    verify(sam_shots == 0, "a Buk does not fire on a tank");
    verify(health_of(theater.simulation, tank) == tank_health, "a Buk beside a tank does it no harm");
    verify(health_of(theater.simulation, sam) < sam_health, "while the tank fires on the Buk");
}

TEST(a_ground_shot_passes_under_an_aircraft_in_flight) {
    FuelTheater theater;
    theater.add_airfield();
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "a cargo aircraft is in the air");
    const EntityId cargo = theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION)->transport;
    theater.run(20 * 30);
    const float x = theater.simulation.get_unit_x(cargo);
    const float y = theater.simulation.get_unit_y(cargo);
    const float before = health_of(theater.simulation, cargo);
    // A ground target further along the line, which the shell must still reach.
    const EntityId behind = static_cast<EntityId>(theater.simulation.create_unit_with_type(x - 250.0f, y, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
    theater.simulation.stop_unit(behind);
    auto& shots = theater.simulation.combat_manager().projectile_manager();
    // A shell fired straight through the aircraft's position, ahead of it.
    shots.spawn(x + 400.0f, y, -1.0f, 0.0f, 300.0f, 500.0f, 5.0f, 3.0f, FactionId::MASS_WARFARE, true, false);
    shots.spawn(x + 400.0f, y + 0.5f, -1.0f, 0.0f, 300.0f, 1.0f, 5.0f, 3.0f, FactionId::MASS_WARFARE, false, true);
    // 650 m at 300 m/s, inside the shells' three-second lifetime.
    theater.run(20 * 3);
    verify(before - health_of(theater.simulation, cargo) <= 1.0f, "the ground shell does nothing to it; only the air shot hits");
    verify(before - health_of(theater.simulation, cargo) > 0.0f, "the air shot along the same line does hit");
    verify(health_of(theater.simulation, behind) <= 0.0f, "and the shell flies on past it to the ground target behind");
}

TEST(a_ground_burst_does_not_harm_an_aircraft_overhead) {
    FuelTheater theater;
    theater.add_airfield();
    verify(theater.simulation.request_fuel_shipment(FactionId::ELITE_PRECISION), "a cargo aircraft is in the air");
    const EntityId cargo = theater.simulation.fuel_shipment(FactionId::ELITE_PRECISION)->transport;
    theater.run(20 * 30);
    const float x = theater.simulation.get_unit_x(cargo);
    const float y = theater.simulation.get_unit_y(cargo);
    // A tank directly under the aircraft, and a big shell bursting on it.
    const EntityId tank = static_cast<EntityId>(theater.simulation.create_unit_with_type(x, y, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
    theater.simulation.stop_unit(tank);
    const float cargo_before = health_of(theater.simulation, cargo);
    const float tank_before = health_of(theater.simulation, tank);
    theater.simulation.combat_manager().projectile_manager().spawn(
        x + 6.0f, y, -1.0f, 0.0f, 300.0f, 40.0f, 30.0f, 3.0f, FactionId::MASS_WARFARE, true, false);
    theater.run(2);
    verify(health_of(theater.simulation, tank) < tank_before, "the burst hits the tank");
    verify(health_of(theater.simulation, cargo) == cargo_before, "but not the aircraft flying over it");
}
