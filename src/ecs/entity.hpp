#pragma once

#include <cstdint>
#include <vector>

namespace rts {

using EntityId = uint32_t;
constexpr EntityId NULL_ENTITY = 0xFFFFFFFF;
constexpr EntityId INVALID_ENTITY = 0xFFFFFFFF;

class Entity {
public:
    EntityId id;
    
    explicit Entity(EntityId entity_id) : id(entity_id) {}
    Entity() : id(NULL_ENTITY) {}
    
    bool valid() const { return id != NULL_ENTITY; }
    explicit operator bool() const { return valid(); }
    
    bool operator==(const Entity& other) const { return id == other.id; }
    bool operator!=(const Entity& other) const { return id != other.id; }
};

class EntityManager {
public:
    EntityManager() = default;
    
    Entity create_entity();
    bool destroy_entity(Entity entity);
    void clear();
    bool is_alive(EntityId entity) const;
    
    size_t entity_count() const { return entities_.size(); }
    const std::vector<EntityId>& get_entities() const { return entities_; }
    
private:
    std::vector<EntityId> entities_;
    std::vector<EntityId> free_list_;
    EntityId next_id_{1};
};

} // namespace rts
