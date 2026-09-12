#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "simulation/economy_api.h"
#include <cmath>
#include <limits>
#include "ecs/components/faction.hpp"
#include "ecs/components/aircraft.hpp"
#include "ecs/components/harvester.hpp"
#include "ecs/components/resources.hpp"

using namespace rts;
namespace {
void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
InputCommand move(EntityId id, uint32_t tick = 1) {
    InputCommand c{}; c.entity_id = id; c.tick_id = tick;
    c.cmd_type = static_cast<uint8_t>(CommandType::MOVE); c.target_x = -1000;
    return c;
}
}
TEST(commands_reject_identity_type_tick_and_duplicates) {
    Simulation s; s.start();
    auto id = s.create_unit(0, 0).id;
    auto c = move(id);
    c.player_id = 1;
    check(s.submit_commands({c}) == 0, "foreign unit rejected");
    c.player_id = 0; c.cmd_type = 255;
    check(s.submit_commands({c}) == 0, "unknown command rejected");
    c = move(id, 0); check(s.submit_commands({c}) == 0, "stale command rejected");
    c = move(id, 2); check(s.submit_commands({c}) == 0, "future command rejected");
    c = move(id); check(s.submit_commands({c, c}) == 0, "duplicate batch rejected atomically");
    check(s.command_manager().local_command_count() == 0, "rejections do not mutate queue");
    check(s.submit_commands({c}) == 1, "next-tick owner command accepted");
    s.update(50);
    check(s.get_unit_x(id) < 0, "signed target moves unit west");
}
TEST(commands_reject_invalid_positions_and_capacity) {
    Simulation s; s.start(); auto id = s.create_unit(0, 0).id;
    for (float x : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 100000.0f, 200.0f})
        check(s.issue_move_commands({id}, FactionId::ELITE_PRECISION, x, 0, 3) == 0, "invalid/outside coordinate rejected before narrowing");
    s.pathfinding().set_cell(s.pathfinding().to_grid_x(10), s.pathfinding().to_grid_y(0), false);
    check(s.issue_move_commands({id}, FactionId::ELITE_PRECISION, 10, 0, 3) == 0, "blocked target rejected");
    check(s.issue_move_commands({id}, FactionId::ELITE_PRECISION, 0, 0, -1) == 0, "negative spacing rejected");
    auto c = move(id);
    while (s.command_manager().inject_local_command(c)) {}
    const auto count = s.command_manager().local_command_count();
    check(s.submit_commands({c}) == 0, "full queue rejects batch");
    check(s.command_manager().local_command_count() == count, "capacity rejection is atomic");
}
TEST(commands_revalidate_at_execution_and_clear_on_reset) {
    Simulation s; s.start(); auto id = s.create_unit(0, 0).id;
    check(s.submit_commands({move(id)}) == 1, "queue valid command");
    s.set_unit_faction(id, FactionId::MASS_WARFARE);
    s.update(50);
    check(s.get_unit_x(id) == 0, "ownership changes invalidate queued command");
    auto injected = move(id, 99); injected.player_id = 1;
    s.command_manager().inject_local_command(injected);
    s.update(50);
    check(s.get_unit_x(id) == 0, "raw injection cannot bypass execution tick check");
    injected.tick_id = s.simulation_tick() + 1;
    s.command_manager().inject_local_command(injected);
    s.reset();
    auto replacement = s.create_unit(0, 0).id;
    s.update(50);
    check(s.command_manager().local_command_count() == 0 && s.get_unit_x(replacement) == 0, "rematch discards old commands before ID reuse");
}
TEST(commands_formation_and_stop_behavior) {
    Simulation s; s.start();
    check(s.pathfinding().has_line_of_sight(0, 2, 12, 0), "boundary endpoint has line of sight on empty map");
    auto a = s.create_unit(0, 0).id, b = s.create_unit(0, 2).id;
    check(s.issue_move_commands({a,b}, FactionId::ELITE_PRECISION, 10, 0, 4) == 2, "formation accepted");
    for (int i = 0; i < 30; ++i) s.update(50);
    if (!(std::abs(s.get_unit_x(a) - 8) < 0.01f && std::abs(s.get_unit_x(b) - 12) < 0.01f))
        throw std::runtime_error("formation positions: " + std::to_string(s.get_unit_x(a)) + ", " + std::to_string(s.get_unit_x(b)));
    check(s.issue_move_commands({a}, FactionId::ELITE_PRECISION, 40, 0, 3) == 1, "second order accepted");
    s.update(50);
    check(s.issue_stop_commands({a}, FactionId::ELITE_PRECISION) == 1, "stop accepted");
    s.update(50); const float x = s.get_unit_x(a);
    s.update(200);
    check(s.get_unit_x(a) == x, "stop cancels persistent movement");
}
TEST(commands_typed_units_remain_idle_until_ordered) {
    Simulation s; s.start();
    const int id = s.create_unit_with_type(17, -9, UnitType::ELITE_MAIN_BATTLE_TANK,
                                           FactionId::ELITE_PRECISION);
    check(id >= 0, "typed unit spawns");
    s.update(500);
    check(s.get_unit_x(id) == 17 && s.get_unit_y(id) == -9,
          "newly spawned typed units do not drift without an order");
    check(s.issue_move_commands({static_cast<EntityId>(id)}, FactionId::ELITE_PRECISION,
                                27, -9, 3) == 1,
          "typed unit accepts move order");
    s.update(500);
    check(s.get_unit_x(id) > 17,
          "typed unit moves after a valid order");
}

