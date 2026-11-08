#include "test_framework.hpp"
#include "simulation/simulation.hpp"

#include <algorithm>
#include <cmath>
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
