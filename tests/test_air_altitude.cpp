// High-altitude strategic recon: ordinary fighters and ground guns cannot reach
// it, advanced anti-air can. All checks run the real combat and projectile code
// on unit data from data/unit_faction_stats.json.
#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "combat/engagement_altitude.hpp"
#include "ecs/components/aircraft.hpp"
#include "ecs/components/weapon.hpp"

#include <algorithm>
#include <cmath>
#include <string>

using namespace rts;

namespace {
constexpr auto STRATEGIC_RECON = static_cast<UnitType>(20);
constexpr auto LONGSIGHT_RECON = static_cast<UnitType>(14);

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

EntityId spawn(Simulation& s, UnitType type, FactionId faction, float x, float y) {
    const int id = s.create_unit_with_type(x, y, type, faction);
    require(id >= 0, "unit " + std::to_string(static_cast<int>(type)) + " spawns");
    const auto entity = static_cast<EntityId>(id);
    if (auto* aircraft = s.component_manager().get_component<Aircraft>(entity))
        aircraft->status = Aircraft::Status::AIRBORNE;
    if (auto* weapon = s.component_manager().get_component<Weapon>(entity))
        weapon->cooldown_remaining = 0.0f;
    return entity;
}

// One shooter against one target 60 m away, out of the target's own 40 m reach.
// Returns the target's health after five seconds of combat.
struct Duel { size_t shots; float target_health; float target_max; };
Duel duel(UnitType target_type, UnitType shooter_type) {
    Simulation s; s.start();
    const EntityId target = spawn(s, target_type, FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    spawn(s, shooter_type, FactionId::MASS_WARFARE, 60.0f, 0.0f);
    auto& combat = s.combat_manager();
    size_t shots = 0;
    for (int step = 0; step < 100; ++step) {
        const size_t before = combat.projectile_manager().total_spawned();
        combat.update(50.0f, s.spatial_grid(), s.component_manager());
        shots += combat.projectile_manager().total_spawned() - before;
    }
    const auto* health = s.component_manager().get_component<Health>(target);
    return {shots, health->current, health->max};
}
} // namespace

TEST(strategic_recon_unit_data_flies_above_the_default_ceiling) {
    const auto& prototypes = get_unit_prototypes();
    const auto& recon = prototypes.at(STRATEGIC_RECON);
    require(recon.is_aircraft && recon.recon_loiter_seconds > 0.0f, "strategic recon is a flight-plan recon plane");
    require(recon.flight_altitude == 24000.0f, "strategic recon cruises at 24,000 m");
    require(recon.flight_altitude > DEFAULT_WEAPON_CEILING_METERS, "strategic recon flies above the default weapon ceiling");
    require(prototypes.at(LONGSIGHT_RECON).flight_altitude == DEFAULT_FLIGHT_ALTITUDE_METERS,
            "ordinary recon keeps the default altitude");
    require(prototypes.at(UnitType::ELITE_T1_FIGHTER).weapon.hits_air &&
                prototypes.at(UnitType::ELITE_T1_FIGHTER).weapon.max_target_altitude < recon.flight_altitude,
            "the T1 fighter's weapon hits air but cannot reach strategic recon");
    for (const auto aa : {UnitType::ELITE_ANTI_AIR, UnitType::MASS_ANTI_AIR})
        require(prototypes.at(aa).weapon.hits_air && prototypes.at(aa).weapon.max_target_altitude >= recon.flight_altitude,
                "advanced anti-air reaches strategic recon");
}

TEST(strategic_recon_altitude_applies_only_in_flight) {
    Simulation s; s.start();
    const int id = s.create_unit_with_type(0.0f, 0.0f, STRATEGIC_RECON, FactionId::ELITE_PRECISION);
    require(id >= 0, "strategic recon spawns");
    auto& cm = s.component_manager();
    const auto entity = static_cast<EntityId>(id);
    require(engagement_altitude(entity, cm) == 0.0f, "a plane on the ground is at zero altitude");
    cm.get_component<Aircraft>(entity)->status = Aircraft::Status::AIRBORNE;
    require(engagement_altitude(entity, cm) == 24000.0f, "a plane in flight is at its cruise altitude");
}

TEST(tier_one_fighter_cannot_engage_strategic_recon) {
    const Duel result = duel(STRATEGIC_RECON, UnitType::ELITE_T1_FIGHTER);
    require(result.shots == 0, "the T1 fighter fires no shots at strategic recon, fired " + std::to_string(result.shots));
    require(result.target_health == result.target_max, "strategic recon takes no damage from the T1 fighter");
}

TEST(tier_one_fighter_still_engages_ordinary_recon) {
    // Control: the same fighter at the same range does shoot a plane at normal altitude.
    const Duel result = duel(LONGSIGHT_RECON, UnitType::ELITE_T1_FIGHTER);
    require(result.shots > 0, "the T1 fighter fires at ordinary recon");
    require(result.target_health < result.target_max, "ordinary recon is damaged by the T1 fighter");
}

TEST(vtol_and_ground_guns_cannot_engage_strategic_recon) {
    // The VTOL's weapon hits air but not this high; the tanks do not hit air at all.
    for (const auto shooter : {UnitType::ELITE_T1_VTOL, UnitType::ELITE_MAIN_BATTLE_TANK, UnitType::MASS_SWARM_TANK}) {
        const Duel result = duel(STRATEGIC_RECON, shooter);
        require(result.shots == 0 && result.target_health == result.target_max,
                "unit " + std::to_string(static_cast<int>(shooter)) + " cannot engage strategic recon");
    }
}

TEST(advanced_anti_air_engages_and_damages_strategic_recon) {
    for (const auto shooter : {UnitType::MASS_ANTI_AIR, UnitType::ELITE_ANTI_AIR}) {
        const Duel result = duel(STRATEGIC_RECON, shooter);
        require(result.shots > 0, "anti-air " + std::to_string(static_cast<int>(shooter)) + " fires at strategic recon");
        require(result.target_health < result.target_max,
                "anti-air " + std::to_string(static_cast<int>(shooter)) + " damages strategic recon");
    }
}

TEST(explicit_attack_order_on_strategic_recon_is_out_of_reach) {
    Simulation s; s.start();
    const EntityId recon = spawn(s, STRATEGIC_RECON, FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    const EntityId fighter = spawn(s, UnitType::ELITE_T1_FIGHTER, FactionId::MASS_WARFARE, 60.0f, 0.0f);
    auto& cm = s.component_manager();
    auto& combat = s.combat_manager();
    const float fighter_ceiling = cm.get_component<Weapon>(fighter)->max_target_altitude;
    combat.start_attack(fighter, recon, cm);
    const auto solution = combat.find_target(60.0f, 0.0f, 120.0f, FactionId::MASS_WARFARE, cm, recon, true, true, fighter_ceiling);
    require(!solution.valid, "an attack order does not let a fighter reach strategic recon");
    const auto aa = combat.find_target(60.0f, 0.0f, 120.0f, FactionId::MASS_WARFARE, cm, recon, true, true, 30000.0f);
    require(aa.valid && aa.target_id == recon, "the same order is valid for a weapon that reaches 30,000 m");
}

TEST(stray_ground_fire_passes_under_strategic_recon) {
    Simulation s; s.start();
    const EntityId recon = spawn(s, STRATEGIC_RECON, FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    auto& cm = s.component_manager();
    auto& projectiles = s.combat_manager().projectile_manager();
    projectiles.spawn(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 50.0f, 5.0f, 1.0f, FactionId::MASS_WARFARE, true, true);
    projectiles.update(50.0f, s.spatial_grid(), cm);
    require(cm.get_component<Health>(recon)->current == cm.get_component<Health>(recon)->max,
            "an anti-air burst with the default ceiling does not hit strategic recon overhead");
    projectiles.spawn(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 50.0f, 5.0f, 1.0f, FactionId::MASS_WARFARE, true, true, 30000.0f);
    projectiles.update(50.0f, s.spatial_grid(), cm);
    require(cm.get_component<Health>(recon)->current < cm.get_component<Health>(recon)->max,
            "a burst from advanced anti-air does hit it");
}

// A recon plane flies a flight plan, but a move order re-plots it: it flies to
// the new point and spends the rest of its time on station there. It still takes
// no other order (queued moves, stops), so it never joins the fighting line.
TEST(recon_plane_move_order_replots_its_flight) {
    Simulation s;
    s.configure_world_size(8000.0f, 8000.0f);
    s.start();
    require(s.create_faction_base(FactionId::ELITE_PRECISION, 3000.0f, 3000.0f) != INVALID_ENTITY, "the command post is placed");
    require(s.order_unit(FactionId::ELITE_PRECISION, LONGSIGHT_RECON, 1000.0f, 1000.0f), "a recon flight is ordered");
    EntityId plane = INVALID_ENTITY;
    for (int tick = 0; tick < 2000 && plane == INVALID_ENTITY; ++tick) {
        s.update(50.0f);
        for (const auto id : s.get_entity_list())
            if (s.on_recon_flight(id)) plane = id;
    }
    require(plane != INVALID_ENTITY, "the recon plane arrives on its flight plan");
    require(s.issue_move_commands({plane}, FactionId::ELITE_PRECISION, -2000.0f, 2500.0f, 3.0f, true) == 0,
            "a queued move is refused: the flight has one point");
    require(s.issue_stop_commands({plane}, FactionId::ELITE_PRECISION) == 0, "a stop order is refused");
    require(s.issue_move_commands({plane}, FactionId::ELITE_PRECISION, -2000.0f, 2500.0f, 3.0f) == 1,
            "a move order to a recon plane is accepted");
    float closest = 1.0e9f;
    for (int tick = 0; tick < 1200; ++tick) {
        s.update(50.0f);
        if (!s.on_recon_flight(plane)) break;
        closest = std::min(closest, std::hypot(s.get_unit_x(plane) + 2000.0f, s.get_unit_y(plane) - 2500.0f));
    }
    require(closest < 600.0f, "the plane flies to the new point (closest " + std::to_string(closest) + " m)");
    // On station it circles the new point, then leaves the map when its time is up.
    float farthest_on_station = 0.0f;
    bool left = false;
    for (int tick = 0; tick < 4000 && !left; ++tick) {
        s.update(50.0f);
        const auto entities = s.get_entity_list();
        left = std::find(entities.begin(), entities.end(), plane) == entities.end();
        if (!left && s.on_recon_flight(plane) && s.get_unit_x(plane) > -2700.0f)
            farthest_on_station = std::max(farthest_on_station, std::hypot(s.get_unit_x(plane) + 2000.0f, s.get_unit_y(plane) - 2500.0f));
    }
    require(farthest_on_station < 900.0f, "it circles the new point, not the first one (farthest " + std::to_string(farthest_on_station) + " m)");
    require(left, "and leaves the map when its time on station ends");
}