TEST(commands_typed_ground_units_steer_on_flow_field_detours) {
    Simulation s; s.start();
    const int id = s.create_unit_with_type(0, 0, UnitType::ELITE_MAIN_BATTLE_TANK,
                                           FactionId::ELITE_PRECISION);
    s.pathfinding().set_cell(s.pathfinding().to_grid_x(5), s.pathfinding().to_grid_y(0), false);
    check(s.issue_move_commands({static_cast<EntityId>(id)}, FactionId::ELITE_PRECISION, 10, 0, 3) == 1,
          "typed ground unit accepts a detour order");
    s.update(50);
    const auto* steering = s.component_manager().get_component<GroundSteering>(static_cast<EntityId>(id));
    check(steering && steering->current_speed >= 0.0f && steering->current_speed < 3.5f &&
              std::abs(steering->heading) > 0.001f,
          "flow-field detours turn through bounded ground steering rather than instant strafing");
}

TEST(commands_wheeled_units_reverse_before_center_axis_spinning) {
    Simulation s; s.start();
    const int id = s.create_unit_with_type(0, 0, UnitType::INDUSTRIAL_ENGINEERING,
                                           FactionId::INDUSTRIAL_EXPERIMENTAL);
    check(s.issue_move_commands({static_cast<EntityId>(id)}, FactionId::INDUSTRIAL_EXPERIMENTAL, 0, -20, 3) == 1,
          "wheeled engineering unit accepts a destination directly behind its initial heading");
    s.update(250);
    const auto* steering = s.component_manager().get_component<GroundSteering>(static_cast<EntityId>(id));
    check(steering && steering->current_speed < 0.0f && std::abs(steering->heading) < 0.01f,
          "wheeled unit chooses reverse travel instead of spinning in place toward a rear destination");
    check(s.get_unit_y(id) < 0.0f,
          "wheeled unit begins a reverse maneuver toward its rear destination");
}

TEST(commands_ground_vehicle_profiles_are_authored_per_prototype) {
    const auto& prototypes = get_unit_prototypes();
    const auto mbt = prototypes.find(UnitType::ELITE_MAIN_BATTLE_TANK);
    const auto engineer = prototypes.find(UnitType::INDUSTRIAL_ENGINEERING);
    check(mbt != prototypes.end() && engineer != prototypes.end() &&
              mbt->second.steering_can_pivot_turn && !engineer->second.steering_can_pivot_turn &&
              mbt->second.steering_minimum_turn_radius < engineer->second.steering_minimum_turn_radius,
          "ground chassis steering profiles are authored per prototype rather than inferred from unit type");
}

