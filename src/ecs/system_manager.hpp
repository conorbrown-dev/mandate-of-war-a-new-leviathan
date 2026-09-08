#pragma once

#include <vector>

#include "ecs/entity.hpp"
#include "ecs/component_manager.hpp"

namespace rts {

class System {
public:
    virtual ~System() = default;
    virtual void update(float delta_ms, ComponentManager& components) = 0;
};

class SystemManager {
public:
    SystemManager() = default;
    
    template<typename T>
    void add_system() {
        systems_.push_back(std::make_unique<T>());
    }
    
    void update(float delta_ms, ComponentManager& components) {
        for (auto& system : systems_) {
            system->update(delta_ms, components);
        }
    }
    
private:
    std::vector<std::unique_ptr<System>> systems_;
};

} // namespace rts
