#include <cmath>
#include "test_framework.hpp"
#include "simulation/simulation.hpp"

using namespace rts;

namespace {
constexpr EntityId kRadioDepot = 910;
constexpr float kRadioDepotX = 500.0f;

void verify(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

int count_kind(const std::vector<RadioEvent>& events, RadioEventKind kind) {
    int count = 0;
    for (const auto& event : events) count += event.kind == kind;
    return count;
}

struct RadioTheater {
    Simulation simulation;
    EntityId player_base = INVALID_ENTITY;
    EntityId enemy_base = INVALID_ENTITY;
    EntityId engineer = INVALID_ENTITY;

    explicit RadioTheater(bool match = true) {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.start();
        simulation.enable_ai(false);
        player_base = simulation.create_faction_base(FactionId::ELITE_PRECISION, -2000.0f, 0.0f);
        enemy_base = simulation.create_faction_base(FactionId::MASS_WARFARE, 2000.0f, 0.0f);
        engineer = simulation.create_unit(0.0f, 0.0f).id;
        simulation.set_unit_faction(engineer, FactionId::ELITE_PRECISION);
        simulation.production_manager().add_resource_node(kRadioDepot,
            ResourceNode{kRadioDepotX, 0.0f, 1500.0f, 1500.0f, ResourceNode::Type::ENERGY, false});
        if (match) simulation.begin_match(player_base, enemy_base);
    }
    ~RadioTheater() { simulation.stop(); }

    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
    void capture() {
        verify(simulation.issue_harvest_commands({engineer}, FactionId::ELITE_PRECISION, kRadioDepotX, 0.0f) == 1,
               "capture order is accepted");
        for (int tick = 0; tick < 4000 && simulation.fuel_depots().at(0).owner != 0; ++tick) simulation.update(50.0f);
        verify(simulation.fuel_depots().at(0).owner == 0, "the depot is captured");
    }
    EntityId fighter(float x, float y, FactionId owner, bool flying = true) {
        const EntityId plane = simulation.create_unit_with_type(x, y, UnitType::ELITE_T1_FIGHTER, owner);
        simulation.component_manager().remove_component<Weapon>(plane);
        auto* aircraft = simulation.component_manager().get_component<Aircraft>(plane);
        verify(aircraft != nullptr, "the fighter is an aircraft");
        aircraft->status = flying ? Aircraft::Status::AIRBORNE : Aircraft::Status::ON_GROUND;
        return plane;
    }
    void post_watcher() {
        const EntityId tank = simulation.create_unit_with_type(0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
        simulation.component_manager().remove_component<Weapon>(tank);
        run(10);
        verify(simulation.can_see_sky(FactionId::ELITE_PRECISION, 10.0f, 10.0f), "the listener watches the sky overhead");
    }
    std::vector<RadioEvent> heard(FactionId listener) { return simulation.radio_events_after(0, listener); }
};
}

TEST(radio_taking_an_enemy_fuel_depot_tells_each_side_once) {
    RadioTheater theater;
    auto& sim = theater.simulation;
    verify(sim.assign_fuel_depot(kRadioDepot, FactionId::MASS_WARFARE), "enemy holds the depot");
    theater.run(20);
    verify(theater.heard(FactionId::ELITE_PRECISION).empty() && theater.heard(FactionId::MASS_WARFARE).empty(),
           "scenario setup and an order alone report nothing");
    theater.capture();
    theater.run(200);
    const auto player = theater.heard(FactionId::ELITE_PRECISION);
    const auto enemy = theater.heard(FactionId::MASS_WARFARE);
    verify(player.size() == 1 && player[0].kind == RadioEventKind::FUEL_DEPOT_SECURED, "the captor hears one secured report, not one a tick");
    verify(enemy.size() == 1 && enemy[0].kind == RadioEventKind::FUEL_DEPOT_LOST, "the previous holder hears one lost report");
    verify(theater.heard(FactionId::INDUSTRIAL_EXPERIMENTAL).empty(), "an uninvolved side hears nothing");
    verify(player[0].x == kRadioDepotX && player[0].y == 0.0f && player[0].subject == kRadioDepot, "the report is about that depot");
    verify(player[0].has_reference && player[0].reference_x == -2000.0f, "bearings are taken from the listener's own command post");
    verify(enemy[0].has_reference && enemy[0].reference_x == 2000.0f, "each listener has its own reference point");
    verify(player[0].sequence == 1 && enemy[0].sequence == 1, "each side has its own sequence");
}

TEST(radio_an_unclaimed_depot_is_secured_without_a_loss_report) {
    RadioTheater theater(false);
    theater.capture();
    theater.run(50);
    const auto player = theater.heard(FactionId::ELITE_PRECISION);
    verify(player.size() == 1 && player[0].kind == RadioEventKind::FUEL_DEPOT_SECURED, "one secured report");
    verify(!player[0].has_reference, "no reference point is invented outside a match");
    verify(theater.heard(FactionId::MASS_WARFARE).empty(), "nobody lost a depot nobody held");
}

TEST(radio_reads_do_not_consume_or_change_the_match) {
    RadioTheater queried, control;
    queried.simulation.assign_fuel_depot(kRadioDepot, FactionId::MASS_WARFARE);
    control.simulation.assign_fuel_depot(kRadioDepot, FactionId::MASS_WARFARE);
    queried.simulation.issue_harvest_commands({queried.engineer}, FactionId::ELITE_PRECISION, kRadioDepotX, 0.0f);
    control.simulation.issue_harvest_commands({control.engineer}, FactionId::ELITE_PRECISION, kRadioDepotX, 0.0f);
    for (int tick = 0; tick < 1500; ++tick) {
        queried.simulation.update(50.0f);
        queried.simulation.radio_events_after(0, FactionId::ELITE_PRECISION);
        control.simulation.update(50.0f);
        verify(queried.simulation.state_hash() == control.simulation.state_hash(), "identical hashes with and without radio reads");
    }
    auto& sim = queried.simulation;
    verify(queried.heard(FactionId::ELITE_PRECISION).size() == 1, "the capture was reported");
    verify(queried.heard(FactionId::ELITE_PRECISION).size() == 1, "reading again returns the same report");
    const auto cursor = sim.radio_event_cursor(FactionId::ELITE_PRECISION);
    verify(sim.radio_events_after(cursor, FactionId::ELITE_PRECISION).empty(), "the cursor prevents repeats");
    verify(sim.radio_events_after(0, static_cast<FactionId>(255)).empty() &&
           sim.radio_event_cursor(static_cast<FactionId>(255)) == 0, "an invalid listener is refused");
    sim.reset();
    verify(sim.radio_events_after(0, FactionId::ELITE_PRECISION).empty(), "a new match drops old reports");
    verify(sim.radio_event_cursor(FactionId::ELITE_PRECISION) == cursor, "the sequence stays monotonic across a reset");
}

TEST(radio_hostile_aircraft_are_reported_only_when_the_listener_sees_them) {
    RadioTheater theater;
    auto& sim = theater.simulation;
    theater.post_watcher();
    const EntityId hidden = theater.fighter(9000.0f, 9000.0f, FactionId::MASS_WARFARE);
    theater.fighter(50.0f, 0.0f, FactionId::MASS_WARFARE, false);
    theater.fighter(60.0f, 0.0f, FactionId::ELITE_PRECISION);
    theater.run(40);
    verify(!sim.can_see_sky(FactionId::ELITE_PRECISION, sim.get_unit_x(hidden), sim.get_unit_y(hidden)), "the distant fighter is out of sight");
    verify(count_kind(theater.heard(FactionId::ELITE_PRECISION), RadioEventKind::AIR_HOSTILE_INBOUND) == 0,
           "unseen, parked and friendly aircraft raise no bandit call");
    verify(sim.radio_event_cursor(FactionId::ELITE_PRECISION) == 0, "the cursor does not hint at hidden aircraft either");
}

TEST(radio_a_sky_full_of_bandits_is_one_report_until_the_sky_has_been_clear) {
    RadioTheater theater;
    auto& sim = theater.simulation;
    theater.post_watcher();
    std::vector<EntityId> wave;
    for (int index = 0; index < 1500; ++index) {
        wave.push_back(theater.fighter(float(index % 40) * 2.0f - 40.0f, float(index / 40) * 2.0f - 40.0f, FactionId::MASS_WARFARE));
    }
    // Aircraft cannot hold still; pin them so they stay in sight for the count.
    for (int tick = 0; tick < 200; ++tick) {
        sim.update(50.0f);
        for (const EntityId plane : wave) {
            auto* position = sim.component_manager().get_component<Position>(plane);
            if (position) { position->x = 10.0f; position->y = 10.0f; }
        }
    }
    auto heard = theater.heard(FactionId::ELITE_PRECISION);
    verify(count_kind(heard, RadioEventKind::AIR_HOSTILE_INBOUND) == 1, "1,500 visible aircraft over 10 seconds are one report");
    verify(heard[0].count >= 1 && heard[0].count <= 1500, "the report carries how many were in sight");
    verify(std::hypot(heard[0].x, heard[0].y) < 2000.0f, "and where the listener saw them");
    verify(count_kind(theater.heard(FactionId::MASS_WARFARE), RadioEventKind::AIR_HOSTILE_INBOUND) == 0, "their owner is not warned about its own aircraft");

    for (const EntityId plane : wave) sim.destroy_unit(plane);
    theater.run(100);
    const EntityId returning = theater.fighter(10.0f, 10.0f, FactionId::MASS_WARFARE);
    theater.run(10);
    verify(count_kind(theater.heard(FactionId::ELITE_PRECISION), RadioEventKind::AIR_HOSTILE_INBOUND) == 1,
           "aircraft back within the clear time are the same raid");
    sim.destroy_unit(returning);
    theater.run(static_cast<int>(kAirThreatClearTicks) + 20);
    theater.fighter(10.0f, 10.0f, FactionId::MASS_WARFARE);
    theater.run(10);
    verify(count_kind(theater.heard(FactionId::ELITE_PRECISION), RadioEventKind::AIR_HOSTILE_INBOUND) == 2,
           "a new raid after a clear sky is reported again");
}

TEST(radio_history_is_bounded) {
    RadioTheater theater(false);
    auto& sim = theater.simulation;
    auto& production = sim.production_manager();
    for (size_t round = 0; round < Simulation::MAX_RADIO_EVENTS + 10; ++round) {
        production.assign_fuel_depot(kRadioDepot, INVALID_ENTITY, false);
        auto* position = sim.component_manager().get_component<Position>(theater.engineer);
        position->x = 0.0f;
        position->y = 0.0f;
        sim.spatial_grid().update(theater.engineer, 0.0f, 0.0f);
        theater.capture();
    }
    const auto heard = theater.heard(FactionId::ELITE_PRECISION);
    verify(heard.size() == Simulation::MAX_RADIO_EVENTS, "only the newest reports are kept");
    verify(heard.back().sequence == Simulation::MAX_RADIO_EVENTS + 10, "sequences keep counting past the bound");
}
