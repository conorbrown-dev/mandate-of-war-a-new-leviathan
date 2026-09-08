#include "test_framework.hpp"
#include "ecs/components/resources.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/faction.hpp"
#include "combat/projectile_manager.hpp"
#include "combat/combat_manager.hpp"
#include "spatial/spatial_grid.hpp"
#include "simulation/simulation.hpp"
#include "ecs/entity.hpp"

TEST(projectile_manager_spawns_projectiles) {
    using namespace rts;
    
    ProjectileManager pm(100);
    
    pm.spawn(100.0f, 100.0f, 1.0f, 0.0f, 50.0f, 25.0f, 10.0f, 2.0f, FactionId::ELITE_PRECISION);
    
    if (pm.active_count() != 1) {
        throw std::runtime_error("Should have 1 active projectile");
    }
}

TEST(projectile_manager_updates_trajectory) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(100.0f);
    ComponentManager cm;
    
    pm.spawn(100.0f, 100.0f, 1.0f, 0.0f, 50.0f, 25.0f, 10.0f, 0.5f, FactionId::ELITE_PRECISION);
    
    pm.update(1000.0f, grid, cm);
    
    if (pm.active_count() != 0) {
        throw std::runtime_error("Projectile should expire after 0.5 second lifetime");
    }
}

TEST(projectile_manager_apply_damage) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(100.0f);
    ComponentManager cm;
    
    auto entity = EntityManager().create_entity();
    cm.add_component<Position>(entity.id, Position{100.0f, 100.0f, 0.0f});
    cm.add_component<Health>(entity.id, Health{100.0f, 100.0f, false});
    grid.insert(entity.id, 100.0f, 100.0f);
    
    auto entities_before = grid.query_in_region(100.0f, 100.0f, 15.0f);
    (void)entities_before;
    
    pm.spawn(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 25.0f, 15.0f, 2.0f, FactionId::ELITE_PRECISION);
    
    pm.update(50.0f, grid, cm);
    
    auto entities_after = grid.query_in_region(100.0f, 100.0f, 15.0f);
    size_t projectile_count = pm.active_count();
    
    auto* health = cm.get_component<Health>(entity.id);
    if (!health) {
        throw std::runtime_error("Health component should exist");
    }
    if (health->current != 75.0f) {
        std::string msg = "Health should be reduced by 25, got " + std::to_string(health->current) + 
                         " (entities_before=" + std::to_string(entities_before.size()) +
                         ", entities_after=" + std::to_string(entities_after.size()) +
                         ", projectiles=" + std::to_string(projectile_count) + ")";
        throw std::runtime_error(msg);
    }
}

TEST(projectile_manager_aoe_damage) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(100.0f);
    ComponentManager cm;
    EntityManager em;
    
    auto e1 = em.create_entity();
    cm.add_component<Position>(e1.id, Position{100.0f, 100.0f, 0.0f});
    cm.add_component<Health>(e1.id, Health{100.0f, 100.0f, false});
    grid.insert(e1.id, 100.0f, 100.0f);
    
    auto e2 = em.create_entity();
    cm.add_component<Position>(e2.id, Position{105.0f, 105.0f, 0.0f});
    cm.add_component<Health>(e2.id, Health{100.0f, 100.0f, false});
    grid.insert(e2.id, 105.0f, 105.0f);
    
    auto e3 = em.create_entity();
    cm.add_component<Position>(e3.id, Position{200.0f, 200.0f, 0.0f});
    cm.add_component<Health>(e3.id, Health{100.0f, 100.0f, false});
    grid.insert(e3.id, 200.0f, 200.0f);
    
    pm.spawn(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 25.0f, 15.0f, 2.0f, FactionId::ELITE_PRECISION);
    
    pm.update(50.0f, grid, cm);
    
    auto* h1 = cm.get_component<Health>(e1.id);
    auto* h2 = cm.get_component<Health>(e2.id);
    auto* h3 = cm.get_component<Health>(e3.id);
    
    std::string msg;
    if (!h1) {
        msg = "e1 health component not found";
        throw std::runtime_error(msg);
    }
    if (!h2) {
        msg = "e2 health component not found";
        throw std::runtime_error(msg);
    }
    if (!h3) {
        msg = "e3 health component not found";
        throw std::runtime_error(msg);
    }
    
    if (h1->current != 75.0f) {
        msg = "e1 (in center) should take damage, got " + std::to_string(h1->current);
        throw std::runtime_error(msg);
    }
    if (h2->current != 75.0f) {
        msg = "e2 (in AoE) should take damage, got " + std::to_string(h2->current);
        throw std::runtime_error(msg);
    }
    if (h3->current != 100.0f) {
        msg = "e3 (outside AoE) should not take damage, got " + std::to_string(h3->current);
        throw std::runtime_error(msg);
    }
}

