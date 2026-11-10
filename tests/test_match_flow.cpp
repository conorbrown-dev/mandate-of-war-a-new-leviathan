#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/faction.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

using namespace rts;

namespace {
void verify(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

void place_unit(Simulation& simulation, EntityId unit, float x, float y) {
    auto* position = simulation.component_manager().get_component<Position>(unit);
    position->x = x;
    position->y = y;
    simulation.spatial_grid().update(unit, x, y);
}

float unit_hit_points(Simulation& simulation, EntityId unit) {
    float current = 0.0f, maximum = 0.0f;
    simulation.get_unit_health(unit, current, maximum);
    return current;
}

constexpr float kFrontDistance = 3000.0f;

struct MatchFront {
    Simulation simulation;
    EntityId player_post = INVALID_ENTITY;
    EntityId enemy_post = INVALID_ENTITY;

    MatchFront() {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.start();
        player_post = simulation.create_faction_base(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
        enemy_post = simulation.create_faction_base(FactionId::MASS_WARFARE, 0.0f, kFrontDistance);
        simulation.begin_match(player_post, enemy_post);
        // The AI keeps its first mobile unit back to claim sites.
        const EntityId engineer = simulation.create_unit_with_type(40.0f, kFrontDistance + 60.0f,
            UnitType::INDUSTRIAL_ENGINEERING, FactionId::MASS_WARFARE);
        simulation.stop_unit(engineer);
    }

    ~MatchFront() { simulation.stop(); }

    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
    AIManager& ai() { return simulation.ai_manager(); }
    EntityId spawn_enemy(float offset) {
        const EntityId id = simulation.create_unit_with_type(offset, kFrontDistance - 60.0f,
            UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
        simulation.stop_unit(id);
        return id;
    }
};
}

TEST(ai_holds_units_at_home_until_a_wave_is_ready) {
    MatchFront front;
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 100, 4, 200);
    const EntityId waiting = front.spawn_enemy(0.0f);
    front.run(100);
    verify(front.ai().waves_launched() == 0, "a part-built wave stays home");
    verify(!front.ai().is_committed(waiting), "a waiting unit is not committed");
    verify(!front.simulation.has_move_target(waiting), "a waiting unit has no assault order");
}

TEST(ai_commits_a_whole_wave_and_grows_the_next_one) {
    MatchFront front;
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 3, 2, 5);
    const EntityId first = front.spawn_enemy(0.0f);
    const EntityId second = front.spawn_enemy(20.0f);
    const EntityId third = front.spawn_enemy(40.0f);
    front.run(25);
    verify(front.ai().waves_launched() >= 1, "a full wave launches");
    verify(front.ai().is_committed(first) && front.ai().is_committed(second) && front.ai().is_committed(third),
        "the whole wave is committed together");
    const float start_x = front.simulation.get_unit_x(first);
    const float start_y = front.simulation.get_unit_y(first);
    front.run(200);
    verify(std::hypot(front.simulation.get_unit_x(first) - start_x, front.simulation.get_unit_y(first) - start_y) > 50.0f,
           "the wave leaves home to search");
}

TEST(ai_reserves_defend_their_home_when_it_is_attacked) {
    MatchFront front;
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 100, 4, 200);
    const EntityId reserve = front.spawn_enemy(0.0f);
    const EntityId raider = front.simulation.create_unit_with_type(70.0f, kFrontDistance - 60.0f,
        UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    front.simulation.stop_unit(raider);
    front.run(100);
    verify(front.ai().focus_target() == raider, "the AI sees the raid on its home");
    verify(!front.ai().is_committed(reserve), "a reserve stays out of the wave");
    const auto* health = front.simulation.component_manager().get_component<Health>(raider);
    verify(health->current < health->max, "a reserve fires on the raider");
}

TEST(match_stays_open_while_both_command_posts_stand) {
    MatchFront front;
    front.run(40);
    verify(front.simulation.match_result() == -1, "the match is still playing");
    verify(front.simulation.match_seconds() > 1.9f && front.simulation.match_seconds() < 2.1f, "match time counts ticks");
}

TEST(destroying_the_enemy_command_post_wins_the_match) {
    MatchFront front;
    front.simulation.apply_damage(front.enemy_post, 100000.0f);
    front.run(1);
    verify(front.simulation.match_result() == 0, "the player wins");
}

TEST(losing_the_player_command_post_loses_the_match) {
    MatchFront front;
    front.simulation.apply_damage(front.player_post, 100000.0f);
    front.run(1);
    verify(front.simulation.match_result() == 1, "the enemy wins");
}

TEST(a_finished_match_keeps_its_first_result) {
    MatchFront front;
    front.simulation.apply_damage(front.player_post, 100000.0f);
    front.run(1);
    front.simulation.apply_damage(front.enemy_post, 100000.0f);
    front.run(1);
    verify(front.simulation.match_result() == 1, "the result does not change");
}

TEST(an_undefended_player_loses_to_the_waves) {
    MatchFront front;
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 3, 2, 8);
    for (int unit = 0; unit < 6; ++unit) front.spawn_enemy(static_cast<float>(unit) * 20.0f);
    for (int tick = 0; tick < 20 * 60 * 15 && front.simulation.match_result() == -1; ++tick) {
        front.simulation.update(50.0f);
    }
    verify(front.simulation.match_result() == 1, "the waves destroy an undefended command post");
}

TEST(ground_units_road_march_until_they_near_an_enemy) {
    Simulation simulation;
    simulation.configure_world_size(40000.0f, 40000.0f);
    simulation.start();
    simulation.enable_ai(false);
    const EntityId tank = simulation.create_unit_with_type(0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    const float combat_speed = simulation.component_manager().get_component<UnitData>(tank)->speed;
    simulation.move_unit(tank, 0.0f, 3000.0f);
    for (int tick = 0; tick < 100; ++tick) simulation.update(50.0f);
    const float start = simulation.get_unit_y(tank);
    for (int tick = 0; tick < 40; ++tick) simulation.update(50.0f);
    const float march_speed = (simulation.get_unit_y(tank) - start) / 2.0f;
    verify(march_speed > combat_speed * 1.45f && march_speed <= combat_speed * 1.65f,
        "an open road march runs at road speed, about 1.6 times cross-country speed");

    const EntityId enemy = simulation.create_unit_with_type(0.0f, simulation.get_unit_y(tank) + 300.0f,
        UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
    simulation.stop_unit(enemy);
    for (int tick = 0; tick < 40; ++tick) simulation.update(50.0f);
    const float slowed_start = simulation.get_unit_y(tank);
    for (int tick = 0; tick < 20; ++tick) simulation.update(50.0f);
    const float contact_speed = simulation.get_unit_y(tank) - slowed_start;
    verify(contact_speed <= combat_speed * 1.05f, "a unit near an enemy moves at combat speed");
    simulation.stop();
}

TEST(short_moves_stay_at_combat_speed) {
    Simulation simulation;
    simulation.configure_world_size(40000.0f, 40000.0f);
    simulation.start();
    simulation.enable_ai(false);
    const EntityId tank = simulation.create_unit_with_type(0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    const float combat_speed = simulation.component_manager().get_component<UnitData>(tank)->speed;
    simulation.move_unit(tank, 0.0f, 120.0f);
    float fastest = 0.0f;
    float previous = simulation.get_unit_y(tank);
    for (int tick = 0; tick < 100; ++tick) {
        simulation.update(50.0f);
        fastest = std::max(fastest, (simulation.get_unit_y(tank) - previous) / 0.05f);
        previous = simulation.get_unit_y(tank);
    }
    verify(fastest <= combat_speed * 1.05f, "tactical moves do not road-march");
    simulation.stop();
}

TEST(an_enemy_is_only_seen_while_something_is_watching_it) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const EntityId enemy = front.spawn_enemy(0.0f);
    front.run(10);
    const auto seen_by_player = [&](EntityId target) {
        const auto seen = simulation.seen_enemies(FactionId::ELITE_PRECISION);
        return std::find(seen.begin(), seen.end(), target) != seen.end();
    };
    verify(!seen_by_player(enemy) && !seen_by_player(front.enemy_post), "the enemy starts unseen");

    const EntityId scout = simulation.create_unit_with_type(0.0f, kFrontDistance - 500.0f, static_cast<UnitType>(13), FactionId::ELITE_PRECISION);
    simulation.stop_unit(scout);
    front.run(10);
    verify(seen_by_player(enemy) && seen_by_player(front.enemy_post), "a scout reveals what is in its range");
    verify(simulation.can_see(FactionId::ELITE_PRECISION, 0.0f, kFrontDistance - 60.0f), "the watched ground is visible");

    simulation.move_unit(scout, 0.0f, 500.0f);
    for (int tick = 0; tick < 1200 && simulation.can_see(FactionId::ELITE_PRECISION, 0.0f, kFrontDistance - 60.0f); ++tick) simulation.update(50.0f);
    front.run(10);
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, 0.0f, kFrontDistance - 60.0f), "ground stops being visible when the scout leaves");
    verify(!seen_by_player(enemy), "the enemy disappears when nothing is watching");
}

