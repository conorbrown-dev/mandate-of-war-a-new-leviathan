#include "test_framework.hpp"
#include "ai/ai_doctrine.hpp"
#include "data/json_parser.hpp"
#include "ecs/components/faction.hpp"
#include "simulation/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
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

const AIDoctrine& doctrine(const char* key) {
    const auto* found = find_ai_doctrine(key);
    if (!found) throw std::runtime_error("missing doctrine");
    return *found;
}

const AIDifficulty& difficulty(const char* key) {
    const auto* found = find_ai_difficulty(key);
    if (!found) throw std::runtime_error("missing difficulty");
    return *found;
}

constexpr float kHomeY = 3000.0f;

// Two command posts 3 km apart on open ground. The AI's engineer is kept
// back to claim sites, so every later unit is free for the AI's plans.
struct Front {
    Simulation simulation;
    EntityId player_post = INVALID_ENTITY;
    EntityId enemy_post = INVALID_ENTITY;

    explicit Front(const char* style = "balanced", const char* level = "normal") {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.start();
        simulation.ai_manager().set_doctrine(doctrine(style));
        simulation.ai_manager().set_difficulty(difficulty(level));
        player_post = simulation.create_faction_base(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
        enemy_post = simulation.create_faction_base(FactionId::MASS_WARFARE, 0.0f, kHomeY);
        simulation.begin_match(player_post, enemy_post);
        const EntityId engineer = simulation.create_unit_with_type(40.0f, kHomeY + 60.0f,
            UnitType::INDUSTRIAL_ENGINEERING, FactionId::MASS_WARFARE);
        simulation.stop_unit(engineer);
    }
    ~Front() { simulation.stop(); }

    AIManager& ai() { return simulation.ai_manager(); }
    void run(int ticks) { for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f); }
    EntityId spawn_tank(float offset) {
        const EntityId id = simulation.create_unit_with_type(offset, kHomeY - 60.0f, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
        simulation.stop_unit(id);
        return id;
    }
    void fund(float materials) {
        auto& production = simulation.production_manager();
        production.storages().at(production.faction_storage(FactionId::MASS_WARFARE)).tactical.materials.current = materials;
    }
    std::map<int, int> fighters_by_type() {
        std::map<int, int> counts;
        for (const EntityId unit : simulation.get_entity_list()) {
            const auto* faction = simulation.component_manager().get_component<Faction>(unit);
            const auto* data = simulation.component_manager().get_component<UnitData>(unit);
            if (faction && faction->faction_id == FactionId::MASS_WARFARE && data && (data->unit_type == 3 || data->unit_type == 4 || data->unit_type == 5)) {
                ++counts[data->unit_type];
            }
        }
        return counts;
    }
};
} // namespace

TEST(ai_styles_and_difficulties_load_from_data) {
    const auto& catalog = get_ai_doctrines();
    for (const char* key : {"balanced", "swarm", "turtle", "tech"}) verify(find_ai_doctrine(key) != nullptr, "each style is listed");
    for (const char* key : {"easy", "normal", "hard"}) verify(find_ai_difficulty(key) != nullptr, "each difficulty is listed");
    verify(catalog.default_doctrine == "balanced" && catalog.default_difficulty == "normal", "balanced on normal is the default");
    AIManager fresh;
    verify(fresh.doctrine().key == "balanced" && fresh.difficulty().key == "normal", "a new AI starts on the defaults");
    verify(doctrine("swarm").unit_preference == AIDoctrine::UnitPreference::CHEAPEST, "swarm buys cheap");
    verify(doctrine("tech").unit_preference == AIDoctrine::UnitPreference::STRONGEST, "tech buys the best");
    verify(doctrine("turtle").home_guard > 0 && doctrine("turtle").attack_only_known_enemy, "turtle guards home and waits");
}

TEST(the_built_in_ai_defaults_match_the_data_file) {
    const AIDoctrine built_in;
    const AIDoctrine& file = default_ai_doctrine();
    verify(built_in.key == file.key && built_in.first_wave == file.first_wave && built_in.wave_growth == file.wave_growth &&
           built_in.max_wave == file.max_wave && built_in.home_guard == file.home_guard &&
           built_in.defence_radius == file.defence_radius && built_in.unit_preference == file.unit_preference &&
           built_in.scouts_wanted == file.scouts_wanted && built_in.first_recon_flight_seconds == file.first_recon_flight_seconds &&
           built_in.recon_flight_interval_seconds == file.recon_flight_interval_seconds && built_in.queue_depth == file.queue_depth &&
           built_in.save_for_recon_flight == file.save_for_recon_flight && built_in.attack_only_known_enemy == file.attack_only_known_enemy,
           "the built-in style is the file's default style");
    const AIDifficulty level;
    const AIDifficulty& file_level = default_ai_difficulty();
    verify(level.key == file_level.key && level.decision_seconds == file_level.decision_seconds &&
           level.reaction_seconds == file_level.reaction_seconds && level.orders_per_decision == file_level.orders_per_decision &&
           level.retreat_health == file_level.retreat_health && level.scout_retask_seconds == file_level.scout_retask_seconds,
           "the built-in difficulty is the file's default difficulty");
}

