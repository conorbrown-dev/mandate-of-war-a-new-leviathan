#pragma once

#include "ecs/entity.hpp"

namespace rts {

struct NavalVessel {
    float x, y;
    float fuel;
    float max_fuel;
    float fuel_consumption_rate;
    bool is_stranded;
};

} // namespace rts
