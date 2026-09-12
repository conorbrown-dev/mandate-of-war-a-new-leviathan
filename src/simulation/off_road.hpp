#pragma once

#include <unordered_map>

#include "ecs/components/factions.hpp"

namespace rts {

struct OffRoadSettings {
    float wear_per_meter = 0.02f;
    float road_wear_multiplier = 0.08f;
    float energy_per_meter = 0.015f;
    float material_per_meter = 0.004f;
    float speed_penalty_per_wear = 0.006f;
    float maximum_speed_penalty = 0.45f;
    std::unordered_map<UnitType, float> unit_factors;
};

const OffRoadSettings& get_off_road_settings();
float off_road_unit_factor(UnitType unit_type);

} // namespace rts
