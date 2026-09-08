#pragma once

#include "ecs/entity.hpp"

namespace rts {

struct Intelligence {
    EntityId entity_id;
    float last_x, last_y;
    uint32_t last_seen_tick;
    float freshness;
    enum class Confidence {
        HIGH,
        MEDIUM,
        LOW
    } confidence;
    bool currently_observed;
};

} // namespace rts
