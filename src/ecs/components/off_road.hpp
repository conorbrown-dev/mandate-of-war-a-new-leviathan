#pragma once

#include "factions.hpp"

namespace rts {

struct OffRoadWear {
    float accumulated = 0.0f;
    float distance_traveled = 0.0f;
    UnitType unit_type = UnitType::ELITE_MAIN_BATTLE_TANK;
};

} // namespace rts