TEST(theater_spawn_rules_keep_ground_units_out_of_water) {
    Simulation s;
    s.configure_world_size(40000.0f, 40000.0f);
    s.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f,
                                   12500.0f, 0.0f, 14000.0f, 38000.0f);
    s.start();
    check(s.create_unit_with_type(0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK,
                                  FactionId::ELITE_PRECISION) < 0,
          "ground unit cannot spawn in the theater water channel");
    check(s.create_unit_with_type(0.0f, 0.0f, UnitType::ELITE_PATROL_BOAT,
                                  FactionId::ELITE_PRECISION) >= 0,
          "patrol boat can spawn in the theater water channel");
    check(s.create_unit_with_type(-12500.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK,
                                  FactionId::ELITE_PRECISION) >= 0,
          "ground unit can spawn on a configured landmass");
}

TEST(structure_placement_validates_land_footprint_and_boundaries) {
    Simulation s;
    s.terrain().load_from_binary("godot/project/scenarios/terrain.bin");
    s.configure_world_size(40000.0f, 40000.0f);
    s.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f,
                                   12500.0f, 0.0f, 14000.0f, 38000.0f);
    s.start();
    check(s.validate_structure_placement(0, -16500.0f, -1500.0f),
          "forward outpost accepts a clear land footprint");
    check(!s.validate_structure_placement(0, 0.0f, 0.0f),
          "structure footprint rejects the theater water channel");
    check(!s.validate_structure_placement(0, -19950.0f, 0.0f),
          "structure footprint rejects a map-edge placement");
    check(!s.validate_structure_placement(0, -12500.0f, 0.0f),
          "structure footprint rejects excessive slope and height variation");
    s.pathfinding().block_world_area(-16500.0f, -1500.0f, 0.0f);
    check(!s.validate_structure_placement(0, -16500.0f, -1500.0f),
          "structure footprint rejects an occupied strategic cell");
}

TEST(engineers_spawn_only_on_ground_suitable_for_construction) {
    Simulation s;
    s.terrain().load_from_binary("godot/project/scenarios/terrain.bin");
    s.configure_world_size(40000.0f, 40000.0f);
    s.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f,
                                   12500.0f, 0.0f, 14000.0f, 38000.0f);
    s.start();
    check(s.validate_engineer_placement(-16500.0f, -1500.0f),
          "engineer accepts a clear, level construction site");
    check(s.create_unit_with_type(-16500.0f, -1500.0f,
                                  UnitType::INDUSTRIAL_ENGINEERING,
                                  FactionId::INDUSTRIAL_EXPERIMENTAL) >= 0,
          "engineer spawns on a valid construction site");
    check(!s.validate_engineer_placement(0.0f, 0.0f) &&
          s.create_unit_with_type(0.0f, 0.0f, UnitType::INDUSTRIAL_ENGINEERING,
                                  FactionId::INDUSTRIAL_EXPERIMENTAL) < 0,
          "engineer rejects a water spawn unsuitable for construction");
    check(!s.validate_engineer_placement(-19950.0f, 0.0f) &&
          s.create_unit_with_type(-19950.0f, 0.0f, UnitType::INDUSTRIAL_ENGINEERING,
                                  FactionId::INDUSTRIAL_EXPERIMENTAL) < 0,
          "engineer rejects a map-edge spawn without construction footprint");
    check(!s.validate_engineer_placement(-12500.0f, 0.0f) &&
          s.create_unit_with_type(-12500.0f, 0.0f, UnitType::INDUSTRIAL_ENGINEERING,
                                  FactionId::INDUSTRIAL_EXPERIMENTAL) < 0,
          "engineer rejects an excessively sloped spawn");
    s.pathfinding().block_world_area(-16500.0f, -1500.0f, 0.0f);
    check(!s.validate_engineer_placement(-16500.0f, -1500.0f) &&
          s.create_unit_with_type(-16500.0f, -1500.0f, UnitType::INDUSTRIAL_ENGINEERING,
                                  FactionId::INDUSTRIAL_EXPERIMENTAL) < 0,
          "engineer rejects an occupied construction site");
}

