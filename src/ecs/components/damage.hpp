#pragma once

#include <cstdint>

namespace rts {

struct Damage {
    float amount;
    float radius;
    enum class Type : uint8_t {
        KINETIC,
        ENERGY,
        EXPLOSIVE
    } type;
    bool direct_hit;
};

} // namespace rts