TEST(a_finished_radar_mast_keeps_watching_its_ground) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const float far_y = -2500.0f;
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, 0.0f, far_y), "the ground starts unwatched");
    auto& production = simulation.production_manager();
    production.update_storage(front.player_post, 2000.0f, 0.0f);
    verify(simulation.queue_structure(production.faction_line(FactionId::ELITE_PRECISION), FactionId::ELITE_PRECISION, 1, 300.0f, -300.0f),
        "a radar mast is accepted");
    front.run(20 * 40);
    verify(simulation.can_see(FactionId::ELITE_PRECISION, 300.0f, far_y), "the mast watches ground kilometres away");
}

TEST(an_engineer_inside_a_finished_airfield_steps_out_and_can_move) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const float site_x = -4000.0f;
    const float site_y = -4000.0f;
    auto& production = simulation.production_manager();
    production.update_storage(front.player_post, 20000.0f, 20000.0f);
    verify(simulation.queue_structure(production.faction_line(FactionId::ELITE_PRECISION), FactionId::ELITE_PRECISION, 2, site_x, site_y),
        "an airfield is accepted");
    const EntityId engineer = simulation.create_unit_with_type(site_x + 7.0f, site_y, UnitType::INDUSTRIAL_ENGINEERING, FactionId::ELITE_PRECISION);
    simulation.stop_unit(engineer);
    for (int tick = 0; tick < 20 * 600 &&
        !simulation.territorial_control_manager().has_active_installation(FactionId::ELITE_PRECISION, InstallationType::AIRFIELD); ++tick) {
        simulation.update(50.0f);
    }
    verify(simulation.territorial_control_manager().has_active_installation(FactionId::ELITE_PRECISION, InstallationType::AIRFIELD),
        "the airfield finishes");
    const auto rules = structure_site_rules(2);
    const float x = simulation.get_unit_x(engineer);
    const float y = simulation.get_unit_y(engineer);
    verify(std::abs(x - site_x) > rules.footprint_x * 0.5f || std::abs(y - site_y) > rules.footprint_y * 0.5f,
        "the engineer is no longer on the runway");
    auto& navigation = simulation.pathfinding();
    verify(navigation.is_walkable(navigation.to_grid_x(x), navigation.to_grid_y(y)), "the engineer stands on walkable ground");
    verify(simulation.issue_commands({engineer}, FactionId::ELITE_PRECISION, CommandType::MOVE, site_x - 1500.0f, site_y) == 1,
        "the engineer accepts a move order");
    front.run(20 * 30);
    const float moved = std::hypot(simulation.get_unit_x(engineer) - x, simulation.get_unit_y(engineer) - y);
    verify(moved > 100.0f, "the engineer drives away from the airfield");
}

TEST(the_ai_finds_the_player_knowing_only_the_sector) {
    MatchFront front;
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 3, 2, 8);
    for (int unit = 0; unit < 6; ++unit) front.spawn_enemy(static_cast<float>(unit) * 20.0f);
    for (int tick = 0; tick < 20 * 60 * 15 && front.simulation.match_result() == -1; ++tick) {
        front.simulation.update(50.0f);
    }
    verify(front.simulation.match_result() == 1, "the AI searches out and destroys the player with no position to go on");
}

TEST(a_recon_flight_goes_straight_to_its_point_with_no_airfield) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const float target_y = 2000.0f;
    verify(!simulation.territorial_control_manager().has_active_installation(FactionId::ELITE_PRECISION, InstallationType::AIRFIELD), "the player has no airfield");
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, -3000.0f, target_y), "the target starts unwatched");
    verify(simulation.issue_build_commands({front.player_post}, FactionId::ELITE_PRECISION, -3000.0f, target_y, 14) == 1, "a recon flight is ordered");
    EntityId plane = INVALID_ENTITY;
    bool watched = false;
    for (int tick = 0; tick < 20 * 240 && !watched; ++tick) {
        simulation.update(50.0f);
        for (const EntityId entity : simulation.get_entity_list()) {
            if (simulation.on_recon_flight(entity)) plane = entity;
        }
        watched = simulation.can_see(FactionId::ELITE_PRECISION, -3000.0f, target_y);
    }
    verify(plane != INVALID_ENTITY, "the plane arrives without an airfield");
    verify(watched, "the plane watches the point it was sent to");

    const float x = simulation.get_unit_x(plane);
    verify(simulation.issue_commands({plane}, FactionId::ELITE_PRECISION, CommandType::MOVE, x, -9000.0f) == 0, "a new order is refused");
    front.run(40);
    verify(simulation.get_unit_y(plane) > 0.0f, "a flight plan cannot be changed once it is set");

    bool left = false;
    for (int tick = 0; tick < 20 * 400 && !left; ++tick) {
        simulation.update(50.0f);
        const auto entities = simulation.get_entity_list();
        left = std::find(entities.begin(), entities.end(), plane) == entities.end();
    }
    verify(left, "the plane leaves the map when its time on station ends");
    verify(simulation.recon_flights_in_air(FactionId::ELITE_PRECISION) == 0, "the flight is over");
    verify(simulation.logistics_manager().crashed_aircraft_count() == 0, "it flew out, it did not crash");
}

TEST(a_combat_aircraft_still_needs_an_airfield) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const auto before = simulation.get_entity_list().size();
    simulation.issue_build_commands({front.player_post}, FactionId::ELITE_PRECISION, 0.0f, 500.0f, 9);
    front.run(20 * 60);
    verify(simulation.get_entity_list().size() == before, "no fighter arrives without an airfield");
}

namespace {
constexpr float kRidgeX = -12500.0f;
constexpr float kRidgeY = 0.0f;
constexpr float kFieldHeight = 100.0f;

// The real heightfield, levelled so each test controls exactly what is in the way.
struct RidgeField {
    Simulation simulation;
    EntityId player_post = INVALID_ENTITY;
    EntityId enemy_post = INVALID_ENTITY;

    RidgeField() {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.terrain().load_from_file("godot/project/scenarios/terrain.json");
        simulation.terrain().set_world_bounds(40000.0f, 40000.0f);
        simulation.start();
        simulation.enable_ai(false);
        simulation.terrain().flatten_rect(kRidgeX, kRidgeY, 3000.0f, 3000.0f, kFieldHeight);
        player_post = simulation.create_faction_base(FactionId::ELITE_PRECISION, kRidgeX - 2500.0f, kRidgeY - 2500.0f);
        enemy_post = simulation.create_faction_base(FactionId::MASS_WARFARE, kRidgeX + 2500.0f, kRidgeY + 2500.0f);
        simulation.begin_match(player_post, enemy_post);
    }

    ~RidgeField() { simulation.stop(); }

    void raise_ridge() { simulation.terrain().flatten_rect(kRidgeX, kRidgeY + 250.0f, 1500.0f, 40.0f, kFieldHeight + 300.0f); }
    void level_ridge() { simulation.terrain().flatten_rect(kRidgeX, kRidgeY + 250.0f, 1500.0f, 40.0f, kFieldHeight); }
    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
};
}

TEST(a_ridge_hides_the_ground_behind_it) {
    RidgeField field;
    auto& simulation = field.simulation;
    const auto scout = static_cast<UnitType>(13);
    const EntityId observer = simulation.create_unit_with_type(kRidgeX, kRidgeY, scout, FactionId::ELITE_PRECISION);
    simulation.stop_unit(observer);
    field.run(6);
    verify(simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500.0f), "open ground in range is watched");

    field.raise_ridge();
    field.run(6);
    verify(simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 150.0f), "ground in front of the ridge stays watched");
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500.0f), "ground behind the ridge is hidden");
    verify(simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY - 500.0f), "ground on the open side stays watched");

    field.level_ridge();
    field.run(6);
    verify(simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500.0f), "earthworks that cut the ridge open the view again");
}

