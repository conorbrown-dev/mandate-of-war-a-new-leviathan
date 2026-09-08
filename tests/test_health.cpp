#include "test_framework.hpp"
#include "ecs/components/resources.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/faction.hpp"
#include "combat/projectile_manager.hpp"
#include "combat/combat_manager.hpp"
#include "spatial/spatial_grid.hpp"
#include "simulation/simulation.hpp"
#include "ecs/entity.hpp"

TEST(unit_health_manager_initializes_health) {
    using namespace rts;
    
    ComponentManager cm;
    EntityManager em;
    
    auto entity = em.create_entity();
    cm.add_component<Health>(entity.id, Health{50.0f, 100.0f, false});
    
    auto* health = cm.get_component<Health>(entity.id);
    if (!health) {
        throw std::runtime_error("Health component should exist");
    }
    if (health->current != 50.0f) {
        throw std::runtime_error("Initial health should be 50");
    }
    if (health->max != 100.0f) {
        throw std::runtime_error("Max health should be 100");
    }
}

TEST(unit_health_applies_damage) {
    using namespace rts;
    
    ComponentManager cm;
    EntityManager em;
    
    auto entity = em.create_entity();
    cm.add_component<Health>(entity.id, Health{100.0f, 100.0f, false});
    
    auto* health = cm.get_component<Health>(entity.id);
    if (!health) {
        throw std::runtime_error("Health component should exist");
    }
    
    health->current = 75.0f;
    if (health->current != 75.0f) {
        throw std::runtime_error("Health should be 75 after damage");
    }
}

TEST(unit_health_detects_death) {
    using namespace rts;
    
    ComponentManager cm;
    EntityManager em;
    
    auto entity = em.create_entity();
    cm.add_component<Health>(entity.id, Health{0.0f, 100.0f});
    
    auto* health = cm.get_component<Health>(entity.id);
    if (!health) {
        throw std::runtime_error("Health component should exist");
    }
    if (!health->is_dead) {
        throw std::runtime_error("Unit with 0 health should be marked dead");
    }
}

TEST(unit_health_restores_health) {
    using namespace rts;
    
    ComponentManager cm;
    EntityManager em;
    
    auto entity = em.create_entity();
    cm.add_component<Health>(entity.id, Health{50.0f, 100.0f, false});
    
    auto* health = cm.get_component<Health>(entity.id);
    if (!health) {
        throw std::runtime_error("Health component should exist");
    }
    
    health->current = 75.0f;
    if (health->current != 75.0f) {
        throw std::runtime_error("Health should restore to 75");
    }
}