TEST(projectile_manager_expires_lifetime) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(100.0f);
    ComponentManager cm;
    
    pm.spawn(100.0f, 100.0f, 1.0f, 0.0f, 50.0f, 25.0f, 10.0f, 0.1f, FactionId::ELITE_PRECISION);
    
    pm.update(50.0f, grid, cm);
    
    if (pm.active_count() != 1) {
        throw std::runtime_error("Projectile should still be active after 50ms (lifetime=100ms)");
    }
    
    pm.update(50.0f, grid, cm);
    
    if (pm.active_count() != 0) {
        throw std::runtime_error("Projectile should expire after 100ms");
    }
}

TEST(combat_manager_integration) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    Entity e1 = sim.create_unit(100.0f, 100.0f);
    Entity e2 = sim.create_unit(200.0f, 200.0f);
    
    sim.combat_manager().update(50.0f, sim.spatial_grid(), sim.component_manager());
    
    size_t entity_count = sim.entity_count();
    if (entity_count != 2) {
        throw std::runtime_error("Should have 2 entities after combat phase");
    }

    auto* weapon = sim.component_manager().get_component<Weapon>(e1.id);
    if (!weapon || weapon->cooldown_remaining != 0.0f) {
        throw std::runtime_error("Single-faction combat should clamp ready cooldowns at zero");
    }

    if (sim.combat_manager().projectile_manager().active_count() != 0) {
        throw std::runtime_error("Single-faction combat should not spawn projectiles");
    }
}

TEST(combat_manager_opposing_units_fire_automatically) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    Entity e1 = sim.create_unit(100.0f, 100.0f);
    Entity e2 = sim.create_unit(150.0f, 150.0f);
    sim.set_unit_faction(e2.id, FactionId::MASS_WARFARE);
    
    ComponentManager& cm = sim.component_manager();
    
    cm.add_component<Health>(e1.id, Health{100.0f, 100.0f, false});
    cm.add_component<Health>(e2.id, Health{100.0f, 100.0f, false});
    
    sim.combat_manager().update(50.0f, sim.spatial_grid(), cm);
    
    size_t projectile_count = sim.combat_manager().projectile_manager().active_count();
    
    if (projectile_count != 2) {
        throw std::runtime_error("Opposing ready units should each spawn one projectile");
    }
}

TEST(combat_manager_empty_after_update) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    Entity e1 = sim.create_unit(100.0f, 100.0f);
    
    sim.combat_manager().update(50.0f, sim.spatial_grid(), sim.component_manager());
    
    size_t projectile_count = sim.combat_manager().projectile_manager().active_count();
    
    if (projectile_count != 0) {
        throw std::runtime_error("Projectile count should be 0 after combat update");
    }
}