TEST(roads_validate_construct_and_apply_traversal_benefit) {
    Simulation s;
    s.configure_world_size(40000.0f, 40000.0f);
    s.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f,
                                   12500.0f, 0.0f, 14000.0f, 38000.0f);
    s.start();
    const auto base = s.create_faction_base(FactionId::INDUSTRIAL_EXPERIMENTAL, -17500.0f, -1500.0f);
    const auto engineer = static_cast<EntityId>(s.create_unit_with_type(
        -17500.0f, -1500.0f, UnitType::INDUSTRIAL_ENGINEERING,
        FactionId::INDUSTRIAL_EXPERIMENTAL));
    check(base != INVALID_ENTITY && engineer != INVALID_ENTITY, "road test creates a faction base and engineer");
    check(s.validate_road_placement(-17500.0f, -1500.0f, -15000.0f, -1500.0f),
          "road accepts a clear land route");
    check(!s.validate_road_placement(-500.0f, 0.0f, 500.0f, 0.0f),
          "road rejects a route through the theater water channel");
    check(s.queue_road(engineer, FactionId::INDUSTRIAL_EXPERIMENTAL,
                       -17500.0f, -1500.0f, -15000.0f, -1500.0f),
          "engineer queues a paid road construction");
    check(s.queue_road(engineer, FactionId::INDUSTRIAL_EXPERIMENTAL,
                       -15000.0f, -1500.0f, -12500.0f, -1500.0f),
          "engineer queues a connected road segment");

    for (int tick = 0; tick < 700; ++tick) s.update(50.0f);
    check(s.road_network().segments().size() == 2 &&
          s.road_network().segments()[0].completed && s.road_network().segments()[1].completed,
          "road construction completes and persists in the network");
    check(s.road_network().segments()[0].connected_segments.size() == 1 &&
          s.road_network().segments()[0].connected_segments.front() == s.road_network().segments()[1].id,
          "road segments record shared-endpoint connectivity");
    check(get_road_settings().material_cost == 180.0f &&
          get_road_settings().energy_cost == 120.0f &&
          get_road_settings().traversal_cost < 1.0f,
          "road construction balance is loaded from content data");
    const int road_cell_x = s.pathfinding().to_grid_x(-16000.0f);
    const int road_cell_y = s.pathfinding().to_grid_y(-1500.0f);
    check(s.pathfinding().traversal_cost(road_cell_x, road_cell_y) < 1.0f,
          "completed road lowers traversal cost on covered navigation cells");
    check(s.pathfinding().movement_speed_multiplier(-16000.0f, -1500.0f) > 1.0f,
          "completed road provides a measurable ground movement benefit");
}
TEST(off_road_travel_accumulates_wear_and_roads_reduce_it) {
    Simulation s;
    s.start();
    const auto road_tank = static_cast<EntityId>(s.create_unit_with_type(
        0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
    const auto off_road_tank = static_cast<EntityId>(s.create_unit_with_type(
        0.0f, 10.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
    check(road_tank != INVALID_ENTITY && off_road_tank != INVALID_ENTITY,
          "wear test creates comparable ground vehicles");
    for (int x = 0; x <= 100; ++x) {
        s.pathfinding().set_traversal_cost(s.pathfinding().to_grid_x(static_cast<float>(x)),
                                           s.pathfinding().to_grid_y(0.0f),
                                           get_road_settings().traversal_cost);
    }
    check(s.issue_move_commands({road_tank}, FactionId::ELITE_PRECISION, 100.0f, 0.0f, 3) == 1 &&
          s.issue_move_commands({off_road_tank}, FactionId::ELITE_PRECISION, 100.0f, 10.0f, 3) == 1,
          "comparable vehicles accept road and off-road orders");
    for (int tick = 0; tick < 250; ++tick) s.update(50.0f);
    float road_wear = 0.0f;
    float road_distance = 0.0f;
    float road_speed = 0.0f;
    float off_wear = 0.0f;
    float off_distance = 0.0f;
    float off_speed = 0.0f;
    check(s.get_unit_off_road_state(road_tank, road_wear, road_distance, road_speed) &&
          s.get_unit_off_road_state(off_road_tank, off_wear, off_distance, off_speed),
          "ground vehicles expose operational wear telemetry");
    check(off_distance > 10.0f && off_wear > road_wear * 2.0f && off_speed < road_speed,
          "long off-road movement accumulates more wear and a larger speed penalty than road travel");
    const auto* road_energy = s.component_manager().get_component<Energy>(road_tank);
    const auto* off_energy = s.component_manager().get_component<Energy>(off_road_tank);
    check(road_energy && off_energy && off_energy->current < road_energy->current,
          "off-road movement consumes more operational energy than road movement");
    check(off_road_unit_factor(UnitType::INDUSTRIAL_ENGINEERING) >
          off_road_unit_factor(UnitType::ELITE_MAIN_BATTLE_TANK),
          "unit-specific off-road resilience is content-authored");
}
TEST(civilian_area_blocks_ground_navigation) {
    Simulation s;
    s.configure_world_size(40000.0f, 40000.0f);
    s.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f,
                                   12500.0f, 0.0f, 14000.0f, 38000.0f);
    check(s.is_land_position(-12500.0f, 6000.0f), "civilian fixture begins on land");
    s.block_civilian_area(-12500.0f, 6000.0f, 180.0f);
    check(!s.is_land_position(-12500.0f, 6000.0f), "civilian structures block their navigation footprint");
    check(s.is_land_position(-12500.0f, 9000.0f), "civilian blocking remains localized");
}

TEST(commands_hidden_attack_and_visibility_loss) {
    Simulation s; s.start();
    auto own = s.create_unit(0, 0).id, enemy = s.create_unit(30, 0).id;
    s.set_unit_faction(enemy, FactionId::MASS_WARFARE);
    s.component_manager().add_component(own, UnitData{12, 10});
    s.component_manager().remove_component<Weapon>(enemy);
    check(s.issue_attack_commands({own}, FactionId::ELITE_PRECISION, enemy) == 0, "hidden attack rejected");
    s.component_manager().get_component<UnitData>(own)->view_range = 40;
    check(s.issue_attack_commands({own}, FactionId::ELITE_PRECISION, enemy) == 1, "visible attack accepted");
    s.component_manager().get_component<UnitData>(own)->view_range = 10;
    s.update(50);
    check(s.combat_manager().explicit_attack_targets().empty(), "sensor loss invalidates pending attack");
    check(s.combat_manager().projectile_manager().projectiles().empty(), "automatic fire must not reveal hidden targets");
}
TEST(commands_explicit_attack_prefers_ordered_enemy) {
    Simulation s; s.start();
    auto own = s.create_unit(0, 0).id, near = s.create_unit(10, 0).id, ordered = s.create_unit(0, 20).id;
    s.set_unit_faction(near, FactionId::MASS_WARFARE);
    s.set_unit_faction(ordered, FactionId::MASS_WARFARE);
    s.component_manager().remove_component<Weapon>(near);
    s.component_manager().remove_component<Weapon>(ordered);
    check(s.issue_attack_commands({own}, FactionId::ELITE_PRECISION, ordered) == 1, "explicit enemy accepted");
    s.update(50);
    const auto& projectiles = s.combat_manager().projectile_manager().projectiles();
    check(!projectiles.empty(), "ordered attack fires projectile");
    check(std::abs(projectiles.front().vel_x) < 0.001f && projectiles.front().vel_y > 0, "projectile follows explicit target instead of nearer enemy");
}
TEST(commands_stop_simulation_freezes_ticks) {
    Simulation s; s.start(); auto own = s.create_unit(0,0).id;
    s.issue_move_commands({own}, FactionId::ELITE_PRECISION, 20, 0, 3);
    s.stop(); s.update(250);
    check(s.simulation_tick() == 0 && s.get_unit_x(own) == 0, "stopped simulation cannot consume commands or advance ticks");
}
TEST(commands_owned_factory_build_research_and_destruction) {
    Simulation s; s.start();
    s.ai_manager().set_faction_id(FactionId::INDUSTRIAL_EXPERIMENTAL);
    auto base = s.create_faction_base(FactionId::ELITE_PRECISION, -100, 0);
    auto other = s.create_faction_base(FactionId::MASS_WARFARE, 100, 0);
    check(base != INVALID_ENTITY && other != INVALID_ENTITY && base != other, "real distinct factories created");
    auto& p = s.production_manager();
    const auto initial = p.storages().at(base);
    s.update(50);
    check(p.storages().at(base).metal_storage == initial.metal_storage &&
          p.storages().at(base).energy_storage == initial.energy_storage &&
          p.storages().at(base).research_storage == initial.research_storage, "commander has no automatic local resource income");
    p.add_resource_node(900, ResourceNode{-80, 0, 100000, 100000, ResourceNode::Type::METAL, false});
    check(s.issue_harvest_commands({base}, FactionId::ELITE_PRECISION, -80, 0) == 1, "commander accepts a territory extraction claim");
    s.update(50);
    check(p.storages().at(base).metal_storage > initial.metal_storage, "claimed territory site credits shared materials to commander storage");
    check(s.issue_build_commands({other}, FactionId::ELITE_PRECISION, 0, 0, 0) == 0, "foreign factory rejects build");
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, 0, 0, 3) == 0, "foreign prototype rejects build");
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, -90, 12, 0) == 1, "owned factory accepts unit with a player-selected rally target");
    s.update(50);
    check(p.production_lines().at(base).queue.size() == 1, "build command creates real queue");
    check(p.production_lines().at(base).queue.front().target_x == -90 &&
          p.production_lines().at(base).queue.front().target_y == 12,
          "build command preserves the selected rally target in the queue");
    check(p.storages().at(base).metal_storage < initial.metal_storage, "build reserves material");
    for (int i = 0; i < 310; ++i) s.update(50);
    check(p.get_construction_count() == 1 && s.entity_count() == 3, "paid build produces unit through simulation");
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, 0, 0, 1) == 0, "artillery production requires research");
    uint32_t research_index = 0;
    while (Simulation::research_id(research_index) != "advance_ballistics" && research_index < 100) ++research_index;
    check(s.issue_commands({base}, FactionId::ELITE_PRECISION, CommandType::RESEARCH, 0, 0, research_index) == 1, "owned research command accepted");
    s.update(50);
    check(!p.research(FactionId::ELITE_PRECISION).active_queue.empty(), "research enters timed project queue");
    for (int i = 0; i < 1001; ++i) s.update(50);
    check(p.research(FactionId::ELITE_PRECISION).completed_projects.at("advance_ballistics"), "research completes through fixed ticks");
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, 0, 0, 1) == 1, "completed research unlocks artillery production");
    s.destroy_unit(base);
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, 0, 0, 0) == 0, "destroyed factory cannot build");
    const auto balance = p.storages().at(base).metal_storage;
    s.update(50);
    check(p.storages().at(base).metal_storage == balance, "destroyed economy stops gathering");
}
TEST(commands_runway_aircraft_ferries_in_airborne_from_outside_theater) {
    Simulation s;
    s.configure_world_size(40000.0f, 40000.0f);
    s.configure_theater_landmasses(-12500.0f, 0.0f, 14000.0f, 38000.0f,
                                  12500.0f, 0.0f, 14000.0f, 38000.0f);
    s.start();
    const auto base = s.create_faction_base(FactionId::ELITE_PRECISION, -15500.0f, 1000.0f);
    check(base != INVALID_ENTITY, "fighter ferry test creates its production line");
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, 0.0f, 0.0f,
                                 static_cast<uint8_t>(UnitType::ELITE_T1_FIGHTER)) == 0,
          "runway fighter remains unavailable before an airfield is active");
    s.territorial_control_manager().add_installation(
        -15500.0f, 1000.0f, InstallationType::AIRFIELD, FactionId::ELITE_PRECISION);
    check(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, 0.0f, 0.0f,
                                 static_cast<uint8_t>(UnitType::ELITE_T1_FIGHTER)) == 1,
          "active airfield accepts the paid fighter ferry request");
    for (int tick = 0; tick < 405; ++tick) s.update(50.0f);
    const auto aircraft = s.component_manager().entities_with<Aircraft>(s.get_entity_list());
    check(aircraft.size() == 1, "completed fighter creates one real aircraft entity");
    const auto fighter = aircraft.front();
    const auto* state = s.component_manager().get_component<Aircraft>(fighter);
    check(state && state->status == Aircraft::Status::AIRBORNE,
          "off-map fighter begins tactical ingress airborne");
    const float entry_x = s.get_unit_x(fighter);
    check(entry_x < -20000.0f, "fighter is initially observed beyond the west map edge");
    for (int tick = 0; tick < 20; ++tick) s.update(50.0f);
    check(s.get_unit_x(fighter) >= -20000.0f && s.get_unit_x(fighter) > entry_x,
          "airborne fighter crosses the theater boundary under simulation movement");
}
TEST(combat_projectile_pool_recycles_and_hits_between_ticks) {
    SpatialGrid grid(10); ComponentManager components;
    components.add_component(EntityId{1}, Position{5,0,0});
    components.add_component(EntityId{1}, Health{100,100});
    components.add_component(EntityId{1}, Faction{FactionId::MASS_WARFARE});
    grid.insert(1,5,0);
    ProjectileManager projectiles(1);
    projectiles.spawn(0,0,1,0,200,25,0,3,FactionId::ELITE_PRECISION);
    projectiles.update(50,grid,components);
    check(components.get_component<Health>(1)->current==75,"fast projectile hits target between tick endpoints");
    projectiles.spawn(0,0,1,0,200,25,0,3,FactionId::ELITE_PRECISION);
    projectiles.update(50,grid,components);
    check(components.get_component<Health>(1)->current==50,"expired slot can fire again beyond lifetime shot cap");
}

