#include <cmath>
#include "test_framework.hpp"
#include "simulation/simulation.hpp"

using namespace rts;

namespace {
constexpr uint32_t kStation = 1;
constexpr uint32_t kJunction = 2;
constexpr uint32_t kPlayerLoad = 3;
constexpr uint32_t kEnemyLoad = 4;
constexpr uint32_t kEnemyStation = 5;
constexpr uint32_t kTrunk = 10;      // steel, civilian: station to junction
constexpr uint32_t kPlayerFeed = 11; // wood, player's: junction to the player's post
constexpr uint32_t kEnemyFeed = 12;  // wood, enemy's: junction to the enemy's post
constexpr float kStationOutput = 120.0f;
constexpr float kPostDemand = 40.0f;
constexpr float kStartMaterials = 1000.0f;

void verify(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

// A civilian station at x=0 feeds a junction at x=1000; wooden lines run from
// there to the player's post (west, y=-500) and the enemy's (east, y=+500).
struct PowerTheater {
    Simulation simulation;
    EntityId player_base = INVALID_ENTITY;
    EntityId enemy_base = INVALID_ENTITY;
    EntityId engineer = INVALID_ENTITY;
    EntityId tank = INVALID_ENTITY;
    EntityId raider = INVALID_ENTITY;

    PowerTheater() {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.start();
        simulation.enable_ai(false);
        player_base = simulation.create_faction_base(FactionId::ELITE_PRECISION, -2000.0f, -500.0f);
        enemy_base = simulation.create_faction_base(FactionId::MASS_WARFARE, 4000.0f, 500.0f);
        engineer = static_cast<EntityId>(simulation.create_unit_with_type(
            900.0f, -400.0f, UnitType::INDUSTRIAL_ENGINEERING, FactionId::ELITE_PRECISION));
        tank = static_cast<EntityId>(simulation.create_unit_with_type(
            900.0f, -300.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
        raider = static_cast<EntityId>(simulation.create_unit_with_type(
            1100.0f, -300.0f, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE));
        simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, kStartMaterials, 0.0f);
        simulation.set_reinforcement_resources(FactionId::MASS_WARFARE, kStartMaterials, 0.0f);

        PowerGrid& grid = simulation.power_grid();
        verify(grid.add_node({kStation, PowerNodeKind::STATION, 0.0f, 0.0f, -1, kStationOutput, 0.0f}), "station");
        verify(grid.add_node({kJunction, PowerNodeKind::JUNCTION, 1000.0f, 0.0f, -1, 0.0f, 0.0f}), "junction");
        verify(grid.add_node({kPlayerLoad, PowerNodeKind::LOAD, -2000.0f, -500.0f, 0, 0.0f, kPostDemand}), "player load");
        verify(grid.add_node({kEnemyLoad, PowerNodeKind::LOAD, 4000.0f, 500.0f, 1, 0.0f, kPostDemand}), "enemy load");
        verify(grid.add_line(kTrunk, kStation, kJunction, PowerLineKind::STEEL, -1), "trunk");
        verify(grid.add_line(kPlayerFeed, kJunction, kPlayerLoad, PowerLineKind::WOOD, 0, {{1000.0f, -500.0f}}), "player feed");
        verify(grid.add_line(kEnemyFeed, kJunction, kEnemyLoad, PowerLineKind::WOOD, 1, {{1000.0f, 500.0f}}), "enemy feed");
    }

    ~PowerTheater() { simulation.stop(); }

    const Power& power(EntityId base) { return simulation.production_manager().tactical_resources(base)->power; }
    float materials(EntityId base) { return simulation.production_manager().tactical_resources(base)->materials.current; }
    const PowerLine& line(uint32_t id) { return *simulation.power_grid().line(id); }
    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
    bool order(EntityId unit, FactionId side, Simulation::PowerOrder work, uint32_t target) {
        return simulation.issue_power_command(unit, side, work, target) == 1;
    }
    template <typename Done> bool run_until(Done done, int limit = 6000) {
        for (int tick = 0; tick < limit && !done(); ++tick) simulation.update(50.0f);
        return done();
    }
    float gap_to_line(EntityId unit, uint32_t id) {
        return simulation.power_grid().distance_to_line(id, simulation.get_unit_x(unit), simulation.get_unit_y(unit));
    }
};
}

TEST(power_reaches_each_side_through_an_unbroken_line_from_a_station) {
    PowerTheater theater;
    theater.run(2);
    verify(theater.power(theater.player_base).generation_mw == kStationOutput, "the player draws the station's output");
    verify(theater.power(theater.player_base).demand_mw == kPostDemand, "the player's post is the demand");
    verify(theater.power(theater.enemy_base).generation_mw == kStationOutput, "a civilian station feeds both sides");
    verify(!theater.power(theater.player_base).in_deficit(), "a supplied side has a reserve");
    verify(theater.simulation.power_grid().load_powered(kPlayerLoad), "the player's post is powered");
}

TEST(a_side_with_no_grid_keeps_the_power_state_it_was_given) {
    Simulation simulation;
    simulation.configure_world_size(40000.0f, 40000.0f);
    simulation.start();
    simulation.enable_ai(false);
    const EntityId base = simulation.create_faction_base(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    simulation.production_manager().set_tactical_power(base, 12.0f, 3.0f);
    for (int tick = 0; tick < 5; ++tick) simulation.update(50.0f);
    verify(simulation.production_manager().tactical_resources(base)->power.generation_mw == 12.0f,
        "a match without a grid does not touch Power");
    simulation.stop();
}

TEST(power_line_is_cut_on_arrival_and_the_side_loses_its_power) {
    PowerTheater theater;
    verify(theater.order(theater.raider, FactionId::MASS_WARFARE, Simulation::PowerOrder::SEVER_LINE, kPlayerFeed),
        "the enemy is sent to cut the player's line");
    theater.run(2);
    verify(theater.line(kPlayerFeed).intact, "the line is not cut from a distance");
    verify(theater.gap_to_line(theater.raider, kPlayerFeed) > kPowerWorkRadius, "the raider starts away from the line");
    verify(theater.run_until([&] { return !theater.line(kPlayerFeed).intact; }), "the raider cuts the line when it arrives");
    verify(theater.gap_to_line(theater.raider, kPlayerFeed) <= kPowerWorkRadius, "the cut happens at the line");
    theater.run(2);
    verify(theater.power(theater.player_base).generation_mw == 0.0f, "the player loses power income");
    verify(theater.power(theater.player_base).in_deficit(), "the player's post is now in deficit");
    verify(!theater.simulation.power_grid().load_powered(kPlayerLoad), "the player's post is dark");
    verify(theater.power(theater.enemy_base).generation_mw == kStationOutput, "the enemy keeps its own supply");
    const PowerLine& cut = theater.line(kPlayerFeed);
    verify(theater.simulation.power_grid().distance_to_line(kPlayerFeed, cut.cut_x, cut.cut_y) < 0.01f,
        "the break is reported at a point on the line");
}

TEST(only_an_engineer_repairs_a_power_line_and_it_costs_materials) {
    PowerTheater theater;
    PowerGrid& grid = theater.simulation.power_grid();
    verify(!theater.order(theater.engineer, FactionId::ELITE_PRECISION, Simulation::PowerOrder::REPAIR_LINE, kPlayerFeed),
        "an intact line needs no repair");
    verify(grid.sever_line(kPlayerFeed, 0.0f, -500.0f), "the line is cut a kilometre from the engineer");
    verify(!theater.order(theater.tank, FactionId::ELITE_PRECISION, Simulation::PowerOrder::REPAIR_LINE, kPlayerFeed),
        "a tank cannot repair a line");
    verify(theater.simulation.power_order_refusal(theater.tank, Simulation::PowerOrder::REPAIR_LINE, kPlayerFeed) ==
        "ENGINEER REQUIRED TO REPAIR POWER LINES", "the refusal says why");
    verify(theater.order(theater.engineer, FactionId::ELITE_PRECISION, Simulation::PowerOrder::REPAIR_LINE, kPlayerFeed),
        "the engineer is sent to repair it");
    theater.run(2);
    verify(theater.materials(theater.player_base) == kStartMaterials, "nothing is paid before the engineer arrives");
    verify(theater.run_until([&] { return theater.line(kPlayerFeed).repair_remaining_ms > 0.0f; }), "the repair starts on arrival");
    verify(std::hypot(theater.simulation.get_unit_x(theater.engineer) - theater.line(kPlayerFeed).cut_x,
                      theater.simulation.get_unit_y(theater.engineer) - theater.line(kPlayerFeed).cut_y) <= kPowerWorkRadius + 20.0f,
        "the engineer works at the break, not anywhere along the line");
    const float cost = grid.repair_cost(kPlayerFeed);
    verify(cost > 0.0f && std::abs(theater.materials(theater.player_base) - (kStartMaterials - cost)) < 0.01f,
        "the repair is paid for once, in Materials");
    verify(!theater.line(kPlayerFeed).intact, "the line stays down while it is repaired");
    theater.run(2);
    verify(theater.power(theater.player_base).generation_mw == 0.0f, "no power flows during the repair");
    theater.run(static_cast<int>(grid.repair_seconds(kPlayerFeed) * 20.0f) + 5);
    verify(theater.line(kPlayerFeed).intact, "the repair finishes after its build time");
    verify(theater.power(theater.player_base).generation_mw == kStationOutput, "power income returns");
    verify(std::abs(theater.materials(theater.player_base) - (kStartMaterials - cost)) < 0.01f, "it is not charged twice");
}

TEST(wooden_power_lines_cost_less_than_steel_ones) {
    PowerTheater theater;
    PowerGrid& grid = theater.simulation.power_grid();
    // The same 1,000 m route in each kind.
    verify(grid.add_node({20, PowerNodeKind::JUNCTION, 0.0f, 3000.0f}), "a");
    verify(grid.add_node({21, PowerNodeKind::JUNCTION, 1000.0f, 3000.0f}), "b");
    verify(grid.add_line(30, 20, 21, PowerLineKind::WOOD, -1), "wood");
    verify(grid.add_line(31, 21, 20, PowerLineKind::STEEL, -1), "steel");
    verify(grid.repair_cost(30) > 0.0f && grid.repair_cost(30) < grid.repair_cost(31), "wood is cheaper than steel");
    verify(grid.repair_seconds(30) < grid.repair_seconds(31), "wood is quicker than steel");
}

TEST(power_line_repair_is_refused_without_the_materials) {
    PowerTheater theater;
    verify(theater.simulation.power_grid().sever_line(kPlayerFeed, 1000.0f, -250.0f), "the line is cut");
    theater.simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, 5.0f, 0.0f);
    verify(!theater.order(theater.engineer, FactionId::ELITE_PRECISION, Simulation::PowerOrder::REPAIR_LINE, kPlayerFeed),
        "a side that cannot pay is refused");
    verify(theater.simulation.power_order_refusal(theater.engineer, Simulation::PowerOrder::REPAIR_LINE, kPlayerFeed) ==
        "INSUFFICIENT MATERIALS", "the refusal says why");
    theater.run(400);
    verify(!theater.line(kPlayerFeed).intact && theater.materials(theater.player_base) == 5.0f, "nothing is spent or repaired");
}

TEST(capturing_a_power_line_denies_it_to_the_side_that_held_it) {
    PowerTheater theater;
    verify(theater.order(theater.tank, FactionId::ELITE_PRECISION, Simulation::PowerOrder::CAPTURE_LINE, kEnemyFeed),
        "the player is sent to take the enemy's line");
    theater.run(2);
    verify(theater.line(kEnemyFeed).owner == 1, "the line does not change hands on the order");
    verify(theater.run_until([&] { return theater.line(kEnemyFeed).owner == 0; }), "it changes hands when the unit arrives");
    theater.run(2);
    verify(theater.line(kEnemyFeed).intact, "a captured line is not damaged");
    verify(theater.power(theater.enemy_base).generation_mw == 0.0f, "the enemy loses the power that line carried");
    verify(theater.power(theater.player_base).generation_mw == kStationOutput, "the player's own supply is unchanged");
    verify(!theater.order(theater.tank, FactionId::ELITE_PRECISION, Simulation::PowerOrder::CAPTURE_LINE, kEnemyFeed),
        "a line already held cannot be captured again");
}

TEST(a_captured_power_station_stops_feeding_the_other_side) {
    PowerTheater theater;
    verify(theater.order(theater.raider, FactionId::MASS_WARFARE, Simulation::PowerOrder::CAPTURE_STATION, kStation),
        "the enemy is sent to take the station");
    theater.run(2);
    verify(theater.simulation.power_grid().node(kStation)->owner == -1, "the station is not taken on the order");
    verify(theater.run_until([&] { return theater.simulation.power_grid().node(kStation)->owner == 1; }),
        "the station changes hands when the unit arrives");
    theater.run(2);
    verify(theater.power(theater.player_base).generation_mw == 0.0f, "the player's lines now lead to an enemy station");
    verify(theater.power(theater.enemy_base).generation_mw == kStationOutput, "the enemy keeps drawing from it");
}

TEST(power_lines_always_draw_from_a_power_station) {
    PowerTheater theater;
    PowerGrid& grid = theater.simulation.power_grid();
    // A second load joined only to a junction: lines, but no station behind them.
    verify(grid.add_node({40, PowerNodeKind::JUNCTION, -6000.0f, 6000.0f}), "dead junction");
    verify(grid.add_node({41, PowerNodeKind::LOAD, -7000.0f, 6000.0f, 2, 0.0f, 10.0f}), "third side's load");
    verify(grid.add_line(42, 40, 41, PowerLineKind::WOOD, 2), "dead line");
    verify(grid.supply(2).generation_mw == 0.0f && !grid.load_powered(41), "a line with no station carries nothing");
    verify(grid.add_node({kEnemyStation, PowerNodeKind::STATION, -6000.0f, 7000.0f, 2, 80.0f, 0.0f}), "station");
    verify(grid.add_line(43, kEnemyStation, 40, PowerLineKind::STEEL, 2), "station line");
    verify(grid.supply(2).generation_mw == 80.0f && grid.load_powered(41), "joining a station brings power");
    verify(grid.supply(0).generation_mw == kStationOutput, "another side's station adds nothing to the player");
}

TEST(a_redirected_or_dead_unit_abandons_power_line_work) {
    PowerTheater redirected;
    verify(redirected.order(redirected.raider, FactionId::MASS_WARFARE, Simulation::PowerOrder::SEVER_LINE, kPlayerFeed), "ordered");
    redirected.run(2);
    verify(redirected.simulation.power_tasks().size() == 1, "the work is pending");
    redirected.simulation.move_unit(redirected.raider, 2500.0f, 1500.0f);
    redirected.run(600);
    verify(redirected.line(kPlayerFeed).intact && redirected.simulation.power_tasks().empty(), "a redirected unit cuts nothing");

    PowerTheater killed;
    verify(killed.order(killed.raider, FactionId::MASS_WARFARE, Simulation::PowerOrder::SEVER_LINE, kPlayerFeed), "ordered");
    killed.run(2);
    killed.simulation.component_manager().get_component<Health>(killed.raider)->is_dead = true;
    killed.run(2);
    verify(killed.line(kPlayerFeed).intact && killed.simulation.power_tasks().empty(), "a dead unit cuts nothing");
}

TEST(finished_structures_add_to_a_sides_power_demand) {
    PowerTheater theater;
    theater.run(2);
    theater.simulation.power_grid().add_structure_demand(0, structure_power_demand(1));
    theater.run(2);
    verify(theater.power(theater.player_base).demand_mw == kPostDemand + structure_power_demand(1),
        "a radar mast draws from the grid");
    verify(structure_power_demand(2) > structure_power_demand(0), "an airfield draws more than an outpost");
}

TEST(power_grid_orders_play_back_from_the_command_log) {
    std::vector<InputCommand> orders;
    std::vector<uint64_t> hashes;
    {
        PowerTheater live;
        verify(live.order(live.raider, FactionId::MASS_WARFARE, Simulation::PowerOrder::SEVER_LINE, kPlayerFeed), "cut ordered");
        verify(live.order(live.tank, FactionId::ELITE_PRECISION, Simulation::PowerOrder::CAPTURE_LINE, kEnemyFeed), "capture ordered");
        for (int tick = 0; tick < 1200; ++tick) {
            live.simulation.update(50.0f);
            if (tick % 20 == 0) hashes.push_back(live.simulation.state_hash());
        }
        verify(!live.line(kPlayerFeed).intact && live.line(kEnemyFeed).owner == 0, "both orders took effect live");
        orders = live.simulation.command_log();
    }
    PowerTheater replay;
    replay.simulation.play_back_commands(orders);
    size_t sample = 0;
    for (int tick = 0; tick < 1200; ++tick) {
        replay.simulation.update(50.0f);
        if (tick % 20 == 0) verify(replay.simulation.state_hash() == hashes[sample++], "playback matches the live run");
    }
    verify(!replay.line(kPlayerFeed).intact && replay.line(kEnemyFeed).owner == 0, "the grid ends in the same state");

    PowerTheater idle;
    idle.run(1200);
    verify(idle.simulation.state_hash() != hashes.back(), "a run with no orders differs, so the comparison can fail");
}
