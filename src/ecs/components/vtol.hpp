#pragma once

#include "ecs/entity.hpp"

namespace rts {

struct VTOL {
    float x, y;
    float fuel;
    float max_fuel;
    float fuel_consumption_rate;
    float range;
    enum class Status {
        ON_GROUND,
        AIRBORNE,
        CRASHED
    } status;
    enum class Mission {
        PATROL,
        RECON,
        INTERCEPT
    } mission;
    EntityId target_base;
};

} // namespace rts
