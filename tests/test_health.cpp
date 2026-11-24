#include "test_framework.hpp"
#include "ecs/components/resources.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/faction.hpp"
#include "combat/projectile_manager.hpp"
#include "combat/combat_manager.hpp"
#include "spatial/spatial_grid.hpp"
#include "simulation/simulation.hpp"
#include "ecs/entity.hpp"
#include "production/production_manager.hpp"

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

TEST(tactical_resource_model_has_materials_fuel_and_power_without_research) {
    using namespace rts;
    Storage storage{};
    storage.metal_storage = 400.0f;
    storage.metal_capacity = 900.0f;
    storage.energy_capacity = 600.0f;
    ProductionManager production;
    production.add_storage(42, storage);
    auto* resources = production.tactical_resources(42);
    if (!resources) throw std::runtime_error("Tactical resources were not created");
    if (resources->materials.current != 400.0f || resources->materials.max != 900.0f)
        throw std::runtime_error("Materials were not initialized from the compatibility bridge");
    if (resources->fuel.current != 0.0f || resources->fuel.capacity != 600.0f)
        throw std::runtime_error("Fuel state is not authoritative storage state");
    if (resources->power.generation_mw != 0.0f || resources->power.demand_mw != 0.0f)
        throw std::runtime_error("Power must start as generation/demand state, not a wallet");
}

TEST(tactical_power_reserve_reports_surplus_and_deficit) {
    using namespace rts;
    ProductionManager production;
    production.add_storage(7, Storage{});
    if (!production.set_tactical_power(7, 100.0f, 70.0f))
        throw std::runtime_error("Could not set valid tactical power state");
    auto* power = &production.tactical_resources(7)->power;
    if (power->reserve_mw() != 30.0f || power->in_deficit())
        throw std::runtime_error("Power surplus is incorrect");
    if (!production.set_tactical_power(7, 50.0f, 80.0f))
        throw std::runtime_error("Could not set deficit tactical power state");
    if (power->reserve_mw() != -30.0f || !power->in_deficit())
        throw std::runtime_error("Power deficit is incorrect");
}

TEST(tactical_extraction_routes_legacy_energy_to_fuel) {
    using namespace rts;
    ProductionManager production;
    Storage storage{};
    storage.metal_capacity = 100.0f;
    storage.energy_capacity = 100.0f;
    production.add_storage(1, storage);
    production.add_resource_node(2, ResourceNode{0, 0, 50, 50, ResourceNode::Type::ENERGY, false});
    production.extractors()[3] = Extractor{0, 0, 2, 10.0f, 0.0f, true, 1};
    production.extract_resource(3, 50.0f);
    if (production.tactical_resources(1)->fuel.current != 10.0f)
        throw std::runtime_error("Tactical energy-node migration did not fill Fuel");
}