TEST(projectile_manager_health_death) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(100.0f);
    ComponentManager cm;
    
    auto entity = EntityManager().create_entity();
    cm.add_component<Position>(entity.id, Position{100.0f, 100.0f, 0.0f});
    cm.add_component<Health>(entity.id, Health{20.0f, 100.0f, false});
    grid.insert(entity.id, 100.0f, 100.0f);
    
    pm.spawn(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 25.0f, 15.0f, 2.0f, FactionId::ELITE_PRECISION);
    pm.update(50.0f, grid, cm);
    
    auto* health = cm.get_component<Health>(entity.id);
    if (!health || !health->is_dead) {
        throw std::runtime_error("Unit should be dead after taking 25 damage with 20 HP");
    }
}

TEST(intercept_calculator_target_faster_than_projectile) {
    using namespace rts;
    
    InterceptCalculator calc;
    
    InterceptSolution solution = calc.calculate_intercept(
        0.0f, 0.0f, 10.0f,
        100.0f, 0.0f, 12.0f, 0.0f
    );
    
    if (solution.valid) {
        throw std::runtime_error("Should be invalid when target speed > projectile speed");
    }
}

TEST(intercept_calculator_same_speed_as_projectile) {
    using namespace rts;
    
    InterceptCalculator calc;
    
    InterceptSolution solution = calc.calculate_intercept(
        0.0f, 0.0f, 10.0f,
        100.0f, 0.0f, 10.0f, 0.0f
    );
    
    if (solution.valid) {
        throw std::runtime_error("Should be invalid when target speed equals projectile speed (edge case)");
    }
}

TEST(projectile_manager_source_filtering) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(200.0f);
    ComponentManager cm;
    
    fprintf(stderr, "=== BEGIN TEST: projectile_manager_source_filtering ===\n");
    
    EntityManager em;
    auto shooter = em.create_entity();
    fprintf(stderr, "shooter entity ID = %d\n", shooter.id);
    cm.add_component<Position>(shooter.id, Position{100.0f, 100.0f, 0.0f});
    cm.add_component<Health>(shooter.id, Health{100.0f, 100.0f, false});
    cm.add_component<Faction>(shooter.id, Faction{FactionId::ELITE_PRECISION});
    grid.insert(shooter.id, 100.0f, 100.0f);
    
    auto target = em.create_entity();
    fprintf(stderr, "target entity ID = %d\n", target.id);
    cm.add_component<Position>(target.id, Position{150.0f, 100.0f, 0.0f});
    cm.add_component<Health>(target.id, Health{100.0f, 100.0f, false});
    grid.insert(target.id, 150.0f, 100.0f);
    
    printf("Before update: shooter at (100,100), target at (150,100)\n");
    pm.spawn(100.0f, 100.0f, 1.0f, 0.0f, 50.0f, 50.0f, 20.0f, 2.0f, FactionId::ELITE_PRECISION);
    
    printf("After spawn: projectile count = %zu\n", pm.active_count());
    
    pm.update(1000.0f, grid, cm);
    
    auto* shooter_health = cm.get_component<Health>(shooter.id);
    auto* target_health = cm.get_component<Health>(target.id);
    
    printf("After update: shooter_health=%p, target_health=%p\n", shooter_health, target_health);
    if (shooter_health) printf("shooter health = %f\n", shooter_health->current);
    if (target_health) printf("target health = %f\n", target_health->current);
    
    if (!shooter_health || !target_health) {
        throw std::runtime_error("Health components should exist");
    }
    
    if (shooter_health->current != 100.0f) {
        std::string error = "Shooter (same faction) should not take damage, got " + std::to_string(shooter_health->current);
        throw std::runtime_error(error);
    }
    if (target_health->current != 50.0f) {
        std::string error = "Target (no faction) should take damage equal to projectile damage (50), got " + std::to_string(target_health->current);
        throw std::runtime_error(error);
    }
}

