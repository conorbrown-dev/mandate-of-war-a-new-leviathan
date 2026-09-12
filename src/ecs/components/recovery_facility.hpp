#pragma once

#include <cstdint>

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

    enum Capability : std::uint8_t {
        AIR_RECOVERY = 1 << 0,
        NAVAL_RESUPPLY = 1 << 1,
    };

    std::uint8_t capabilities = 0;

    bool supports(Type requested) const {
        const std::uint8_t required = requested == Type::NAVAL_BASE
            ? NAVAL_RESUPPLY : AIR_RECOVERY;
        return (capabilities & required) != 0 || type == requested;
    }
};

} // namespace rts
