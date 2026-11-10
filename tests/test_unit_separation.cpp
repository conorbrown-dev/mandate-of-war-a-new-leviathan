#include "test_framework.hpp"
#include "simulation/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace rts;

// Ground units must not stack on one spot: units ordered to the same point,
// tight formations and spawns on one point end roughly a hull length apart.
namespace {
void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

float min_pairwise_distance(Simulation& s, const std::vector<EntityId>& ids) {
    float closest = std::numeric_limits<float>::max();
    for (size_t a = 0; a < ids.size(); ++a)
        for (size_t b = a + 1; b < ids.size(); ++b)
            closest = std::min(closest, std::hypot(s.get_unit_x(ids[a]) - s.get_unit_x(ids[b]),
                                                   s.get_unit_y(ids[a]) - s.get_unit_y(ids[b])));
    return closest;
}

float max_distance_to(Simulation& s, const std::vector<EntityId>& ids, float x, float y) {
    float furthest = 0.0f;
    for (const auto id : ids) furthest = std::max(furthest, std::hypot(s.get_unit_x(id) - x, s.get_unit_y(id) - y));
    return furthest;
}

std::vector<EntityId> spawn_tanks(Simulation& s, int count, float x0, float y0, float step) {
    std::vector<EntityId> ids;
    for (int i = 0; i < count; ++i) {
        const int id = s.create_unit_with_type(x0 + step * float(i), y0, UnitType::ELITE_MAIN_BATTLE_TANK,
                                               FactionId::ELITE_PRECISION);
        expect(id >= 0, "tank spawns");
        ids.push_back(static_cast<EntityId>(id));
    }
    return ids;
}

// Ten tanks, each given its own order to the same point.
std::vector<std::pair<float, float>> run_same_point_orders(Simulation& s, std::vector<EntityId>& ids) {
    s.start();
    ids = spawn_tanks(s, 10, -45.0f, -60.0f, 10.0f);
    for (const auto id : ids)
        expect(s.issue_move_commands({id}, FactionId::ELITE_PRECISION, 0.0f, 100.0f, 3.0f) == 1,
               "single move order accepted");
    for (int tick = 0; tick < 1200; ++tick) s.update(50.0f);
    std::vector<std::pair<float, float>> positions;
    for (const auto id : ids) positions.emplace_back(s.get_unit_x(id), s.get_unit_y(id));
    return positions;
}
} // namespace

TEST(separation_units_ordered_to_one_point_end_a_hull_length_apart) {
    Simulation s;
    std::vector<EntityId> ids;
    run_same_point_orders(s, ids);
    const float closest = min_pairwise_distance(s, ids);
    expect(closest >= 6.0f, "tanks ordered to one point stack: closest pair " + std::to_string(closest) + " m");
    const float furthest = max_distance_to(s, ids, 0.0f, 100.0f);
    expect(furthest <= 40.0f, "tanks did not reach the ordered area: furthest " + std::to_string(furthest) + " m");
    for (const auto id : ids) expect(!s.has_move_target(id), "every tank settles instead of circling the occupied point");
    std::vector<std::pair<float, float>> settled;
    for (const auto id : ids) settled.emplace_back(s.get_unit_x(id), s.get_unit_y(id));
    for (int tick = 0; tick < 60; ++tick) s.update(50.0f);
    for (size_t i = 0; i < ids.size(); ++i)
        expect(std::hypot(s.get_unit_x(ids[i]) - settled[i].first, s.get_unit_y(ids[i]) - settled[i].second) < 0.05f,
               "settled tanks do not keep jostling");
}

TEST(separation_tight_formation_spacing_does_not_overlap_hulls) {
    Simulation s; s.start();
    auto ids = spawn_tanks(s, 9, -40.0f, -80.0f, 10.0f);
    expect(s.issue_move_commands(ids, FactionId::ELITE_PRECISION, 0.0f, 60.0f, 3.0f) == ids.size(),
           "formation order accepted");
    for (int tick = 0; tick < 1200; ++tick) s.update(50.0f);
    const float closest = min_pairwise_distance(s, ids);
    expect(closest >= 6.0f, "3 m formation slots overlap hulls: closest pair " + std::to_string(closest) + " m");
    expect(max_distance_to(s, ids, 0.0f, 60.0f) <= 15.0f,
           "formation reaches its destination area: furthest " + std::to_string(max_distance_to(s, ids, 0.0f, 60.0f)) + " m");
    for (const auto id : ids) expect(!s.has_move_target(id), "every formation member settles");
}

