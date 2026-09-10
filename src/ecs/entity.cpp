#include <algorithm>
#include <vector>

#include "ecs/entity.hpp"

namespace rts {

Entity EntityManager::create_entity() {
    EntityId id;
    if (!free_list_.empty()) {
        id = free_list_.back();
        free_list_.pop_back();
    } else {
        id = next_id_++;
    }
    entities_.push_back(id);
    return Entity{id};
}

bool EntityManager::destroy_entity(Entity entity) {
    if (!entity.valid()) return false;
    
    auto it = std::find(entities_.begin(), entities_.end(), entity.id);
    if (it != entities_.end()) {
        std::swap(*it, entities_.back());
        entities_.pop_back();
        if (recycle_ids_) free_list_.push_back(entity.id);
        return true;
    }
    return false;
}

bool EntityManager::is_alive(EntityId entity) const {
    return std::find(entities_.begin(), entities_.end(), entity) != entities_.end();
}

void EntityManager::clear() {
    entities_.clear();
    free_list_.clear();
    next_id_ = 1;
}

} // namespace rts
