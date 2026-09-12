#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/components/aircraft.hpp"
#include "ecs/components/intelligence.hpp"

namespace rts {

using ComponentType = uint16_t;
using ComponentMask = uint64_t;

constexpr size_t MAX_COMPONENTS = 64;

inline ComponentType& get_component_type_counter() {
    static ComponentType next = 0;
    return next;
}

template<typename T>
ComponentType component_type() {
    static ComponentType id = [] {
        auto& next = get_component_type_counter();
        if (next >= MAX_COMPONENTS) {
            throw std::runtime_error("Maximum ECS component type count exceeded");
        }
        return next++;
    }();
    return id;
}

template<typename T>
ComponentMask component_mask() {
    return ComponentMask{1} << component_type<T>();
}

class ComponentManager {
public:
    ComponentManager() = default;
     
     template<typename T>
    void add_component(EntityId entity, const T& value) {
        auto& storage = ensure_storage<T>();
        storage[entity] = value;
        masks_[entity] |= component_mask<T>();
    }
    
    template<typename T>
    void remove_component(EntityId entity) {
        if (auto* storage = find_storage<T>()) {
            storage->erase(entity);
        }

        auto mask = masks_.find(entity);
        if (mask != masks_.end()) {
            mask->second &= ~component_mask<T>();
            if (mask->second == 0) {
                masks_.erase(mask);
            }
        }
    }

    void remove_entity(EntityId entity) {
        for (auto& [type, storage] : storages_) {
            storage->erase(entity);
        }
        masks_.erase(entity);
    }
    
    void cleanup_dead_entities(const std::vector<EntityId>& active_entities) {
        const std::unordered_set<EntityId> active(active_entities.begin(), active_entities.end());
        std::vector<EntityId> to_remove;
        for (const auto& [entity_id, _] : masks_) {
            if (!active.contains(entity_id)) {
                to_remove.push_back(entity_id);
            }
        }
        for (auto entity : to_remove) {
            remove_entity(entity);
        }
    }
    
    void cleanup_all() {
        masks_.clear();
        storages_.clear();
    }
    
    template<typename T>
    T* get_component(EntityId entity) {
        auto* storage = find_storage<T>();
        if (!storage) {
            return nullptr;
        }
        auto it = storage->find(entity);
        if (it != storage->end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    template<typename T>
    const T* get_component(EntityId entity) const {
        const auto* storage = find_storage<T>();
        if (!storage) {
            return nullptr;
        }
        auto it = storage->find(entity);
        if (it != storage->end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    ComponentMask get_mask(EntityId entity) const {
        auto it = masks_.find(entity);
        if (it != masks_.end()) {
            return it->second;
        }
        return 0;
    }

    template<typename... Components>
    std::vector<EntityId> entities_with(const std::vector<EntityId>& active_entities) const {
        std::vector<EntityId> result;
        result.reserve(active_entities.size());
        const ComponentMask required = (component_mask<Components>() | ...);
        
        for (auto entity : active_entities) {
            auto it = masks_.find(entity);
            if (it != masks_.end()) {
                if ((it->second & required) == required) {
                    result.push_back(entity);
                }
            }
        }
        return result;
    }
    
    template<typename... Components>
    std::vector<EntityId> entities_with() const {
        std::vector<EntityId> result;
        const ComponentMask required = (component_mask<Components>() | ...);
        for (const auto& [entity, mask] : masks_) {
            if ((mask & required) == required) {
                result.push_back(entity);
            }
        }
        return result;
    }

private:
    template<typename T>
    using Storage = std::unordered_map<EntityId, T>;

    struct StorageBase {
        virtual ~StorageBase() = default;
        virtual void erase(EntityId entity) = 0;
    };

    template<typename T>
    struct TypedStorage final : StorageBase {
        Storage<T> values;

        void erase(EntityId entity) override {
            values.erase(entity);
        }
    };

    template<typename T>
    Storage<T>& ensure_storage() {
        const ComponentType type = component_type<T>();
        auto [it, inserted] = storages_.try_emplace(type);
        if (inserted) {
            it->second = std::make_unique<TypedStorage<T>>();
        }
        return static_cast<TypedStorage<T>*>(it->second.get())->values;
    }

    template<typename T>
    Storage<T>* find_storage() {
        auto it = storages_.find(component_type<T>());
        if (it == storages_.end()) {
            return nullptr;
        }
        return &static_cast<TypedStorage<T>*>(it->second.get())->values;
    }

    template<typename T>
    const Storage<T>* find_storage() const {
        auto it = storages_.find(component_type<T>());
        if (it == storages_.end()) {
            return nullptr;
        }
        return &static_cast<const TypedStorage<T>*>(it->second.get())->values;
    }

    std::unordered_map<ComponentType, std::unique_ptr<StorageBase>> storages_;
    std::unordered_map<EntityId, ComponentMask> masks_;
};

} // namespace rts