TEST(separation_units_spawned_on_one_spot_are_pushed_apart) {
    Simulation s; s.start();
    auto ids = spawn_tanks(s, 6, 50.0f, 50.0f, 0.0f);
    for (int tick = 0; tick < 200; ++tick) s.update(50.0f);
    const float closest = min_pairwise_distance(s, ids);
    expect(closest >= 6.0f, "tanks spawned on one spot stay stacked: closest pair " + std::to_string(closest) + " m");
    expect(max_distance_to(s, ids, 50.0f, 50.0f) <= 20.0f, "spawn-stacked tanks only shuffle locally");
}

TEST(separation_overfilled_spawn_still_spreads_every_hull) {
    Simulation s; s.start();
    auto ids = spawn_tanks(s, 80, 50.0f, 50.0f, 0.0f);
    for (int tick = 0; tick < 500; ++tick) s.update(50.0f);
    const float closest = min_pairwise_distance(s, ids);
    expect(closest >= 6.0f, "bounded neighbor work leaves a stacked hull: closest pair " + std::to_string(closest) + " m");
}

TEST(separation_never_pushes_units_into_blocked_cells) {
    Simulation s; s.start();
    // A wall of blocked cells just east of the spawn point.
    for (int gx = 0; gx < 320; ++gx)
        for (int gy = 0; gy < 320; ++gy)
            if (s.pathfinding().to_world_x(gx) >= 2.0f) s.pathfinding().set_cell(gx, gy, false);
    auto ids = spawn_tanks(s, 6, 0.0f, 0.0f, 0.0f);
    for (int tick = 0; tick < 200; ++tick) {
        s.update(50.0f);
        for (const auto id : ids) {
            const auto& nav = s.pathfinding();
            expect(nav.is_walkable(nav.to_grid_x(s.get_unit_x(id)), nav.to_grid_y(s.get_unit_y(id))),
                   "separation pushed a unit into a blocked cell");
        }
    }
    expect(min_pairwise_distance(s, ids) >= 6.0f, "units still separate along the open side of a wall");
}

TEST(separation_is_deterministic) {
    Simulation first, second;
    std::vector<EntityId> first_ids, second_ids;
    const auto a = run_same_point_orders(first, first_ids);
    const auto b = run_same_point_orders(second, second_ids);
    expect(a == b, "identical orders give bit-identical separated positions");
}

// Making room means driving: a unit that is overlapped turns and drives to a
// free spot like any other move, and goes back when its own spot is clear.
namespace {
// How far a unit's last step strayed from the line its hull points along.
// Zero for driving (forward or reverse), large for a slide.
struct StepWatcher {
    Simulation& s;
    EntityId id;
    float last_x, last_y;
    float path = 0.0f;
    int slide_steps = 0;
    StepWatcher(Simulation& sim, EntityId unit) : s(sim), id(unit), last_x(sim.get_unit_x(unit)), last_y(sim.get_unit_y(unit)) {}
    void step() {
        const float x = s.get_unit_x(id), y = s.get_unit_y(id);
        const float dx = x - last_x, dy = y - last_y;
        const float length = std::hypot(dx, dy);
        last_x = x;
        last_y = y;
        if (length < 0.05f) return;
        path += length;
        const auto* steering = s.component_manager().get_component<GroundSteering>(id);
        const float along = std::abs(dx * std::sin(steering->heading) + dy * std::cos(steering->heading));
        if (std::sqrt(std::max(0.0f, length * length - along * along)) > 0.5f * length) ++slide_steps;
    }
};

// Six tanks parked 9 m apart in a row, and a seventh driving through them.
struct ThroughTraffic {
    Simulation s;
    std::vector<EntityId> parked;
    EntityId mover = INVALID_ENTITY;
    std::vector<std::pair<float, float>> homes;
    ThroughTraffic() {
        s.start();
        s.enable_ai(false);
        parked = spawn_tanks(s, 6, 40.0f, 0.0f, 9.0f);
        mover = spawn_tanks(s, 1, 0.0f, 0.5f, 0.0f).front();
        for (int tick = 0; tick < 20; ++tick) s.update(50.0f);
        for (const auto id : parked) homes.emplace_back(s.get_unit_x(id), s.get_unit_y(id));
        expect(s.issue_move_commands({mover}, FactionId::ELITE_PRECISION, 140.0f, 0.5f, 3.0f) == 1, "the mover accepts its order");
    }
};
} // namespace