TEST(projectile_manager_faction_filtering) {
    using namespace rts;
    
    fprintf(stderr, "=== BEGIN TEST: projectile_manager_faction_filtering ===\n");
    
    ProjectileManager pm(100);
    SpatialGrid grid(200.0f);
    ComponentManager cm;
    
    EntityManager em;
    
    auto e1 = em.create_entity();
    fprintf(stderr, "e1 ID = %d\n", e1.id);
    cm.add_component<Position>(e1.id, Position{100.0f, 100.0f, 0.0f});
    cm.add_component<Health>(e1.id, Health{100.0f, 100.0f, false});
    cm.add_component<Faction>(e1.id, Faction{FactionId::ELITE_PRECISION});
    grid.insert(e1.id, 100.0f, 100.0f);
    
    auto e2 = em.create_entity();
    fprintf(stderr, "e2 ID = %d\n", e2.id);
    cm.add_component<Position>(e2.id, Position{102.0f, 102.0f, 0.0f});
    cm.add_component<Health>(e2.id, Health{100.0f, 100.0f, false});
    cm.add_component<Faction>(e2.id, Faction{FactionId::ELITE_PRECISION});
    grid.insert(e2.id, 102.0f, 102.0f);
    
    auto e3 = em.create_entity();
    fprintf(stderr, "e3 ID = %d\n", e3.id);
    cm.add_component<Position>(e3.id, Position{135.0f, 135.0f, 0.0f});
    cm.add_component<Health>(e3.id, Health{100.0f, 100.0f, false});
    cm.add_component<Faction>(e3.id, Faction{FactionId::MASS_WARFARE});
    grid.insert(e3.id, 135.0f, 135.0f);
    
    pm.spawn(100.0f, 100.0f, 0.7071f, 0.7071f, 50.0f, 25.0f, 20.0f, 2.0f, FactionId::ELITE_PRECISION);
    pm.update(1000.0f, grid, cm);
    
    auto* h1 = cm.get_component<Health>(e1.id);
    auto* h2 = cm.get_component<Health>(e2.id);
    auto* h3 = cm.get_component<Health>(e3.id);
    
    if (!h1 || !h2 || !h3) {
        throw std::runtime_error("Health components should exist");
    }
    
    fprintf(stderr, "e1 health = %.2f\n", h1->current);
    fprintf(stderr, "e2 health = %.2f\n", h2->current);
    fprintf(stderr, "e3 health = %.2f\n", h3->current);
    
    if (h1->current != 100.0f) {
        throw std::runtime_error("e1 (same faction at center) should NOT take damage (faction filter)");
    }
    if (h2->current != 100.0f) {
        throw std::runtime_error("e2 (same faction in AoE) should NOT take damage (faction filter)");
    }
    if (h3->current != 75.0f) {
        throw std::runtime_error("e3 (opposing faction in AoE) should take damage");
    }
}

TEST(combat_manager_updates_projectiles) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    sim.combat_manager().update(50.0f, sim.spatial_grid(), sim.component_manager());
    
    size_t count = sim.combat_manager().projectile_manager().active_count();
    
    if (count < 0) {
        throw std::runtime_error("Active projectile count should be >= 0");
    }
}

TEST(intercept_calculator_zero_quadratic_coefficient) {
    using namespace rts;
    
    InterceptCalculator calc;
    
    InterceptSolution solution = calc.calculate_intercept(
        0.0f, 0.0f, 25.0f,
        100.0f, 0.0f, 0.0f, 10.0f
    );
    
    if (!solution.valid) {
        throw std::runtime_error("Should be valid when target is stationary and projectile is faster");
    }
    
    if (solution.aim_x < 100.0f || solution.aim_x > 200.0f) {
        std::string msg = "Aim x should be between 100 and 200, got " + std::to_string(solution.aim_x);
        throw std::runtime_error(msg);
    }
}

TEST(intercept_calculator_zero_projectile_speed) {
    using namespace rts;
    
    InterceptCalculator calc;
    
    InterceptSolution solution = calc.calculate_intercept(
        0.0f, 0.0f, 0.0f,
        100.0f, 0.0f, 10.0f, 25.0f
    );
    
    if (solution.valid) {
        throw std::runtime_error("Should be invalid when projectile speed is 0");
    }
}

