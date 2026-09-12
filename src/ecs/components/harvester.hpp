#pragma once

#include <cstdint>

#include "ecs/entity.hpp"

namespace rts {

struct Harvester {
    EntityId node_id;
    float last_extraction_tick;
    float current_amount;
};

} // namespace rts