TEST(a_broken_ai_style_file_is_refused) {
    const std::string good = read_game_data("ai_doctrines.json");
    verify(parse_ai_doctrines(good).doctrines.size() == 4, "the shipped file parses");
    const auto refuses = [](const std::string& json) {
        try {
            parse_ai_doctrines(json);
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    std::string unknown = good;
    unknown.replace(unknown.find("\"cheapest\""), 10, "\"lavish\"");
    verify(refuses(unknown), "an unknown unit preference is refused");
    std::string missing = good;
    missing.replace(missing.find("\"home_guard\""), 12, "\"home_gaurd\"");
    verify(refuses(missing), "a misspelt field is refused");
    std::string bad_default = good;
    bad_default.replace(bad_default.find("\"default_difficulty\": \"normal\""), 30, "\"default_difficulty\": \"brutal\"");
    verify(refuses(bad_default), "a default that names nothing is refused");
}

TEST(a_new_simulation_puts_the_ai_back_on_the_defaults) {
    Simulation simulation;
    simulation.start();
    simulation.ai_manager().set_doctrine(doctrine("swarm"));
    simulation.ai_manager().set_difficulty(difficulty("easy"));
    simulation.stop();
    simulation.start();
    verify(simulation.ai_manager().doctrine().key == "balanced", "the style does not leak into the next match");
    verify(simulation.ai_manager().difficulty().key == "normal", "nor does the difficulty");
    simulation.stop();
}

TEST(the_balanced_style_keeps_the_briefings_wave_numbers) {
    Front front("balanced");
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 3, 2, 5);
    verify(front.ai().next_wave_size() == 3, "the first wave is the briefing's size");
}

TEST(a_style_with_its_own_waves_replaces_the_briefings) {
    Front front("swarm");
    front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 6, 4, 24);
    verify(front.ai().next_wave_size() == 4, "a swarm attacks with small waves");
    for (int unit = 0; unit < 4; ++unit) front.spawn_tank(static_cast<float>(unit) * 20.0f);
    front.run(25);
    verify(front.ai().waves_launched() == 1, "four units are enough for a swarm wave");
    verify(front.ai().next_wave_size() == 6, "the next swarm wave grows by two");
    front.ai().set_doctrine(doctrine("turtle"));
    verify(front.ai().next_wave_size() == 18, "a change of style resizes the next wave from the waves already sent");
}

TEST(a_swarm_buys_its_cheapest_units_and_tech_its_strongest) {
    for (const char* style : {"swarm", "tech"}) {
        Front front(style);
        front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 100, 4, 200);
        front.fund(20000.0f);
        front.run(20 * 90);
        const auto counts = front.fighters_by_type();
        const bool swarm = std::string(style) == "swarm";
        const int wanted = swarm ? 3 : 4;
        int total = 0;
        for (const auto& [type, count] : counts) total += count;
        verify(total >= 5, "the AI builds a fighting line");
        verify(counts.contains(wanted) && counts.at(wanted) == total,
               swarm ? "a swarm builds only its cheapest tank" : "tech builds only its strongest vehicle");
    }
}

TEST(a_mixed_style_builds_every_fighting_unit) {
    Front front("turtle");
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 100, 4, 200);
    front.fund(20000.0f);
    front.run(20 * 90);
    verify(front.fighters_by_type().size() == 3, "the turtle mixes tanks, assault vehicles and anti-air");
}