TEST(a_ridge_stops_units_seeing_or_shooting_each_other) {
    RidgeField field;
    auto& simulation = field.simulation;
    const EntityId shooter = simulation.create_unit_with_type(kRidgeX, kRidgeY + 200.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    const EntityId target = simulation.create_unit_with_type(kRidgeX, kRidgeY + 300.0f, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
    simulation.stop_unit(shooter);
    simulation.stop_unit(target);
    field.raise_ridge();
    const float hit_points = unit_hit_points(simulation, target);
    field.run(20 * 20);
    verify(!simulation.is_visible_to(FactionId::ELITE_PRECISION, target), "the enemy behind the ridge is not seen");
    verify(unit_hit_points(simulation, target) == hit_points, "automatic fire does not reach through the ridge");
    verify(simulation.issue_attack_commands({shooter}, FactionId::ELITE_PRECISION, target) == 0, "an attack order on an unseen enemy is refused");

    field.level_ridge();
    field.run(6);
    verify(simulation.is_visible_to(FactionId::ELITE_PRECISION, target), "with the ridge gone the enemy is seen");
    field.run(20 * 20);
    verify(unit_hit_points(simulation, target) < hit_points, "and it can be hit");
}

TEST(a_recon_plane_sees_over_a_ridge_that_stops_a_scout) {
    RidgeField field;
    auto& simulation = field.simulation;
    const EntityId scout = simulation.create_unit_with_type(kRidgeX, kRidgeY, static_cast<UnitType>(13), FactionId::ELITE_PRECISION);
    simulation.stop_unit(scout);
    field.raise_ridge();
    field.run(6);
    const float behind_y = kRidgeY + 500.0f;
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, behind_y), "the scout cannot see past the ridge");
    verify(simulation.issue_build_commands({field.player_post}, FactionId::ELITE_PRECISION, kRidgeX, kRidgeY, 14) == 1, "a recon flight is ordered");
    bool watched = false;
    for (int tick = 0; tick < 20 * 240 && !watched; ++tick) {
        simulation.update(50.0f);
        watched = simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, behind_y);
    }
    verify(watched, "the plane looks down behind the ridge");
}

TEST(a_plane_behind_a_ridge_can_still_be_seen_in_the_sky) {
    RidgeField field;
    auto& simulation = field.simulation;
    const EntityId observer = simulation.create_unit_with_type(kRidgeX, kRidgeY, static_cast<UnitType>(13), FactionId::ELITE_PRECISION);
    simulation.stop_unit(observer);
    field.raise_ridge();
    field.run(6);
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500.0f), "the ground below the plane is hidden");
    verify(simulation.can_see_sky(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500.0f), "the sky above it is not");
}

TEST(an_enemy_that_drops_out_of_sight_leaves_a_last_known_position) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const EntityId enemy = front.spawn_enemy(0.0f);
    const EntityId watcher = simulation.create_unit_with_type(0.0f, kFrontDistance - 200.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    simulation.stop_unit(watcher);
    front.run(10);
    verify(simulation.last_known(FactionId::ELITE_PRECISION).at(enemy).in_view, "a seen enemy is logged as in view");
    const float seen_x = simulation.last_known(FactionId::ELITE_PRECISION).at(enemy).x;

    place_unit(simulation, watcher, 0.0f, 500.0f);
    front.run(10);
    const auto& log = simulation.last_known(FactionId::ELITE_PRECISION);
    verify(log.contains(enemy) && !log.at(enemy).in_view, "the enemy stays logged once it is out of sight");
    verify(log.at(enemy).x == seen_x, "the record holds where it was seen, not where it is");
    verify(log.at(enemy).unit_type == static_cast<int>(UnitType::MASS_SWARM_TANK), "the record holds what it was");

    place_unit(simulation, enemy, 5000.0f, 3000.0f);
    front.run(10);
    verify(simulation.last_known(FactionId::ELITE_PRECISION).at(enemy).x == seen_x, "a hidden move does not update the record");

    place_unit(simulation, watcher, seen_x, kFrontDistance - 60.0f);
    front.run(10);
    verify(!simulation.last_known(FactionId::ELITE_PRECISION).contains(enemy), "checking the spot and finding it empty clears the record");
}

TEST(a_found_command_post_is_never_forgotten) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const EntityId scout = simulation.create_unit_with_type(0.0f, 300.0f, static_cast<UnitType>(15), FactionId::MASS_WARFARE);
    simulation.stop_unit(scout);
    front.run(10);
    verify(simulation.last_known(FactionId::MASS_WARFARE).contains(front.player_post), "the scout finds the post");
    place_unit(simulation, scout, 0.0f, 2500.0f);
    front.run(kSightingMemoryTicks + 10);
    verify(simulation.last_known(FactionId::MASS_WARFARE).contains(front.player_post), "a march longer than sighting memory still has somewhere to go");
}

TEST(a_last_known_position_expires) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    const EntityId enemy = front.spawn_enemy(0.0f);
    const EntityId watcher = simulation.create_unit_with_type(0.0f, kFrontDistance - 200.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    simulation.stop_unit(watcher);
    front.run(10);
    place_unit(simulation, watcher, 0.0f, 500.0f);
    front.run(10);
    verify(simulation.last_known(FactionId::ELITE_PRECISION).contains(enemy), "the record exists");
    front.run(kSightingMemoryTicks + 10);
    verify(!simulation.last_known(FactionId::ELITE_PRECISION).contains(enemy), "it is forgotten after three minutes");
}

TEST(the_ai_only_knows_about_enemies_it_can_see) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(true);
    const EntityId hidden = simulation.create_unit_with_type(9000.0f, 9000.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    simulation.stop_unit(hidden);
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 100, 4, 200);
    front.run(100);
    const auto known = front.ai().get_enemy_units();
    verify(std::find(known.begin(), known.end(), hidden) == known.end(), "a unit nothing can see is unknown to the AI");
    verify(known == simulation.seen_enemies(FactionId::MASS_WARFARE), "the AI reads the same fog as the player");
}

TEST(the_ai_buys_scouts_and_keeps_them_moving) {
    MatchFront front;
    auto& simulation = front.simulation;
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 100, 4, 200);
    const auto materials = [&] {
        const auto storage = simulation.production_manager().faction_storage(FactionId::MASS_WARFARE);
        return simulation.production_manager().storages().at(storage).tactical.materials.current;
    };
    const float before = materials();
    front.run(20 * 20);
    verify(front.ai().scout_count() == 2, "the AI fields two scouts");
    verify(materials() < before, "they are paid for");
    verify(materials() >= 260.0f, "the price of a recon flight is kept back while the enemy is unknown");
    auto& production = simulation.production_manager();
    production.storages().at(production.faction_storage(FactionId::MASS_WARFARE)).tactical.materials.current = 3000.0f;
    std::vector<std::pair<float, float>> first;
    std::vector<EntityId> scouts;
    for (const EntityId unit : simulation.get_entity_list()) {
        const auto* data = simulation.component_manager().get_component<UnitData>(unit);
        if (data && data->unit_type == 15) {
            scouts.push_back(unit);
            first.emplace_back(simulation.get_unit_x(unit), simulation.get_unit_y(unit));
        }
    }
    verify(scouts.size() == 2, "both scouts are on the map");
    front.run(20 * 30);
    for (size_t index = 0; index < scouts.size(); ++index) {
        verify(std::hypot(simulation.get_unit_x(scouts[index]) - first[index].first, simulation.get_unit_y(scouts[index]) - first[index].second) > 50.0f,
               "each scout keeps moving through the sector");
    }
    verify(!front.ai().is_committed(scouts[0]), "scouts are not fed into waves");
    simulation.apply_damage(scouts[0], 1000.0f);
    front.run(20 * 30);
    verify(front.ai().scout_count() == 2, "a lost scout is replaced");
}

TEST(the_ai_sends_recon_flights_with_no_airfield_while_it_knows_nothing) {
    MatchFront front;
    auto& simulation = front.simulation;
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 100, 4, 200);
    verify(!simulation.territorial_control_manager().has_active_installation(FactionId::MASS_WARFARE, InstallationType::AIRFIELD), "the AI has no airfield");
    front.run(20 * 150);
    verify(front.ai().recon_flights_ordered() >= 1, "the AI orders a recon flight");
}

