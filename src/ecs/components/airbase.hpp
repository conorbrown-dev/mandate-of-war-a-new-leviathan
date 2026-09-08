#pragma once

#include "ecs/entity.hpp"

namespace rts {

struct Airbase {
    float x, y;
    int runway_capacity;
    int refuel_rate;
    int rearm_rate;
    float max_fuel;
    float current_fuel;
    float max_munitions;
    float current_munitions;
    bool runway_usable;
};

} // namespace rts