TEST(intercept_calculator_zero_initial_distance) {
    using namespace rts;
    
    InterceptCalculator calc;
    
    InterceptSolution solution = calc.calculate_intercept(
        0.0f, 0.0f, 10.0f,
        0.0f, 0.0f, 10.0f, 25.0f
    );
    
    if (!solution.valid) {
        throw std::runtime_error("Should be valid when shooter and target coincide");
    }
}

TEST(projectile_manager_friendly_fire_disabled) {
    using namespace rts;
    
    ProjectileManager pm(100);
    SpatialGrid grid(200.0f);
    ComponentManager cm;
    
    EntityManager em;
    
    auto shooter = em.create_entity();
    cm.add_component<Position>(shooter.id, Position{100.0f, 100.0f, 0.0f});
    cm.add_component<Health>(shooter.id, Health{100.0f, 100.0f, false});
    cm.add_component<Faction>(shooter.id, Faction{FactionId::ELITE_PRECISION});
    grid.insert(shooter.id, 100.0f, 100.0f);
    
    auto ally = em.create_entity();
    cm.add_component<Position>(ally.id, Position{110.0f, 110.0f, 0.0f});
    cm.add_component<Health>(ally.id, Health{100.0f, 100.0f, false});
    cm.add_component<Faction>(ally.id, Faction{FactionId::ELITE_PRECISION});
    grid.insert(ally.id, 110.0f, 110.0f);
    
    pm.spawn(100.0f, 100.0f, 0.7071f, 0.7071f, 50.0f, 25.0f, 20.0f, 2.0f, FactionId::ELITE_PRECISION);
    pm.update(1000.0f, grid, cm);
    
    auto* shooter_health = cm.get_component<Health>(shooter.id);
    auto* ally_health = cm.get_component<Health>(ally.id);
    
    if (!shooter_health || !ally_health) {
        throw std::runtime_error("Health components should exist");
    }
    
    if (shooter_health->current != 100.0f) {
        throw std::runtime_error("Shooter should not damage self");
    }
    if (ally_health->current != 100.0f) {
        throw std::runtime_error("Ally should not take friendly fire damage");
    }
}

TEST(command_manager_process_attack) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    sim.initialize_faction(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    
    Entity shooter = sim.create_unit(100.0f, 100.0f);
    sim.set_unit_faction(shooter.id, FactionId::ELITE_PRECISION);
    
    Entity target = sim.create_unit(150.0f, 100.0f);
    sim.set_unit_faction(target.id, FactionId::MASS_WARFARE);
    
    auto* shooter_weap = sim.component_manager().get_component<Weapon>(shooter.id);
    if (!shooter_weap) {
        throw std::runtime_error("Shooter should have weapon component");
    }
    
    auto* target_pos = sim.component_manager().get_component<Position>(target.id);
    auto* target_faction = sim.component_manager().get_component<Faction>(target.id);
    auto* target_health = sim.component_manager().get_component<Health>(target.id);
    
    if (!target_pos || !target_faction || !target_health) {
        throw std::runtime_error("Target should have position, faction, and health components");
    }
    
    sim.command_manager().process_command(
        static_cast<uint32_t>(shooter.id),
        static_cast<uint8_t>(CommandType::ATTACK),
        0, 0, 0, 0,
        static_cast<uint32_t>(target.id)
    );
    
    sim.process_commands();
    
    auto pending = sim.combat_manager().pending_fire();
    if (pending.find(shooter.id) == pending.end() || !pending.find(shooter.id)->second.valid) {
        throw std::runtime_error("Attack command should create pending fire solution");
    }
    if (pending.find(shooter.id)->second.target_id != target.id) {
        throw std::runtime_error("Pending fire solution should target correct entity");
    }
}