TEST(the_ai_goes_to_where_it_last_saw_the_enemy) {
    MatchFront front;
    auto& simulation = front.simulation;
    simulation.enable_ai(false);
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 2, 2, 8);
    const EntityId first = front.spawn_enemy(0.0f);
    const EntityId second = front.spawn_enemy(20.0f);
    const EntityId intruder = simulation.create_unit_with_type(4000.0f, kFrontDistance, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    simulation.stop_unit(intruder);
    place_unit(simulation, second, 4000.0f, kFrontDistance - 100.0f);
    front.run(10);
    verify(simulation.last_known(FactionId::MASS_WARFARE).contains(intruder), "the AI sights the intruder");
    place_unit(simulation, second, 20.0f, kFrontDistance - 60.0f);
    front.run(10);
    verify(!simulation.is_visible_to(FactionId::MASS_WARFARE, intruder), "then loses it");
    simulation.enable_ai(true);
    // About 1.6 km at the Swarm Tank's 22 m/s road-march speed.
    front.run(20 * 75);
    verify(simulation.get_unit_x(first) > 1000.0f, "the wave heads for the last known position");
}

namespace {
struct TransportFront {
    Simulation simulation;
    EntityId post = INVALID_ENTITY;
    TransportFront() {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.start();
        simulation.enable_ai(false);
        post = simulation.create_faction_base(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
        simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, 5000.0f, 3000.0f);
    }
    ~TransportFront() { simulation.stop(); }
    float materials() {
        const auto line = simulation.production_manager().faction_line(FactionId::ELITE_PRECISION);
        const auto& lines = simulation.production_manager().production_lines();
        return simulation.production_manager().storages().at(lines.at(line).storage_id).tactical.materials.current;
    }
    int tanks() { return simulation.unit_count(FactionId::ELITE_PRECISION); }
};
}

TEST(ground_units_are_built_in_the_theatre_unless_forces_arrive_by_transport) {
    TransportFront front;
    const float before = front.materials();
    verify(front.simulation.order_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0.0f, 0.0f),
        "with the rule off, a ground unit is queued on the line");
    verify(front.simulation.inbound_forces().empty(), "and nothing is in transit");
    verify(front.materials() < before, "and the line has taken its price");
}

TEST(a_delivery_needs_a_landing_site_and_costs_nothing_without_one) {
    TransportFront front;
    front.simulation.set_forces_by_transport(true);
    const float before = front.materials();
    verify(!front.simulation.order_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0.0f, 0.0f),
        "with no airfield or landing zone, a ground unit cannot be ordered");
    verify(front.simulation.delivery_refusal(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK)
        == "AIRFIELD REQUIRED FOR REINFORCEMENTS", "and the refusal says why");
    verify(front.materials() == before && front.simulation.inbound_forces().empty(), "and nothing was taken or sent");
}

TEST(a_delivered_unit_pays_once_waits_out_its_flight_and_lands_at_the_site) {
    TransportFront front;
    front.simulation.set_forces_by_transport(true);
    verify(front.simulation.set_landing_zone(FactionId::ELITE_PRECISION, 300.0f, 0.0f), "a landing zone on land is accepted");
    // Unit counts are refreshed by the economy phase, so let one tick pass first.
    front.simulation.update(50.0f);
    const float before = front.materials();
    const int units_before = front.tanks();
    verify(front.simulation.order_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0.0f, 0.0f),
        "a ground unit can be ordered");
    const float price = before - front.materials();
    verify(price > 0.0f && front.simulation.inbound_count(FactionId::ELITE_PRECISION) == 1, "it was paid for and is in transit");
    const float build_and_flight = get_unit_prototypes().at(UnitType::ELITE_MAIN_BATTLE_TANK).build_time_seconds + Simulation::kTransitSeconds;
    const int ticks = static_cast<int>(std::ceil(build_and_flight * 20.0f));
    for (int tick = 0; tick < ticks - 2; ++tick) front.simulation.update(50.0f);
    verify(front.tanks() == units_before, "the unit has not landed before its flight is over");
    for (int tick = 0; tick < 6; ++tick) front.simulation.update(50.0f);
    verify(front.tanks() == units_before + 1 && front.simulation.inbound_forces().empty(), "it lands once");
    verify(front.materials() == before - price, "and was never charged a second time");
    bool at_site = false;
    for (const auto entity : front.simulation.get_entity_list()) {
        if (entity == front.post) continue;
        const float dx = front.simulation.get_unit_x(entity) - 300.0f;
        const float dy = front.simulation.get_unit_y(entity);
        FactionId owner = FactionId::MASS_WARFARE;
        at_site = at_site || (dx * dx + dy * dy < 200.0f * 200.0f && front.simulation.get_unit_faction_id(entity, owner) &&
            owner == FactionId::ELITE_PRECISION);
    }
    verify(at_site, "it stands at the landing zone, not at the command post");
}

TEST(aircraft_and_boats_keep_their_own_arrival_under_the_transport_rule) {
    TransportFront front;
    front.simulation.set_forces_by_transport(true);
    verify(!front.simulation.needs_delivery(UnitType::ELITE_T1_FIGHTER), "aircraft are not delivered through the ground rule");
    verify(!front.simulation.needs_delivery(UnitType::ELITE_PATROL_BOAT), "boats are not delivered through the ground rule");
    verify(front.simulation.needs_delivery(UnitType::ELITE_MAIN_BATTLE_TANK), "ground units are");
}

TEST(units_in_transit_count_against_the_unit_cap) {
    TransportFront front;
    front.simulation.set_forces_by_transport(true);
    front.simulation.set_unit_cap_per_player(front.tanks() + 2);
    verify(front.simulation.set_landing_zone(FactionId::ELITE_PRECISION, 300.0f, 0.0f), "a landing zone is set");
    verify(front.simulation.order_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0.0f, 0.0f), "first order");
    verify(front.simulation.order_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0.0f, 0.0f), "second order");
    verify(!front.simulation.order_unit(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 0.0f, 0.0f),
        "a third is refused: the two in the air already fill the cap");
    verify(front.simulation.delivery_refusal(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK) == "UNIT CAP REACHED",
        "and the refusal names the cap");
}

namespace {
// The AI's side of a front under the transport rule: a command post, its
// Field Engineer and the same funds the Godot skirmish gives each side.
struct TransportMatch {
    MatchFront front;
    explicit TransportMatch(bool with_engineer = true) {
        auto& simulation = front.simulation;
        simulation.set_forces_by_transport(true);
        simulation.set_reinforcement_resources(FactionId::MASS_WARFARE, 5000.0f, 5000.0f);
        front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 6, 4, 24);
        if (!with_engineer) {
            for (const EntityId unit : simulation.get_entity_list()) {
                if (simulation.is_construction_unit(unit)) simulation.apply_damage(unit, 1.0e6f);
            }
        }
    }
    bool has_airfield() {
        return front.simulation.territorial_control_manager().has_active_installation(FactionId::MASS_WARFARE, InstallationType::AIRFIELD);
    }
    int ground_forces() {
        int count = 0;
        for (const EntityId unit : front.simulation.get_entity_list()) {
            FactionId owner = FactionId::ELITE_PRECISION;
            const auto* data = front.simulation.component_manager().get_component<UnitData>(unit);
            if (unit == front.enemy_post || !data || front.simulation.is_construction_unit(unit) ||
                !front.simulation.get_unit_faction_id(unit, owner) || owner != FactionId::MASS_WARFARE) continue;
            const auto prototype = get_unit_prototypes().find(static_cast<UnitType>(data->unit_type));
            count += prototype != get_unit_prototypes().end() && front.simulation.needs_delivery(prototype->first) ? 1 : 0;
        }
        return count;
    }
};
}

TEST(the_ai_has_no_free_landing_site_under_the_transport_rule) {
    TransportMatch match;
    float x = 0.0f;
    float y = 0.0f;
    verify(!match.front.simulation.landing_site(FactionId::MASS_WARFARE, x, y), "the AI starts with nowhere to land forces");
    verify(match.front.simulation.delivery_refusal(FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK) == "AIRFIELD REQUIRED FOR REINFORCEMENTS",
           "so its ground orders are refused, as the player's are");
}

