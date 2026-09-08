#pragma once

#include "ecs/entity.hpp"
#include "airbase.hpp"

namespace rts {

struct Carrier : public Airbase {
    enum class Tier {
        T1_LIGHT,
        T2_FLEET,
        T3_SUPER,
        T4_EXPERIMENTAL
    };

    Tier tier = Tier::T1_LIGHT;
    float speed = 0.0f;
    float max_speed = 0.0f;
    int deck_capacity = 0;
    int deck_occupancy = 0;
    int launch_queue_size = 0;
};

} // namespace rts
