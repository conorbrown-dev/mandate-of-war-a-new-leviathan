#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/faction.hpp"
#include <limits>

using namespace rts;

extern "C" {
void simulation_start();
void simulation_update(float delta_ms);
int simulation_create_unit_with_type(float x, float y, int unit_type, int faction_id);
}

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

EntityId spawn(Simulation& sim, float x, FactionId faction, float vision = 20.0f) {
    const auto entity = sim.create_unit(x, 0.0f);
    sim.set_unit_faction(entity.id, faction);
    sim.component_manager().add_component(entity.id, UnitData{10.0f, vision});
    return entity.id;
}
}

TEST(ai_visibility_is_faction_scoped_and_sensor_limited) {
    Simulation sim;
    sim.start();
    sim.ai_manager().set_faction_id(FactionId::MASS_WARFARE);
    const auto observer = spawn(sim, 0, FactionId::MASS_WARFARE);
    spawn(sim, 1, FactionId::MASS_WARFARE);
    const auto boundary = spawn(sim, 20, FactionId::ELITE_PRECISION);
    spawn(sim, 100, FactionId::ELITE_PRECISION);
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_visible_units().size() == 2, "Only own units are controllable");
    require(sim.ai_manager().get_enemy_units() == std::vector<EntityId>{boundary}, "Hidden enemies must not enter AI perception; sensor boundary is inclusive");
    sim.destroy_unit(observer);
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_enemy_units() == std::vector<EntityId>{boundary}, "Overlapping friendly sensor retains detection");
    sim.component_manager().get_component<Position>(boundary)->x = 100;
    sim.spatial_grid().update(boundary, 100, 0);
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_enemy_units().empty(), "Enemy moving out of sensor range is no longer observed");
    sim.component_manager().get_component<Position>(boundary)->x = 20;
    sim.spatial_grid().update(boundary, 20, 0);
    sim.component_manager().get_component<UnitData>(sim.ai_manager().get_visible_units()[0])->view_range = 0;
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_enemy_units().empty(), "Loss of final sensor removes current enemy visibility");
}

TEST(ai_visibility_rejects_dead_and_invalid_sensors) {
    Simulation sim;
    sim.start();
    const auto observer = spawn(sim, 0, FactionId::MASS_WARFARE);
    const auto enemy = spawn(sim, 10, FactionId::ELITE_PRECISION);
    sim.ai_manager().set_faction_id(FactionId::MASS_WARFARE);
    sim.component_manager().get_component<UnitData>(observer)->view_range = std::numeric_limits<float>::quiet_NaN();
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_enemy_units().empty(), "Non-finite sensor range must fail closed");
    sim.component_manager().get_component<UnitData>(observer)->view_range = 20;
    sim.component_manager().get_component<Health>(enemy)->is_dead = true;
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_enemy_units().empty(), "Dead enemy must not be perceived as active");
    sim.component_manager().get_component<Health>(enemy)->is_dead = false;
    sim.component_manager().get_component<Health>(observer)->current = 0;
    sim.ai_manager().update(1000);
    require(sim.ai_manager().get_visible_units().empty(), "Dead observer must not remain controllable");
    require(sim.ai_manager().get_enemy_units().empty(), "Dead observer must not reveal enemies");
}

TEST(ai_visibility_reset_faction_and_invalid_delta) {
    Simulation sim;
    sim.start();
    spawn(sim, 0, FactionId::MASS_WARFARE);
    spawn(sim, 10, FactionId::ELITE_PRECISION);
    sim.ai_manager().update(std::numeric_limits<float>::quiet_NaN());
    sim.ai_manager().update(std::numeric_limits<float>::infinity());
    sim.ai_manager().update(-1000);
    sim.ai_manager().update(500);
    require(sim.ai_manager().get_visible_units().empty(), "Invalid delta must not advance decision clock");
    sim.ai_manager().update(500);
    require(sim.ai_manager().get_visible_units().size() == 1, "Valid deltas after NaN must still drive decisions");
    sim.ai_manager().set_faction_id(FactionId::ELITE_PRECISION);
    require(sim.ai_manager().get_visible_units().empty() && sim.ai_manager().get_enemy_units().empty(), "Faction change invalidates old observations immediately");
    sim.ai_manager().update(1000);
    sim.reset();
    require(sim.ai_manager().get_visible_units().empty() && sim.ai_manager().get_enemy_units().empty(), "Rematch clears AI observations");
    spawn(sim, 0, FactionId::MASS_WARFARE);
    sim.ai_manager().update(999);
    require(sim.ai_manager().get_visible_units().empty(), "Rematch resets decision timer");
}

TEST(ai_visibility_instances_and_order_are_deterministic) {
    Simulation first, second;
    first.start();
    second.start();
    spawn(first, 0, FactionId::MASS_WARFARE);
    spawn(second, 0, FactionId::MASS_WARFARE);
    const auto a = spawn(first, 10, FactionId::ELITE_PRECISION);
    const auto b = spawn(first, -10, FactionId::ELITE_PRECISION);
    spawn(second, 10, FactionId::ELITE_PRECISION);
    spawn(second, -10, FactionId::ELITE_PRECISION);
    // Reverse spatial insertion order without changing the state.
    first.spatial_grid().insert(b, -10, 0);
    first.spatial_grid().insert(a, 10, 0);
    first.ai_manager().update(1000);
    second.ai_manager().update(1000);
    require(first.ai_manager().get_enemy_units() == std::vector<EntityId>({a, b}), "Perception must be sorted and deduplicated");
    require(first.ai_manager().get_enemy_units() == second.ai_manager().get_enemy_units(), "Equal worlds produce identical perceptions");
    first.ai_manager().reset();
    require(second.ai_manager().get_enemy_units().size() == 2, "AI reset must not affect another simulation");
}

TEST(ai_c_api_uses_simulation_owned_manager) {
    simulation_start();
    ai_init();
    ai_set_faction_id(1);
    require(simulation_create_unit_with_type(0, 0, 3, 1) >= 0, "AI typed unit spawns");
    require(simulation_create_unit_with_type(1, 0, 0, 0) >= 0, "Nearby enemy spawns");
    require(simulation_create_unit_with_type(300, 0, 0, 0) >= 0, "Hidden enemy spawns");
    ai_update(1000);
    require(ai_get_visible_unit_count() == 1, "C API observes simulation-owned faction units");
    require(ai_get_enemy_unit_count() == 1, "Typed units use prototype vision, not global enemy access");
    ai_set_faction_id(257);
    require(ai_get_visible_unit_count() == 1, "Invalid faction must not wrap into valid identity");
    ai_reset();
    require(ai_get_visible_unit_count() == 0 && ai_get_enemy_unit_count() == 0, "C API reset clears same manager");
    for (int tick = 0; tick < 20; ++tick) simulation_update(50);
    require(ai_get_visible_unit_count() == 1, "Fixed simulation ticks drive the exported manager");
    simulation_start();
    require(ai_get_visible_unit_count() == 0 && ai_get_enemy_unit_count() == 0, "Simulation restart clears exported AI state");
}