TEST(separation_parked_tanks_drive_aside_and_back_instead_of_sliding) {
    ThroughTraffic traffic;
    auto& s = traffic.s;
    std::vector<StepWatcher> watchers;
    for (const auto id : traffic.parked) watchers.emplace_back(s, id);
    for (int tick = 0; tick < 400; ++tick) {
        s.update(50.0f);
        for (auto& watcher : watchers) watcher.step();
    }
    float driven = 0.0f;
    int slid = 0;
    for (const auto& watcher : watchers) {
        driven += watcher.path;
        slid += watcher.slide_steps;
    }
    expect(driven > 20.0f, "the parked tanks made room for the tank driving through (" + std::to_string(driven) + " m driven)");
    expect(slid == 0, "parked tanks slid sideways " + std::to_string(slid) + " times instead of driving");
    for (int tick = 0; tick < 700; ++tick) s.update(50.0f);
    for (size_t i = 0; i < traffic.parked.size(); ++i) {
        const auto id = traffic.parked[i];
        expect(!s.has_move_target(id), "a tank that made room is stopped again");
        expect(std::hypot(s.get_unit_x(id) - traffic.homes[i].first, s.get_unit_y(id) - traffic.homes[i].second) < 2.0f,
               "a tank that made room drove back to where it stood");
    }
    expect(min_pairwise_distance(s, traffic.parked) >= 8.0f, "returned tanks do not overlap");
    expect(s.get_unit_x(traffic.mover) > 135.0f, "the tank driving through reached its destination");
}

TEST(separation_two_stopped_tanks_resolve_by_one_driving_away) {
    Simulation s; s.start();
    s.enable_ai(false);
    const auto ids = spawn_tanks(s, 2, 50.0f, 50.0f, 3.0f);
    const float held_x = s.get_unit_x(ids[0]), held_y = s.get_unit_y(ids[0]);
    s.update(50.0f);
    expect(s.has_move_target(ids[1]) && !s.has_move_target(ids[0]), "the higher id is told to make room, the lower holds its ground");
    StepWatcher watcher(s, ids[1]);
    for (int tick = 0; tick < 400; ++tick) {
        s.update(50.0f);
        watcher.step();
    }
    expect(watcher.slide_steps == 0, "the tank making room drove, it did not slide");
    expect(s.get_unit_x(ids[0]) == held_x && s.get_unit_y(ids[0]) == held_y, "the tank holding its ground was not moved");
    expect(min_pairwise_distance(s, ids) >= 8.0f, "the two hulls end clear of each other");
    for (const auto id : ids) expect(!s.has_move_target(id), "both settle");
    // The old spot is still occupied, so the tank stays where it went.
    expect(std::hypot(s.get_unit_x(ids[1]) - held_x, s.get_unit_y(ids[1]) - held_y) >= 8.0f, "it does not drive back into its neighbour");
}

TEST(separation_a_new_order_while_making_room_is_obeyed) {
    Simulation s; s.start();
    s.enable_ai(false);
    const auto ids = spawn_tanks(s, 2, 50.0f, 50.0f, 3.0f);
    s.update(50.0f);
    expect(s.has_move_target(ids[1]), "the tank is making room");
    expect(s.issue_move_commands({ids[1]}, FactionId::ELITE_PRECISION, 50.0f, 120.0f, 3.0f) == 1, "it accepts a new order");
    for (int tick = 0; tick < 900; ++tick) s.update(50.0f);
    expect(std::hypot(s.get_unit_x(ids[1]) - 50.0f, s.get_unit_y(ids[1]) - 120.0f) < 6.0f,
           "it went where it was sent, not back to where it made room from");
    expect(!s.has_move_target(ids[1]), "and stopped there");
}

TEST(separation_making_room_is_deterministic) {
    ThroughTraffic first, second;
    for (int tick = 0; tick < 700; ++tick) {
        first.s.update(50.0f);
        second.s.update(50.0f);
    }
    for (size_t i = 0; i < first.parked.size(); ++i) {
        expect(first.s.get_unit_x(first.parked[i]) == second.s.get_unit_x(second.parked[i]) &&
               first.s.get_unit_y(first.parked[i]) == second.s.get_unit_y(second.parked[i]),
               "identical runs make room identically");
    }
}