TEST(commands_commanders_are_mobile_and_vision_is_positional) {
    Simulation s; s.start();
    auto base=s.create_faction_base(FactionId::ELITE_PRECISION,-100,0);
    check(s.issue_commands({base},FactionId::ELITE_PRECISION,CommandType::MOVE,-90,0)==1,"commander can receive movement");
    check(s.is_position_visible_to(FactionId::ELITE_PRECISION,-95,0),"living commander reveals nearby position");
    check(!s.is_position_visible_to(FactionId::ELITE_PRECISION,100,0),"distant projectile/contact position remains hidden");
    s.destroy_unit(base);
    check(!s.is_position_visible_to(FactionId::ELITE_PRECISION,-95,0),"dead observer provides no position vision");
}
TEST(ai_prioritizes_observed_base_threat_over_nearer_enemy) {
    Simulation s; s.start();
    auto base=s.create_faction_base(FactionId::MASS_WARFARE,100,0);
    auto defender=s.create_unit_with_type(50,0,UnitType::MASS_SWARM_TANK,FactionId::MASS_WARFARE);
    auto threat=s.create_unit_with_type(90,0,UnitType::ELITE_MAIN_BATTLE_TANK,FactionId::ELITE_PRECISION);
    auto distraction=s.create_unit_with_type(45,0,UnitType::ELITE_MAIN_BATTLE_TANK,FactionId::ELITE_PRECISION);
    s.ai_manager().attack_enemy();
    bool responds=false;
    for (const auto& command:s.command_manager().get_local_commands())
        if(command.entity_id==defender) responds |= (command.cmd_type==static_cast<int>(CommandType::ATTACK) && command.extra==threat) ||
            (command.cmd_type==static_cast<int>(CommandType::MOVE) && command.target_x==9000);
    check(responds,"AI prioritizes visible command-center threat over nearer distraction");
}

