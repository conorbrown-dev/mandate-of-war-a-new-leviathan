// The tactical missile launchers (issue #25): one per faction. Their unit data,
// and the real combat and projectile code firing them at ground targets.
// Everything reads data/unit_faction_stats.json.
#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/weapon.hpp"

#include <string>

using namespace rts;

namespace {
constexpr auto INDUSTRIAL_LAUNCHER = static_cast<UnitType>(26);
constexpr auto ELITE_LAUNCHER = static_cast<UnitType>(27);
constexpr auto MASS_LAUNCHER = static_cast<UnitType>(28);

struct Launcher { UnitType type; FactionId faction; float range; float damage; float blast; float fire_rate;
                  UnitType target_type; FactionId target_faction; };
const Launcher LAUNCHERS[] = {
    {INDUSTRIAL_LAUNCHER, FactionId::INDUSTRIAL_EXPERIMENTAL, 260.0f, 140.0f, 12.0f, 14.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION},
    {ELITE_LAUNCHER, FactionId::ELITE_PRECISION, 300.0f, 160.0f, 8.0f, 16.0f, UnitType::INDUSTRIAL_MBT, FactionId::INDUSTRIAL_EXPERIMENTAL},
    {MASS_LAUNCHER, FactionId::MASS_WARFARE, 220.0f, 110.0f, 16.0f, 11.0f, UnitType::INDUSTRIAL_MBT, FactionId::INDUSTRIAL_EXPERIMENTAL},
};

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

EntityId spawn(Simulation& s, UnitType type, FactionId faction, float x, float y) {
    const int id = s.create_unit_with_type(x, y, type, faction);
    require(id >= 0, "unit " + std::to_string(static_cast<int>(type)) + " spawns");
    const auto entity = static_cast<EntityId>(id);
    if (auto* weapon = s.component_manager().get_component<Weapon>(entity)) weapon->cooldown_remaining = 0.0f;
    return entity;
}

float health_of(Simulation& s, EntityId id) {
    return s.component_manager().get_component<Health>(id)->current;
}

// Runs combat for the given number of 50 ms steps and returns shots fired.
size_t fight(Simulation& s, int steps) {
    auto& combat = s.combat_manager();
    size_t shots = 0;
    for (int step = 0; step < steps; ++step) {
        const size_t before = combat.projectile_manager().total_spawned();
        combat.update(50.0f, s.spatial_grid(), s.component_manager());
        shots += combat.projectile_manager().total_spawned() - before;
    }
    return shots;
}

std::string name_of(const Launcher& l) { return "launcher " + std::to_string(static_cast<int>(l.type)); }
} // namespace

TEST(every_faction_has_a_tactical_missile_launcher) {
    const auto& prototypes = get_unit_prototypes();
    for (const auto& l : LAUNCHERS) {
        const auto found = prototypes.find(l.type);
        require(found != prototypes.end(), name_of(l) + " exists in the unit data");
        const auto& unit = found->second;
        require(unit.faction == l.faction, name_of(l) + " belongs to its own faction");
        require(unit.weapon.range == l.range && unit.weapon.damage == l.damage && unit.weapon.aoe_radius == l.blast,
                name_of(l) + " has its documented range, damage and blast");
        require(unit.weapon.range > prototypes.at(UnitType::INDUSTRIAL_MISSILE_PLATFORM).weapon.range - 1.0f || l.type == MASS_LAUNCHER,
                name_of(l) + " reaches at least as far as the Missile Platform, except the Mass launcher");
        require(unit.hp <= 200.0f, name_of(l) + " is fragile");
        require(unit.support_unit, name_of(l) + " is a support unit the AI does not mass-produce");
    }
    require(prototypes.at(ELITE_LAUNCHER).weapon.range > prototypes.at(INDUSTRIAL_LAUNCHER).weapon.range &&
                prototypes.at(INDUSTRIAL_LAUNCHER).weapon.range > prototypes.at(MASS_LAUNCHER).weapon.range,
            "Elite outranges Industrial, which outranges Mass");
    require(prototypes.at(ELITE_LAUNCHER).material_cost > prototypes.at(INDUSTRIAL_LAUNCHER).material_cost &&
                prototypes.at(INDUSTRIAL_LAUNCHER).material_cost > prototypes.at(MASS_LAUNCHER).material_cost,
            "Elite costs most and Mass least");
}

TEST(tactical_missile_hits_a_ground_target_inside_its_range) {
    for (const auto& l : LAUNCHERS) {
        Simulation s; s.start();
        const EntityId target = spawn(s, l.target_type, l.target_faction, 0.0f, 0.0f);
        const float before = health_of(s, target);
        spawn(s, l.type, l.faction, l.range - 20.0f, 0.0f);
        const size_t shots = fight(s, 100); // five seconds
        require(shots == 1, name_of(l) + " fires one missile in five seconds, fired " + std::to_string(shots));
        const float lost = before - health_of(s, target);
        require(lost > 0.0f && lost <= l.damage, name_of(l) + " damages the target by up to " + std::to_string(l.damage) +
                                                    ", lost " + std::to_string(lost));
    }
}

TEST(tactical_missile_does_not_fire_beyond_its_range) {
    for (const auto& l : LAUNCHERS) {
        Simulation s; s.start();
        const EntityId target = spawn(s, l.target_type, l.target_faction, 0.0f, 0.0f);
        const float before = health_of(s, target);
        spawn(s, l.type, l.faction, l.range + 20.0f, 0.0f);
        require(fight(s, 100) == 0, name_of(l) + " does not fire 20 m beyond its range");
        require(health_of(s, target) == before, name_of(l) + " leaves the target untouched beyond its range");
    }
}

TEST(tactical_missile_blast_hits_neighbours_but_not_distant_units) {
    for (const auto& l : LAUNCHERS) {
        Simulation s; s.start();
        const EntityId primary = spawn(s, l.target_type, l.target_faction, 0.0f, 0.0f);
        const EntityId near = spawn(s, l.target_type, l.target_faction, 0.0f, 3.0f);
        const EntityId far = spawn(s, l.target_type, l.target_faction, 0.0f, 60.0f);
        const float full_primary = health_of(s, primary), full_near = health_of(s, near), full_far = health_of(s, far);
        spawn(s, l.type, l.faction, l.range - 20.0f, 0.0f);
        fight(s, 100);
        require(health_of(s, primary) < full_primary || health_of(s, near) < full_near, name_of(l) + " hits the cluster");
        require(health_of(s, far) == full_far, name_of(l) + ": a unit 60 m away is outside the blast");
    }
}
