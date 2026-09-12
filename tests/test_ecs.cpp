#include "test_framework.hpp"
#include "ecs/component_manager.hpp"
#include "ecs/entity.hpp"

TEST(component_type_uniqueness) {
    using namespace rts;
    
    auto type1 = component_type<int>();
    auto type2 = component_type<float>();
    
    if (type1 == type2) {
        throw std::runtime_error("Different types should have different IDs");
    }
    
    auto mask1 = component_mask<int>();
    auto mask2 = component_mask<float>();
    
    if (mask1 == mask2) {
        throw std::runtime_error("Component masks should be different");
    }
    if (mask1 != (ComponentMask{1} << type1)) {
        throw std::runtime_error("Component mask should match shift calculation");
    }
    
    auto type3 = component_type<int>();
    if (type1 != type3) {
        throw std::runtime_error("Same type should have same ID within same TU");
    }
}

TEST(entity_creation_destroy) {
    using namespace rts;
    
    EntityManager em;
    
    auto e1 = em.create_entity();
    if (!e1.valid()) {
        throw std::runtime_error("Created entity should be valid");
    }
    if (e1.id == NULL_ENTITY) {
        throw std::runtime_error("Entity ID should not be NULL_ENTITY");
    }
    
    auto e2 = em.create_entity();
    if (e1.id == e2.id) {
        throw std::runtime_error("Each entity should have unique ID");
    }
    
    em.destroy_entity(e1);
    
    if (em.entity_count() != 1) {
        throw std::runtime_error("Entity count should be 1 after destroy");
    }
}

TEST(component_manager_operations) {
    using namespace rts;
    
    ComponentManager cm;
    EntityManager em;
    
    auto e = em.create_entity();
    
    cm.add_component<int>(e.id, 42);
    cm.add_component<float>(e.id, 3.14f);
    
    auto* int_val = cm.get_component<int>(e.id);
    auto* float_val = cm.get_component<float>(e.id);
    
    if (!int_val || *int_val != 42) {
        throw std::runtime_error("Component manager should store and retrieve int");
    }
    if (!float_val || *float_val != 3.14f) {
        throw std::runtime_error("Component manager should store and retrieve float");
    }
    
    auto mask = cm.get_mask(e.id);
    auto int_mask = component_mask<int>();
    auto float_mask = component_mask<float>();
    
    if ((mask & int_mask) == 0) {
        throw std::runtime_error("Component mask should include int");
    }
    if ((mask & float_mask) == 0) {
        throw std::runtime_error("Component mask should include float");
    }
    
    cm.remove_component<int>(e.id);
    
    if (cm.get_component<int>(e.id) != nullptr) {
        throw std::runtime_error("Component should be removed");
    }
}

TEST(entities_with_filter) {
    using namespace rts;
    
    ComponentManager cm;
    EntityManager em;
    
    auto e1 = em.create_entity();
    auto e2 = em.create_entity();
    auto e3 = em.create_entity();
    
    cm.add_component<int>(e1.id, 1);
    cm.add_component<int>(e2.id, 2);
    cm.add_component<float>(e2.id, 2.0f);
    cm.add_component<int>(e3.id, 3);
    cm.add_component<float>(e3.id, 3.0f);
    
    auto all = em.get_entities();
    
    auto with_int = cm.entities_with<int>(all);
    if (with_int.size() != 3) {
        throw std::runtime_error("Should find 3 entities with int component");
    }
    
    auto with_float = cm.entities_with<float>(all);
    if (with_float.size() != 2) {
        std::string msg = "Should find 2 entities with float component, found " + std::to_string(with_float.size());
        throw std::runtime_error(msg);
    }
    
    auto with_both = cm.entities_with<int, float>(all);
    if (with_both.size() != 2) {
        std::string msg = "Should find 2 entities with both components, found " + std::to_string(with_both.size());
        throw std::runtime_error(msg);
    }
    if (with_both.size() != 2) {
        std::string msg = "Should find 2 entities with both components, found " + std::to_string(with_both.size());
        throw std::runtime_error(msg);
    }
    if ((with_both[0] != e2.id && with_both[0] != e3.id) || 
        (with_both.size() > 1 && with_both[1] != e2.id && with_both[1] != e3.id)) {
        throw std::runtime_error("Should find e2 and e3 with both components");
    }
}

TEST(component_manager_instance_isolation) {
    using namespace rts;

    ComponentManager first;
    ComponentManager second;
    first.add_component<int>(1, 42);

    if (second.get_component<int>(1) != nullptr) {
        throw std::runtime_error("Component storage must not leak between manager instances");
    }
}

TEST(component_manager_const_access_and_cleanup) {
    using namespace rts;

    ComponentManager components;
    components.add_component<int>(1, 42);
    components.add_component<float>(1, 3.5f);

    const ComponentManager& const_components = components;
    const auto* value = const_components.get_component<int>(1);
    if (!value || *value != 42) {
        throw std::runtime_error("Const component access must read the owned storage");
    }

    components.remove_entity(1);
    if (components.get_component<int>(1) || components.get_component<float>(1) || components.get_mask(1) != 0) {
        throw std::runtime_error("Removing an entity must remove all components and its mask");
    }
}

TEST(entity_id_reuse) {
    using namespace rts;

    EntityManager entities;
    auto first = entities.create_entity();
    if (!entities.destroy_entity(first)) {
        throw std::runtime_error("Destroying a live entity should succeed");
    }
    if (entities.destroy_entity(first)) {
        throw std::runtime_error("Destroying the same entity twice should fail");
    }

    auto reused = entities.create_entity();
    if (reused.id != first.id || !entities.is_alive(reused.id)) {
        throw std::runtime_error("Destroyed entity IDs should be reused as live IDs");
    }
}

int main() {
    using namespace rts::test;
    
    try {
        test_runner().run_all();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