TEST(road_and_rail_routes_deliver_paid_ground_forces_without_an_airfield) {
    Simulation simulation;
    simulation.configure_world_size(40000.0f, 40000.0f);
    simulation.start();
    simulation.enable_ai(false);
    const auto post = simulation.create_faction_base(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    verify(post != INVALID_ENTITY, "transport test has a command post");
    simulation.set_forces_by_transport(true);
    simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, 5000.0f, 0.0f);
    simulation.update(50.0f); // refresh the live-unit count after creating the post
    const int starting_units = simulation.unit_count(FactionId::ELITE_PRECISION);
    const auto type = UnitType::ELITE_MAIN_BATTLE_TANK;
    verify(simulation.delivery_refusal_via(FactionId::ELITE_PRECISION, type, Simulation::GroundTransport::ROAD) ==
           "ROAD ROUTE UNAVAILABLE", "road transport requires a configured corridor");
    verify(simulation.configure_transport_route(FactionId::ELITE_PRECISION, Simulation::GroundTransport::ROAD,
           0.0f, 4000.0f, 0.0f, 600.0f), "road corridor is configured on land");
    verify(simulation.configure_transport_route(FactionId::ELITE_PRECISION, Simulation::GroundTransport::RAIL,
           300.0f, 4000.0f, 300.0f, 600.0f), "rail corridor is configured on land");
    const auto& navigation = simulation.pathfinding();
    verify(navigation.traversal_cost(navigation.to_grid_x(0.0f), navigation.to_grid_y(2000.0f)) <
           navigation.traversal_cost(navigation.to_grid_x(1000.0f), navigation.to_grid_y(2000.0f)),
           "the road corridor improves ordinary ground-unit traversal");
    verify(simulation.issue_ground_transport_command(FactionId::ELITE_PRECISION, type,
           Simulation::GroundTransport::ROAD) == 1, "paid road order is accepted from the command post");
    verify(simulation.issue_ground_transport_command(FactionId::ELITE_PRECISION, type,
           Simulation::GroundTransport::RAIL) == 1, "paid rail order is accepted from the command post");
    simulation.update(50.0f);
    verify(simulation.inbound_forces().size() == 2, "both modes hold a native inbound order");
    const auto road = simulation.inbound_forces()[0], rail = simulation.inbound_forces()[1];
    verify(road.route_mode == 1 && rail.route_mode == 2 && rail.arrive_tick < road.arrive_tick,
           "rail uses its faster route travel time");
    verify(simulation.production_manager().tactical_resources(post)->materials.current < 5000.0f,
           "both orders deduct Materials when accepted");
    while (simulation.simulation_tick() < rail.arrive_tick) simulation.update(50.0f);
    if (simulation.unit_count(FactionId::ELITE_PRECISION) != starting_units + 1 || simulation.inbound_forces().size() != 1)
        throw std::runtime_error("rail unloads its unit at the railhead before the road convoy arrives: units=" +
            std::to_string(simulation.unit_count(FactionId::ELITE_PRECISION)) + " inbound=" +
            std::to_string(simulation.inbound_forces().size()) + " tick=" +
            std::to_string(simulation.simulation_tick()) + " due=" + std::to_string(rail.arrive_tick));
    while (simulation.simulation_tick() < road.arrive_tick) simulation.update(50.0f);
    verify(simulation.unit_count(FactionId::ELITE_PRECISION) == starting_units + 2 && simulation.inbound_forces().empty(),
           "road unloads its unit at the road terminal");
    bool near_road = false, near_rail = false;
    for (const auto id : simulation.get_entity_list()) {
        const auto* owner = simulation.component_manager().get_component<Faction>(id);
        if (!owner || owner->faction_id != FactionId::ELITE_PRECISION ||
            !simulation.component_manager().get_component<UnitData>(id)) continue;
        near_road |= std::hypot(simulation.get_unit_x(id), simulation.get_unit_y(id) - 600.0f) < 60.0f;
        near_rail |= std::hypot(simulation.get_unit_x(id) - 300.0f, simulation.get_unit_y(id) - 600.0f) < 60.0f;
    }
    verify(near_road && near_rail, "each ground force arrives at its own route terminal");
}

TEST(the_ai_builds_its_own_airfield_and_its_forces_land_there) {
    TransportMatch match;
    auto& simulation = match.front.simulation;
    int ticks = 0;
    bool delivered_early = false;
    for (; ticks < 20 * 240 && !match.has_airfield(); ++ticks) {
        simulation.update(50.0f);
        delivered_early = delivered_early || simulation.inbound_count(FactionId::MASS_WARFARE) > 0 || match.ground_forces() > 0;
    }
    verify(match.has_airfield(), "the AI builds an airfield within four minutes");
    verify(match.front.ai().airfield_orders() == 1, "with one order");
    verify(!delivered_early, "and nothing is sent or lands before the airfield is finished");
    const auto& commands = simulation.command_log();
    verify(std::any_of(commands.begin(), commands.end(), [&](const InputCommand& command) {
        return command.player_id == static_cast<uint8_t>(FactionId::MASS_WARFARE) && (command.extra & Simulation::STRUCTURE_ORDER_FLAG) != 0 &&
               simulation.is_construction_unit(command.entity_id);
    }), "the order came from its Field Engineer");
    float site_x = 0.0f;
    float site_y = 0.0f;
    verify(simulation.landing_site(FactionId::MASS_WARFARE, site_x, site_y), "the airfield is a landing site");
    float field_x = 0.0f;
    float field_y = 0.0f;
    simulation.territorial_control_manager().get_active_installation_position(FactionId::MASS_WARFARE, InstallationType::AIRFIELD, field_x, field_y);
    verify(std::hypot(field_x - simulation.get_unit_x(match.front.enemy_post), field_y - simulation.get_unit_y(match.front.enemy_post)) < 4000.0f,
           "near its command post");
    for (int tick = 0; tick < 20 * 120 && match.ground_forces() == 0; ++tick) simulation.update(50.0f);
    verify(match.ground_forces() > 0, "ordered forces then land");
    bool at_site = false;
    for (const EntityId unit : simulation.get_entity_list()) {
        FactionId owner = FactionId::ELITE_PRECISION;
        if (unit == match.front.enemy_post || simulation.is_construction_unit(unit) ||
            !simulation.get_unit_faction_id(unit, owner) || owner != FactionId::MASS_WARFARE) continue;
        at_site = at_site || std::hypot(simulation.get_unit_x(unit) - site_x, simulation.get_unit_y(unit) - site_y) < 400.0f;
    }
    verify(at_site, "at the AI's own airfield");
}

TEST(the_ai_cannot_build_an_airfield_without_an_engineer) {
    TransportMatch match(false);
    match.front.run(20 * 180);
    verify(match.front.ai().airfield_orders() == 0, "with its engineer dead the AI orders no airfield");
    verify(!match.has_airfield(), "and has none");
    verify(match.ground_forces() == 0 && match.front.simulation.inbound_count(FactionId::MASS_WARFARE) == 0,
           "so no ground forces reach it");
}

namespace {
// The Pathfinder Scout Car has a data id but no named enum value.
constexpr UnitType kScoutCar = static_cast<UnitType>(13);

// The player's side of a match under the transport rule, with no airfield, and
// a scout car watching the ground a paratroop drop will use. Sight only runs
// once a match has begun.
struct DropFront {
    MatchFront match;
    Simulation& simulation = match.simulation;
    static constexpr float kDropX = 2100.0f;
    static constexpr float kDropY = 50.0f;
    EntityId spotter = INVALID_ENTITY;
    DropFront() {
        simulation.enable_ai(false);
        simulation.set_forces_by_transport(true);
        simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, 5000.0f, 3000.0f);
        spotter = simulation.create_unit_with_type(2000.0f, 0.0f, kScoutCar, FactionId::ELITE_PRECISION);
        simulation.stop_unit(spotter);
        // Sight is refreshed every quarter second; unit counts every tick.
        for (int tick = 0; tick < 10; ++tick) simulation.update(50.0f);
    }
    float materials() {
        const auto& production = simulation.production_manager();
        const auto line = production.faction_line(FactionId::ELITE_PRECISION);
        return production.storages().at(production.production_lines().at(line).storage_id).tactical.materials.current;
    }
    std::vector<EntityId> scout_cars() {
        std::vector<EntityId> found;
        for (const EntityId unit : simulation.get_entity_list()) {
            const auto* data = simulation.component_manager().get_component<UnitData>(unit);
            if (unit != spotter && data && data->unit_type == static_cast<uint8_t>(kScoutCar)) found.push_back(unit);
        }
        return found;
    }
};
}