TEST(a_turtle_keeps_a_home_guard_and_waits_until_it_has_found_the_enemy) {
    Front front("turtle");
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 6, 4, 24);
    std::vector<EntityId> tanks;
    for (int unit = 0; unit < 22; ++unit) tanks.push_back(front.spawn_tank(static_cast<float>(unit % 11) * 20.0f - 100.0f + (unit / 11) * 5.0f));
    front.run(20 * 5);
    verify(front.ai().waves_launched() == 0, "a turtle does not march out blind");
    int guards = 0;
    for (const EntityId tank : tanks) guards += front.ai().is_home_guard(tank) ? 1 : 0;
    verify(guards == doctrine("turtle").home_guard, "the home guard is filled first");
    // Far from home, one of its tanks glimpses a player tank, which then drives off.
    const EntityId stray = front.simulation.create_unit_with_type(5000.0f, kHomeY + 100.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    front.simulation.stop_unit(stray);
    place_unit(front.simulation, tanks.back(), 5000.0f, kHomeY);
    front.run(10);
    verify(front.simulation.last_known(FactionId::MASS_WARFARE).contains(stray), "the turtle sights the enemy");
    place_unit(front.simulation, stray, 9000.0f, 9000.0f);
    place_unit(front.simulation, tanks.back(), 0.0f, kHomeY - 60.0f);
    front.run(20 * 3);
    verify(front.simulation.last_known(FactionId::MASS_WARFARE).contains(stray), "it remembers where it saw the enemy");
    verify(front.ai().waves_launched() == 1, "once the enemy has been found, a full wave goes");
    for (const EntityId tank : tanks) {
        if (front.ai().is_home_guard(tank)) verify(!front.ai().is_committed(tank), "the home guard never joins a wave");
    }
}