TEST(command_manager_reject_invalid_attack) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    sim.initialize_faction(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    
    Entity shooter = sim.create_unit(100.0f, 100.0f);
    sim.set_unit_faction(shooter.id, FactionId::ELITE_PRECISION);
    
    Entity alive_target = sim.create_unit(150.0f, 100.0f);
    sim.set_unit_faction(alive_target.id, FactionId::ELITE_PRECISION);
    
    sim.command_manager().process_command(
        static_cast<uint32_t>(shooter.id),
        static_cast<uint8_t>(CommandType::ATTACK),
        0, 0, 0, 0,
        static_cast<uint32_t>(alive_target.id)
    );
    
    sim.process_commands();
    
    auto pending = sim.combat_manager().pending_fire();
    if (pending.find(shooter.id) != pending.end()) {
        throw std::runtime_error("Invalid attack should be rejected (same faction)");
    }
    
    sim.combat_manager().pending_fire().clear();
    
    EntityId nonexistent_target = EntityId{9999};
    sim.command_manager().process_command(
        static_cast<uint32_t>(shooter.id),
        static_cast<uint8_t>(CommandType::ATTACK),
        0, 0, 0, 0,
        static_cast<uint32_t>(nonexistent_target)
    );
    
    sim.process_commands();
    
    auto pending2 = sim.combat_manager().pending_fire();
    if (pending2.find(shooter.id) != pending2.end()) {
        throw std::runtime_error("Invalid attack should be rejected (nonexistent target)");
    }
}

TEST(command_manager_unregister_clears_attack_target) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    sim.initialize_faction(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    
    Entity shooter = sim.create_unit(100.0f, 100.0f);
    sim.set_unit_faction(shooter.id, FactionId::ELITE_PRECISION);
    
    Entity target = sim.create_unit(150.0f, 100.0f);
    sim.set_unit_faction(target.id, FactionId::MASS_WARFARE);
    
    sim.command_manager().process_command(
        static_cast<uint32_t>(shooter.id),
        static_cast<uint8_t>(CommandType::ATTACK),
        0, 0, 0, 0,
        static_cast<uint32_t>(target.id)
    );
    
    sim.process_commands();
    
    auto before = sim.combat_manager().explicit_attack_targets().find(shooter.id);
    if (before == sim.combat_manager().explicit_attack_targets().end()) {
        throw std::runtime_error("Attack target should be registered");
    }
    
    sim.destroy_unit(shooter.id);
    
    auto after = sim.combat_manager().explicit_attack_targets().find(shooter.id);
    if (after != sim.combat_manager().explicit_attack_targets().end()) {
        throw std::runtime_error("Attack target should be cleared on unit death");
    }
}

TEST(combatsystem_visibility_faction_id) {
    using namespace rts;
    
    Simulation sim;
    sim.start();
    
    sim.initialize_faction(FactionId::ELITE_PRECISION, 0.0f, 0.0f);
    
    Entity unit1 = sim.create_unit(100.0f, 100.0f);
    sim.set_unit_faction(unit1.id, FactionId::ELITE_PRECISION);
    
    FactionId faction_out;
    if (!sim.get_unit_faction_id(unit1.id, faction_out)) {
        throw std::runtime_error("get_unit_faction_id should return true");
    }
    if (faction_out != FactionId::ELITE_PRECISION) {
        throw std::runtime_error(" Faction should be ELITE_PRECISION");
    }
    
    Entity unit2 = sim.create_unit(200.0f, 200.0f);
    sim.set_unit_faction(unit2.id, FactionId::MASS_WARFARE);
    
    if (!sim.get_unit_faction_id(unit2.id, faction_out)) {
        throw std::runtime_error("get_unit_faction_id should return true for second unit");
    }
    if (faction_out != FactionId::MASS_WARFARE) {
        throw std::runtime_error(" Faction should be MASS_WARFARE");
    }
    
    if (sim.get_unit_faction_id(EntityId{9999}, faction_out)) {
        throw std::runtime_error("get_unit_faction_id should return false for nonexistent entity");
    }
}