TEST(a_paradrop_needs_a_scouted_drop_zone_and_a_unit_light_enough_to_drop) {
    DropFront drop;
    auto& simulation = drop.simulation;
    const float before = drop.materials();
    verify(simulation.can_see(FactionId::ELITE_PRECISION, DropFront::kDropX, DropFront::kDropY), "the scout car watches the drop zone");
    verify(!simulation.can_see(FactionId::ELITE_PRECISION, -6000.0f, 6000.0f), "and nobody watches the far ground");
    verify(simulation.paradrop_refusal(FactionId::ELITE_PRECISION, kScoutCar, -6000.0f, 6000.0f) == "DROP ZONE NOT SCOUTED",
           "a drop onto unscouted ground is refused");
    verify(!simulation.order_paradrop(FactionId::ELITE_PRECISION, kScoutCar, -6000.0f, 6000.0f), "and not flown");
    verify(simulation.paradrop_refusal(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, DropFront::kDropX, DropFront::kDropY) == "CANNOT BE AIRDROPPED",
           "a main battle tank cannot be dropped");
    verify(!simulation.order_paradrop(FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, DropFront::kDropX, DropFront::kDropY), "and is not flown");
    verify(drop.materials() == before && simulation.inbound_forces().empty(), "a refused drop costs nothing");
    float site_x = 0.0f;
    float site_y = 0.0f;
    verify(!simulation.landing_site(FactionId::ELITE_PRECISION, site_x, site_y), "the player has no airfield");
    verify(simulation.paradrop_refusal(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY).empty(),
           "but a scout car can be dropped on watched ground without one");
}

TEST(a_paradrop_pays_once_flies_in_from_the_map_edge_and_lands_spread_out) {
    DropFront drop;
    auto& simulation = drop.simulation;
    const float before = drop.materials();
    const int units_before = simulation.unit_count(FactionId::ELITE_PRECISION);
    verify(simulation.issue_paradrop_command(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY) == 1 &&
           simulation.issue_paradrop_command(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY) == 1 &&
           simulation.issue_paradrop_command(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY) == 1,
           "three drops are ordered");
    simulation.update(50.0f);
    const float price = simulation.production_manager().get_unit_metal_cost(FactionId::ELITE_PRECISION, kScoutCar);
    verify(simulation.inbound_count(FactionId::ELITE_PRECISION) == 3, "all three are in the air");
    verify(std::all_of(simulation.inbound_forces().begin(), simulation.inbound_forces().end(),
           [](const Simulation::InboundForce& force) { return force.airdrop; }), "as paratroop drops");
    verify(std::abs(before - drop.materials() - 3.0f * price) < 0.5f, "each was paid for once, at the catalog price");
    const auto& commands = simulation.command_log();
    verify(std::count_if(commands.begin(), commands.end(), [](const InputCommand& command) {
        return (command.extra & Simulation::PARADROP_ORDER_FLAG) != 0;
    }) == 3, "each drop is a recorded order");
    const float seconds = simulation.paradrop_seconds(kScoutCar, DropFront::kDropX);
    verify(seconds > 100.0f, "the transport flies all the way from the west map edge");
    const auto cargo_aircraft = [&simulation]() {
        int count = 0;
        for (const EntityId unit : simulation.get_entity_list()) {
            const auto* data = simulation.component_manager().get_component<UnitData>(unit);
            const auto prototype = data ? get_unit_prototypes().find(static_cast<UnitType>(data->unit_type)) : get_unit_prototypes().end();
            count += prototype != get_unit_prototypes().end() && prototype->second.cargo_transport ? 1 : 0;
        }
        return count;
    };
    const int build_ticks = static_cast<int>(std::ceil(get_unit_prototypes().at(kScoutCar).build_time_seconds * 20.0f));
    for (int tick = 1; tick < build_ticks + 2; ++tick) simulation.update(50.0f);
    verify(cargo_aircraft() == 3, "once each unit is ready, it takes off on its own cargo aircraft");
    // Released over the zone, the units come down under canopy.
    int elapsed = build_ticks + 2;
    int released_tick = -1;
    int landed_tick = -1;
    for (; elapsed < static_cast<int>(seconds * 20.0f * 1.5f) && landed_tick < 0; ++elapsed) {
        simulation.update(50.0f);
        const bool released = std::any_of(simulation.inbound_forces().begin(), simulation.inbound_forces().end(),
            [](const Simulation::InboundForce& force) { return force.released; });
        if (released_tick < 0 && released) released_tick = elapsed;
        if (!drop.scout_cars().empty()) landed_tick = elapsed;
    }
    verify(released_tick > 0 && cargo_aircraft() == 0, "the aircraft release their loads over the zone and leave");
    const int canopy = static_cast<int>(std::ceil(Simulation::kParachuteSeconds * 20.0f));
    verify(landed_tick - released_tick >= canopy - 1 && landed_tick - released_tick <= canopy + 2,
           "the first lands after its time under canopy, not before");
    verify(std::abs(static_cast<float>(landed_tick) / 20.0f - seconds) < seconds * 0.1f, "close to the time Strategic Command gave");
    for (int tick = 0; tick < 20 * 5; ++tick) simulation.update(50.0f);
    const auto landed = drop.scout_cars();
    verify(landed.size() == 3 && simulation.inbound_forces().empty(), "all three land");
    verify(simulation.unit_count(FactionId::ELITE_PRECISION) == units_before + 3, "and count as the player's units");
    verify(std::abs(before - drop.materials() - 3.0f * price) < 0.5f, "with no second charge");
    float nearest_pair = std::numeric_limits<float>::max();
    for (size_t first = 0; first < landed.size(); ++first) {
        const float x = simulation.get_unit_x(landed[first]);
        const float y = simulation.get_unit_y(landed[first]);
        verify(std::hypot(x - DropFront::kDropX, y - DropFront::kDropY) <= Simulation::kParadropScatterMeters + 1.0f, "each lands around the drop point");
        for (size_t second = first + 1; second < landed.size(); ++second) {
            nearest_pair = std::min(nearest_pair, std::hypot(x - simulation.get_unit_x(landed[second]), y - simulation.get_unit_y(landed[second])));
        }
    }
    verify(nearest_pair > 20.0f, "spread out, not stacked on one spot");
}

TEST(a_paradrop_obeys_the_unit_cap_like_any_delivery) {
    DropFront drop;
    auto& simulation = drop.simulation;
    simulation.set_unit_cap_per_player(simulation.unit_count(FactionId::ELITE_PRECISION) + 1);
    verify(simulation.order_paradrop(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY), "one drop fits");
    verify(simulation.paradrop_refusal(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY) == "UNIT CAP REACHED",
           "a second is refused while the first is in the air");
}

namespace {
// A tank on open ground with a known fuel store, for the ground fuel burn.
struct FuelFront {
    MatchFront match;
    Simulation& simulation = match.simulation;
    EntityId tank = INVALID_ENTITY;
    FuelFront(bool burn, float fuel) {
        simulation.enable_ai(false);
        simulation.set_ground_fuel_burn(burn);
        simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, 5000.0f, fuel);
        tank = simulation.create_unit_with_type(-2000.0f, -2000.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
        simulation.stop_unit(tank);
        simulation.update(50.0f);
    }
    float fuel() {
        const auto& production = simulation.production_manager();
        return production.storages().at(production.faction_storage(FactionId::ELITE_PRECISION)).tactical.fuel.current;
    }
    // Drives east and returns the path length actually covered.
    float drive(int ticks) {
        simulation.issue_move_commands({tank}, FactionId::ELITE_PRECISION, 2000.0f, -2000.0f, 12.0f);
        float driven = 0.0f;
        for (int tick = 0; tick < ticks; ++tick) {
            const float x = simulation.get_unit_x(tank), y = simulation.get_unit_y(tank);
            simulation.update(50.0f);
            driven += std::hypot(simulation.get_unit_x(tank) - x, simulation.get_unit_y(tank) - y);
        }
        return driven;
    }
};
}