TEST(an_easy_ai_takes_time_to_react_to_a_new_contact) {
    for (const char* level : {"normal", "easy"}) {
        Front front("balanced", level);
        front.ai().configure_assault(-1500.0f, -500.0f, 1500.0f, 3500.0f, 100, 4, 200);
        front.spawn_tank(0.0f);
        front.run(20 * 4);
        // In sight of the command post, out of reach of any gun.
        const EntityId raider = front.simulation.create_unit_with_type(0.0f, kHomeY - 350.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
        front.simulation.stop_unit(raider);
        front.run(20 * 4);
        const auto early = front.ai().get_enemy_units();
        const bool reacted_early = std::find(early.begin(), early.end(), raider) != early.end();
        if (std::string(level) == "normal") {
            verify(reacted_early, "a normal AI acts on a contact within seconds");
            continue;
        }
        verify(!reacted_early, "an easy AI has not yet taken in the contact after four seconds");
        verify(front.simulation.is_visible_to(FactionId::MASS_WARFARE, raider), "although it is in plain sight");
        front.run(20 * 9);
        const auto late = front.ai().get_enemy_units();
        verify(std::find(late.begin(), late.end(), raider) != late.end(), "it reacts once its reaction time has passed");
    }
}

TEST(a_slow_commander_orders_only_so_many_units_at_a_time) {
    Front front;
    AIDifficulty limited = difficulty("normal");
    limited.orders_per_decision = 2;
    front.ai().set_difficulty(limited);
    front.ai().configure_assault(-6000.0f, -6000.0f, 6000.0f, 6000.0f, 6, 4, 24);
    std::vector<EntityId> wave;
    for (int unit = 0; unit < 6; ++unit) wave.push_back(front.spawn_tank(static_cast<float>(unit) * 20.0f));
    const auto moving = [&] {
        int count = 0;
        for (const EntityId unit : wave) count += front.simulation.has_move_target(unit) ? 1 : 0;
        return count;
    };
    front.run(22);
    verify(front.ai().waves_launched() == 1, "the wave launches");
    verify(moving() == 2, "only two units get orders in the first decision");
    front.run(20 * 3);
    verify(moving() == 6, "the rest are reached in later decisions");
}

TEST(the_ai_orders_a_paid_paradrop_only_onto_ground_it_currently_watches) {
    Front front;
    front.simulation.set_forces_by_transport(true);
    front.ai().configure_assault(-2000.0f, -1000.0f, 2000.0f, 4000.0f, 100, 4, 200);
    front.fund(20000.0f);
    front.run(20);
    verify(front.ai().paradrops_ordered() == 0, "no blind paradrop before contact");

    const EntityId target = front.simulation.create_unit_with_type(0.0f, 750.0f,
        static_cast<UnitType>(13), FactionId::ELITE_PRECISION);
    const EntityId spotter = front.simulation.create_unit_with_type(0.0f, 1050.0f,
        static_cast<UnitType>(15), FactionId::MASS_WARFARE);
    front.simulation.stop_unit(target);
    front.simulation.stop_unit(spotter);
    front.fund(0.0f);
    front.run(20);
    verify(front.ai().paradrops_ordered() == 0, "visible contact cannot waive the Materials cost");
    front.fund(20000.0f);
    front.run(40);
    verify(front.ai().paradrops_ordered() == 1, "AI orders one scout paradrop after seeing a forward contact");
    const auto& commands = front.simulation.command_log();
    const auto order = std::find_if(commands.begin(), commands.end(), [](const InputCommand& command) {
        return command.player_id == static_cast<uint8_t>(FactionId::MASS_WARFARE) &&
               (command.extra & Simulation::PARADROP_ORDER_FLAG) != 0;
    });
    verify(order != commands.end(), "the AI uses the recorded player paradrop command");
    verify(front.simulation.can_see(FactionId::MASS_WARFARE, order->target_x, order->target_y),
           "the selected flank point is currently watched");
    verify(front.simulation.inbound_count(FactionId::MASS_WARFARE) == 1 &&
           std::any_of(front.simulation.inbound_forces().begin(), front.simulation.inbound_forces().end(),
               [](const Simulation::InboundForce& force) {
                   return force.faction == FactionId::MASS_WARFARE && force.airdrop && force.type == static_cast<UnitType>(15);
               }), "the paid scout is in the shared cargo delivery pipeline");
    front.run(20 * 5);
    verify(front.ai().paradrops_ordered() == 1, "contact does not trigger a drop every decision");
}

namespace {
// The real battlefield and the real starting positions, with a few units on
// each side so there is fighting, scouting and production to record.
struct RecordedBattle {
    Simulation simulation;
    EntityId player_post = INVALID_ENTITY;
    EntityId enemy_post = INVALID_ENTITY;
    std::vector<EntityId> player_tanks;

    // With forces_by_transport, ground forces arrive only at an airfield and
    // each side has a Field Engineer, as on Broken Strait.
    explicit RecordedBattle(bool ai_on, bool forces_by_transport = false) {
        simulation.configure_world_size(40000.0f, 40000.0f);
        simulation.terrain().load_from_file("godot/project/scenarios/terrain.json");
        simulation.terrain().set_world_bounds(40000.0f, 40000.0f);
        simulation.start();
        simulation.enable_ai(ai_on);
        simulation.ai_manager().set_doctrine(doctrine("swarm"));
        player_post = simulation.create_faction_base(FactionId::ELITE_PRECISION, -12500.0f, 0.0f);
        enemy_post = simulation.create_faction_base(FactionId::MASS_WARFARE, -15000.0f, -6500.0f);
        simulation.begin_match(player_post, enemy_post);
        simulation.ai_manager().configure_assault(-19500.0f, -19000.0f, -5500.0f, 19000.0f, 6, 4, 24);
        for (int unit = 0; unit < 6; ++unit) {
            player_tanks.push_back(simulation.create_unit_with_type(-12500.0f + unit * 30.0f, -400.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
            simulation.create_unit_with_type(-15000.0f + unit * 30.0f, -6200.0f, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE);
        }
        if (forces_by_transport) {
            simulation.set_forces_by_transport(true);
            simulation.set_reinforcement_resources(FactionId::MASS_WARFARE, 5000.0f, 5000.0f);
            const EntityId engineer = simulation.create_unit_with_type(-14900.0f, -6300.0f, UnitType::INDUSTRIAL_ENGINEERING, FactionId::MASS_WARFARE);
            simulation.stop_unit(engineer);
        }
    }
    ~RecordedBattle() { simulation.stop(); }

    // The player's orders. Played back from the record, never repeated by hand.
    void play(int ticks, bool give_orders) {
        for (int tick = 0; tick < ticks; ++tick) {
            if (give_orders && tick == 100) simulation.issue_move_commands(player_tanks, FactionId::ELITE_PRECISION, -14000.0f, -3500.0f, 12.0f);
            if (give_orders && tick == 2400) simulation.issue_move_commands(player_tanks, FactionId::ELITE_PRECISION, -15000.0f, -6000.0f, 12.0f);
            simulation.update(50.0f);
        }
    }
};
} // namespace

TEST(a_match_record_holds_every_order_and_regular_state_hashes) {
    RecordedBattle battle(true);
    battle.play(20 * 60, true);
    const auto& record = battle.simulation.match_record();
    verify(record.recording(), "the record stays open while the match is played");
    verify(record.info().at("ai_doctrine") == "swarm", "the record names the AI style");
    verify(record.commands().size() > 10, "player and AI orders are recorded");
    verify(std::any_of(record.commands().begin(), record.commands().end(), [](const InputCommand& command) {
        return command.player_id == static_cast<uint8_t>(FactionId::ELITE_PRECISION);
    }), "the player's orders are in it");
    verify(std::any_of(record.commands().begin(), record.commands().end(), [](const InputCommand& command) {
        return command.player_id == static_cast<uint8_t>(FactionId::MASS_WARFARE) && command.cmd_type == static_cast<uint8_t>(CommandType::BUILD);
    }), "so are the AI's production orders");
    verify(record.hashes().size() == 60, "a state hash every second");
    verify(record.samples().size() == 6, "a sample every ten seconds");
    verify(record.samples().back().factions[0].fighters == 6, "the samples count the player's army");
    battle.simulation.apply_damage(battle.enemy_post, 1.0e6f);
    battle.play(1, false);
    verify(!record.recording() && record.result() == 0, "the record closes with the result");
    const std::string json = record.to_json();
    verify(data::JsonParser::parse(json).has_value(), "the record is valid JSON");
    std::vector<InputCommand> read;
    verify(MatchRecord::commands_from_json(json, read) && read.size() == record.commands().size(), "the orders read back from the file");
    verify(std::equal(read.begin(), read.end(), record.commands().begin(), [](const InputCommand& a, const InputCommand& b) {
        return a.tick_id == b.tick_id && a.entity_id == b.entity_id && a.player_id == b.player_id && a.cmd_type == b.cmd_type &&
               a.target_x == b.target_x && a.target_y == b.target_y && a.extra == b.extra;
    }), "every field survives the round trip");
}

TEST(the_same_match_plays_out_the_same_way_twice) {
    RecordedBattle first(true);
    RecordedBattle second(true);
    first.play(20 * 180, true);
    second.play(20 * 180, true);
    verify(first.simulation.match_record().hashes() == second.simulation.match_record().hashes(), "every state hash agrees");
    // The hashes must be able to tell matches apart, or agreement proves nothing.
    RecordedBattle idle(false);
    idle.play(20 * 180, false);
    const auto& played = first.simulation.match_record().hashes();
    const auto& quiet = idle.simulation.match_record().hashes();
    verify(played.size() == quiet.size() && played.front() == quiet.front(), "both start from the same state");
    verify(played.back() != quiet.back(), "a match with no orders hashes differently");
}

TEST(a_recorded_match_plays_back_exactly_with_the_ai_switched_off) {
    RecordedBattle original(true);
    original.play(20 * 180, true);
    std::vector<InputCommand> orders;
    verify(MatchRecord::commands_from_json(original.simulation.match_record().to_json(), orders), "the record reads back");
    RecordedBattle replay(false);
    replay.simulation.play_back_commands(orders);
    replay.play(20 * 180, false);
    const auto& expected = original.simulation.match_record().hashes();
    const auto& actual = replay.simulation.match_record().hashes();
    verify(expected.size() == actual.size(), "the playback is hashed at the same points");
    size_t first_difference = expected.size();
    for (size_t index = 0; index < expected.size(); ++index) {
        if (expected[index] != actual[index]) {
            first_difference = index;
            break;
        }
    }
    if (first_difference != expected.size()) {
        fprintf(stderr, "playback diverged at tick %u\n", expected[first_difference].first);
    }
    verify(first_difference == expected.size(), "the playback matches the original at every hash");
    verify(replay.simulation.match_record().commands().size() == original.simulation.match_record().commands().size(),
           "the playback ran exactly the recorded orders");
}

TEST(an_ai_built_airfield_plays_back_from_the_record) {
    RecordedBattle original(true, true);
    original.play(20 * 180, true);
    const auto& territory = original.simulation.territorial_control_manager();
    verify(original.simulation.ai_manager().airfield_orders() == 1, "the AI orders one airfield on the real terrain");
    verify(territory.has_active_installation(FactionId::MASS_WARFARE, InstallationType::AIRFIELD), "and it is finished within three minutes");
    const auto& commands = original.simulation.match_record().commands();
    verify(std::any_of(commands.begin(), commands.end(), [](const InputCommand& command) {
        return command.player_id == static_cast<uint8_t>(FactionId::MASS_WARFARE) &&
               command.cmd_type == static_cast<uint8_t>(CommandType::BUILD) && (command.extra & Simulation::STRUCTURE_ORDER_FLAG) != 0;
    }), "the airfield order is in the record like any other order");
    std::vector<InputCommand> orders;
    verify(MatchRecord::commands_from_json(original.simulation.match_record().to_json(), orders), "the record reads back");
    RecordedBattle replay(false, true);
    replay.simulation.play_back_commands(orders);
    replay.play(20 * 180, false);
    verify(replay.simulation.territorial_control_manager().has_active_installation(FactionId::MASS_WARFARE, InstallationType::AIRFIELD),
           "the playback builds the same airfield with the AI switched off");
    verify(replay.simulation.match_record().hashes() == original.simulation.match_record().hashes(),
           "and matches the original at every hash");
}
