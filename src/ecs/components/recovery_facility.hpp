#pragma once

#include "ecs/entity.hpp"

namespace rts {

struct RecoveryFacility {
    float x, y;
    float max_recovery_distance;
    enum class Type {
        AIRBASE,
        CARRIER,
        NAVAL_BASE
    } type;
};

} // namespace rts