TEST(ground_vehicles_burn_fuel_from_their_side_as_they_drive) {
    FuelFront burning(true, 3000.0f);
    const float before = burning.fuel();
    const float driven = burning.drive(20 * 60);
    const float per_km = get_unit_prototypes().at(UnitType::ELITE_MAIN_BATTLE_TANK).fuel_per_km;
    verify(per_km > 0.0f && driven > 500.0f, "the tank drives a long way in a minute");
    const float expected = driven / 1000.0f * per_km;
    verify(std::abs((before - burning.fuel()) - expected) < expected * 0.02f + 0.01f, "it burns its fuel_per_km for the distance it drove");
    verify(burning.simulation.fuel_starved_units(FactionId::ELITE_PRECISION) == 0, "and is never short of fuel");
    FuelFront free(false, 3000.0f);
    const float free_before = free.fuel();
    verify(free.drive(20 * 60) > 500.0f && free.fuel() == free_before, "with the rule off, driving costs no fuel");
}

TEST(a_vehicle_out_of_fuel_cannot_move_but_still_fights_and_moves_once_fuel_comes) {
    FuelFront empty(true, 0.0f);
    auto& simulation = empty.simulation;
    const float x = simulation.get_unit_x(empty.tank);
    const float y = simulation.get_unit_y(empty.tank);
    verify(empty.drive(20 * 10) < 0.01f, "with an empty store the tank does not move");
    verify(std::hypot(simulation.get_unit_x(empty.tank) - x, simulation.get_unit_y(empty.tank) - y) < 0.01f, "it stays where it was");
    verify(simulation.fuel_starved_units(FactionId::ELITE_PRECISION) == 1, "and is reported as halted for want of fuel");
    const EntityId enemy = simulation.create_unit_with_type(x + 60.0f, y, UnitType::MASS_ASSAULT_VEHICLE, FactionId::MASS_WARFARE);
    simulation.stop_unit(enemy);
    const float enemy_health = unit_hit_points(simulation, enemy);
    for (int tick = 0; tick < 20 * 10; ++tick) simulation.update(50.0f);
    verify(unit_hit_points(simulation, enemy) < enemy_health, "but it still fires on an enemy in range");
    simulation.set_reinforcement_resources(FactionId::ELITE_PRECISION, 5000.0f, 500.0f);
    verify(empty.drive(20 * 10) > 50.0f, "once fuel arrives it drives again");
}

namespace {
// The AI's side with an airfield already standing, ground fuel burn on and a
// chosen amount of fuel, for its fuel decisions.
struct AIFuelFront {
    MatchFront front;
    Simulation& simulation = front.simulation;
    explicit AIFuelFront(float fuel) {
        simulation.set_ground_fuel_burn(true);
        simulation.set_reinforcement_resources(FactionId::MASS_WARFARE, 5000.0f, fuel);
        simulation.territorial_control_manager().add_installation(-2000.0f, kFrontDistance, InstallationType::AIRFIELD, FactionId::MASS_WARFARE);
        front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 100, 4, 200);
    }
    float fuel() {
        const auto& production = simulation.production_manager();
        return production.storages().at(production.faction_storage(FactionId::MASS_WARFARE)).tactical.fuel.current;
    }
};
}

TEST(the_ai_orders_a_fuel_shipment_when_it_runs_low) {
    AIFuelFront low(400.0f);
    low.front.run(20 * 3);
    verify(low.front.ai().fuel_shipments_ordered() == 1, "short of fuel, the AI asks for a shipment");
    verify(low.simulation.fuel_shipment(FactionId::MASS_WARFARE) != nullptr, "and it is on its way");
    const auto& commands = low.simulation.command_log();
    verify(std::any_of(commands.begin(), commands.end(), [](const InputCommand& command) {
        return command.player_id == static_cast<uint8_t>(FactionId::MASS_WARFARE) && command.extra == Simulation::FUEL_SHIPMENT_ORDER_FLAG;
    }), "through a recorded order");
    const float before = low.fuel();
    for (int tick = 0; tick < 20 * 400 && low.simulation.fuel_shipment(FactionId::MASS_WARFARE); ++tick) low.simulation.update(50.0f);
    verify(low.fuel() > before + 1000.0f, "the shipment lands and fills the store");
    AIFuelFront full(4000.0f);
    full.front.run(20 * 60);
    verify(full.front.ai().fuel_shipments_ordered() == 0, "with fuel to spare it orders none");
}

TEST(the_ai_takes_a_fuel_depot_before_a_materials_site_when_short_of_fuel) {
    for (const float fuel : {400.0f, 4000.0f}) {
        AIFuelFront front(fuel);
        auto& production = front.simulation.production_manager();
        // Both an equal drive from the AI's engineer; the Materials site holds far more.
        production.add_resource_node(920, ResourceNode{600.0f, kFrontDistance + 60.0f, 1500.0f, 1500.0f, ResourceNode::Type::ENERGY, false});
        production.add_resource_node(921, ResourceNode{-520.0f, kFrontDistance + 60.0f, 50000.0f, 50000.0f, ResourceNode::Type::METAL, false});
        front.front.run(20 * 5);
        const EntityId target = front.front.ai().expansion_target();
        if (fuel < AIManager::kLowFuel) verify(target == 920, "short of fuel, the AI goes for the depot");
        else verify(target == 921, "with fuel to spare it goes for the bigger Materials site");
    }
}

TEST(the_ai_keeps_the_price_of_a_fuel_shipment_when_short_of_fuel) {
    for (const float fuel : {400.0f, 4000.0f}) {
        MatchFront front;
        auto& simulation = front.simulation;
        simulation.set_ground_fuel_burn(true);
        // No airfield, so no shipment can be ordered: only the reserve holds the money.
        simulation.set_reinforcement_resources(FactionId::MASS_WARFARE, 350.0f, fuel);
        front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 100, 4, 200);
        front.run(20 * 60);
        const auto& production = simulation.production_manager();
        const float materials = production.storages().at(production.faction_storage(FactionId::MASS_WARFARE)).tactical.materials.current;
        if (fuel < AIManager::kLowFuel) verify(materials >= kFuelShipmentMaterialCost, "short of fuel, the AI keeps a shipment's price");
        else verify(materials < kFuelShipmentMaterialCost, "with fuel to spare it spends that money on forces");
    }
}

TEST(a_paradrop_shot_down_on_the_way_is_lost) {
    for (const bool defended : {false, true}) {
        DropFront drop;
        auto& simulation = drop.simulation;
        if (defended) {
            // Enemy surface-to-air missiles under the route, which runs along the drop zone's y.
            for (int index = 0; index < 8; ++index) {
                const EntityId sam = static_cast<EntityId>(simulation.create_unit_with_type(
                    -9000.0f + static_cast<float>(index) * 60.0f, DropFront::kDropY + 30.0f, UnitType::MASS_ANTI_AIR, FactionId::MASS_WARFARE));
                simulation.stop_unit(sam);
            }
        }
        const float before = drop.materials();
        verify(simulation.order_paradrop(FactionId::ELITE_PRECISION, kScoutCar, DropFront::kDropX, DropFront::kDropY), "the drop is ordered");
        for (int tick = 0; tick < 20 * 300 && !simulation.inbound_forces().empty(); ++tick) simulation.update(50.0f);
        if (defended) {
            verify(drop.scout_cars().empty(), "anti-air under the route shoots the transport down and nothing lands");
            verify(simulation.paradrops_lost(FactionId::ELITE_PRECISION) == 1, "the loss is counted");
            verify(drop.materials() < before, "and the unit's price is not refunded");
        } else {
            verify(drop.scout_cars().size() == 1 && simulation.paradrops_lost(FactionId::ELITE_PRECISION) == 0, "unopposed, the drop lands");
        }
    }
}

namespace {
struct EffectsFight {
    Simulation simulation;
    EntityId shooter, victim;
    EffectsFight() {
        simulation.configure_world_size(40000, 40000);
        simulation.start();
        simulation.enable_ai(false);
        auto player = simulation.create_faction_base(FactionId::ELITE_PRECISION, -10000, 0);
        auto enemy = simulation.create_faction_base(FactionId::MASS_WARFARE, 10000, 0);
        shooter = simulation.create_unit_with_type(0, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::MASS_WARFARE);
        victim = simulation.create_unit_with_type(50, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::INDUSTRIAL_EXPERIMENTAL);
        simulation.component_manager().remove_component<Weapon>(victim);
        simulation.component_manager().get_component<Health>(victim)->current = 1;
        simulation.begin_match(player, enemy);
    }
    void run() { for (int i = 0; i < 40; ++i) simulation.update(50); }
};
}