TEST(harvest_command_creates_extractor_and_harvester_component) {
    Simulation s; s.start();
    rts::ResourceNode node{50.0f, 50.0f, 1000.0f};
    s.production_manager().add_resource_node(1, node);
    const auto base = s.create_faction_base(FactionId::ELITE_PRECISION, -100, 0);
    check(base != INVALID_ENTITY, "faction has a storage facility before harvesting");
    
    auto unit = s.create_unit(0, 0);
    auto unit_id = unit.id;
    s.set_unit_faction(unit_id, FactionId::ELITE_PRECISION);
    
    check(s.issue_harvest_commands({unit_id}, FactionId::ELITE_PRECISION, 50.0f, 50.0f) == 1, "harvest command accepted");
    s.update(50);
    
    auto* harvester = s.component_manager().get_component<Harvester>(unit_id);
    check(harvester != nullptr, "unit has Harvester component after harvest command");
    
    auto* position = s.component_manager().get_component<Position>(unit_id);
    check(position != nullptr, "unit has Position component");
    
    const auto& extractors = s.production_manager().extractors();
    check(!extractors.empty(), "extractor created for harvest position");
}

TEST(resource_sites_transfer_shared_materials_and_can_be_destroyed) {
    Simulation s; s.start();
    const auto elite = s.create_faction_base(FactionId::ELITE_PRECISION, -100, 0);
    const auto mass = s.create_faction_base(FactionId::MASS_WARFARE, 100, 0);
    s.production_manager().add_resource_node(900, ResourceNode{0, 0, 1000});
    const float elite_before = s.production_manager().storages().at(elite).metal_storage;
    const float mass_before = s.production_manager().storages().at(mass).metal_storage;

    check(s.issue_harvest_commands({elite}, FactionId::ELITE_PRECISION, 0, 0) == 1,
          "commander claims unowned material facility");
    s.update(50);
    check(s.production_manager().storages().at(elite).metal_storage > elite_before,
          "claimed site increases the single Materials balance");
    check(s.production_manager().storages().at(mass).metal_storage == mass_before,
          "other resource categories are not tracked as separate site balances");

    check(s.issue_harvest_commands({mass}, FactionId::MASS_WARFARE, 0, 0) == 1,
          "enemy commander may capture material facility");
    s.update(50);
    check(s.production_manager().storages().at(mass).metal_storage > mass_before,
          "captured site routes Materials to its new owner");
    check(s.destroy_resource_site(elite, 0, 0), "commander can destroy a material facility");
    check(s.production_manager().resource_nodes().empty() && s.production_manager().extractors().empty(),
          "destroyed material facility no longer produces or remains capturable");
}

TEST(defend_command_moves_and_stops_unit) {
    Simulation s; s.start();
    auto unit = s.create_unit(0, 0);
    auto unit_id = unit.id;
    s.set_unit_faction(unit_id, FactionId::ELITE_PRECISION);
    
    check(s.issue_defend_commands({unit_id}, FactionId::ELITE_PRECISION, 30.0f, 40.0f) == 1, "defend command accepted");
    s.update(50);
    
    float x = s.get_unit_x(unit_id);
    float y = s.get_unit_y(unit_id);
    check(x > 0 && x < 31 && y > 0 && y < 41, "unit moves toward defend position and stops");
}