TEST(combat_presentation_reports_real_shots_hits_and_deaths_without_consuming_them) {
    EffectsFight fight;
    fight.run();
    auto& sim = fight.simulation;
    auto events = sim.combat_events_after(0, FactionId::MASS_WARFARE);
    verify(events.size() == 3, "one shot, one hit and one destruction");
    verify(events[0].kind == CombatEventKind::SHOT && events[0].entity == fight.shooter, "shot belongs to actual shooter");
    verify(events[0].velocity_x > 0 && events[0].velocity_y == 0, "shot carries native direction");
    verify(events[1].kind == CombatEventKind::HIT && events[2].kind == CombatEventKind::DESTROYED, "damage precedes destruction");
    verify(events[1].entity == fight.victim && events[2].x == 50, "hit and death at victim, retained after removal");
    verify(sim.get_unit_is_dead(fight.victim), "victim really died");
    const auto hash = sim.state_hash();
    verify(sim.combat_events_after(0, FactionId::MASS_WARFARE).size() == 3, "readers do not consume events");
    verify(sim.combat_events_after(sim.combat_event_cursor(), FactionId::MASS_WARFARE).empty(), "cursor prevents duplicates");
    verify(sim.state_hash() == hash, "query has no gameplay side effects");
}

TEST(combat_presentation_does_not_reveal_hidden_battles_even_after_the_ground_is_scouted) {
    EffectsFight fight;
    fight.run();
    auto& sim = fight.simulation;
    verify(sim.combat_event_cursor() > 0, "hidden battle occurred");
    verify(sim.combat_event_cursor(FactionId::ELITE_PRECISION) == 0, "cursor also reveals no hidden battle activity");
    verify(sim.combat_events_after(0, FactionId::ELITE_PRECISION).empty(), "third party receives no hidden shot hit or death");
    auto scout = sim.create_unit_with_type(25, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    sim.component_manager().remove_component<Weapon>(scout);
    sim.component_manager().remove_component<Weapon>(fight.shooter);
    for (int i = 0; i < 5; ++i) sim.update(50);
    verify(sim.can_see(FactionId::ELITE_PRECISION, 50, 0), "ground now scouted");
    verify(sim.combat_events_after(0, FactionId::ELITE_PRECISION).empty(), "later vision cannot reveal historic hidden effects");
    verify(sim.combat_events_after(0, static_cast<FactionId>(255)).empty(), "invalid viewer refused");
}

TEST(combat_presentation_queries_do_not_change_the_fight_and_departures_do_not_explode) {
    EffectsFight queried, control;
    for (int i = 0; i < 40; ++i) {
        queried.simulation.update(50);
        queried.simulation.combat_events_after(0, FactionId::MASS_WARFARE);
        control.simulation.update(50);
        verify(queried.simulation.state_hash() == control.simulation.state_hash(), "identical match hashes with and without presentation reads");
    }
    const auto cursor = queried.simulation.combat_event_cursor();
    queried.simulation.destroy_unit(queried.shooter);
    verify(queried.simulation.combat_event_cursor() == cursor, "alive unit removal is not combat destruction");
    queried.simulation.reset();
    verify(queried.simulation.combat_events_after(0, FactionId::MASS_WARFARE).empty(), "reset drops old effects");
    verify(queried.simulation.combat_event_cursor() == cursor, "sequence stays monotonic across reset");
}

TEST(combat_presentation_history_is_bounded) {
    Simulation sim;
    sim.start();
    auto victim = sim.create_unit_with_type(0, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::MASS_WARFARE);
    auto& projectiles = sim.combat_manager().projectile_manager();
    for (size_t i = 0; i < Simulation::MAX_COMBAT_EVENTS + 20; ++i) {
        projectiles.spawn(-4, 0, 100, 0, 100, 0.001f, 0, 3, FactionId::ELITE_PRECISION);
        projectiles.update(50, sim.spatial_grid(), sim.component_manager());
    }
    const auto events = sim.combat_events_after(0, FactionId::MASS_WARFARE);
    verify(!sim.get_unit_is_dead(victim), "small damage leaves target alive");
    verify(events.size() == Simulation::MAX_COMBAT_EVENTS, "history bounded under sustained combat");
    verify(events.front().sequence == 21 && events.back().sequence == sim.combat_event_cursor(), "overflow retains latest events in order");
}

TEST(combat_effect_visibility_uses_the_sky_layer_for_aircraft_and_ground_layer_for_vehicles) {
    RidgeField field;
    auto& sim = field.simulation;
    const auto observer = sim.create_unit_with_type(kRidgeX, kRidgeY, static_cast<UnitType>(13), FactionId::ELITE_PRECISION);
    sim.component_manager().remove_component<Weapon>(observer);
    field.raise_ridge();
    field.run(6);
    verify(!sim.can_see(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500), "ridge hides the ground");
    verify(sim.can_see_sky(FactionId::ELITE_PRECISION, kRidgeX, kRidgeY + 500), "sky remains visible");
    const auto plane = sim.create_unit_with_type(kRidgeX, kRidgeY + 500, static_cast<UnitType>(16), FactionId::MASS_WARFARE);
    sim.component_manager().get_component<Aircraft>(plane)->status = Aircraft::Status::AIRBORNE;
    const auto tank = sim.create_unit_with_type(kRidgeX, kRidgeY + 500, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
    sim.apply_damage(plane, 100000);
    sim.destroy_unit(plane);
    sim.apply_damage(tank, 100000);
    sim.destroy_unit(tank);
    const auto events = sim.combat_events_after(0, FactionId::ELITE_PRECISION);
    verify(events.size() == 1 && events[0].entity == plane && events[0].airborne, "only the aircraft destruction crosses the ridge");
}

TEST(radar_detects_anonymous_contacts_without_granting_sight_or_a_weapon_lock) {
    RidgeField field;
    auto& sim = field.simulation;
    const auto observer = sim.create_unit_with_type(kRidgeX-500, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    const auto enemy = sim.create_unit_with_type(kRidgeX+100, 0, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
    auto* sensor = sim.component_manager().get_component<UnitData>(observer);
    sensor->view_range = 100; sensor->intel_range = 900;
    sim.component_manager().get_component<UnitData>(enemy)->intel_range = 0;
    sim.component_manager().get_component<Weapon>(observer)->range = 800;
    const auto hp = unit_hit_points(sim, enemy);
    field.run(20);
    const auto contacts = sim.radar_contacts(FactionId::ELITE_PRECISION);
    verify(contacts.size() == 1, "a hidden enemy inside intel range yields one anonymous contact");
    verify(contacts[0][0] == kRidgeX+150 && contacts[0][1] == 50, "radar reports only a coarse 100 m cell centre");
    verify(!sim.is_visible_to(FactionId::ELITE_PRECISION, enemy), "radar grants no model visibility");
    verify(!sim.last_known(FactionId::ELITE_PRECISION).contains(enemy), "radar does not identify or seed visual memory");
    verify(unit_hit_points(sim, enemy) == hp, "automatic weapons cannot fire from radar alone even in weapon range");
    verify(sim.radar_contacts(FactionId::MASS_WARFARE).empty(), "contacts do not leak to another faction");
    place_unit(sim, enemy, kRidgeX+1500, 0);
    field.run(5);
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).empty(), "contacts disappear on leaving intel coverage");
    place_unit(sim, enemy, kRidgeX-450, 0);
    field.run(5);
    verify(sim.is_visible_to(FactionId::ELITE_PRECISION, enemy), "sight still identifies a nearby enemy");
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).empty(), "identified enemies do not get duplicate radar markers");
}

TEST(radar_respects_terrain_sensor_loss_and_reset) {
    RidgeField field;
    auto& sim = field.simulation;
    const auto observer = sim.create_unit_with_type(kRidgeX, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    const auto enemy = sim.create_unit_with_type(kRidgeX, 700, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
    auto* sensor = sim.component_manager().get_component<UnitData>(observer);
    sensor->view_range = 100; sensor->intel_range = 1200;
    field.run(5);
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).size() == 1, "ground radar detects across clear terrain");
    field.raise_ridge(); field.run(5);
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).empty(), "a ridge blocks ground radar");
    field.level_ridge(); field.run(5);
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).size() == 1, "earthworks refresh radar occlusion");
    sim.destroy_unit(observer); field.run(5);
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).empty(), "destroyed sensors provide no radar coverage");
    sim.reset();
    verify(sim.radar_contacts(FactionId::ELITE_PRECISION).empty(), "reset clears contacts");
    (void)enemy;
}
