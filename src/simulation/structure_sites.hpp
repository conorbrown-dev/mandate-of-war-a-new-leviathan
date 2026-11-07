#pragma once

#include <algorithm>
#include <cstdint>

namespace rts {

// Queue job that levels ground before the structure it serves is built.
inline constexpr uint8_t EARTHWORKS_JOB = 4;

struct StructureSiteRules {
    float footprint_x = 0.0f;
    float footprint_y = 0.0f;
    // Ground inside these limits takes the structure as it stands.
    float direct_slope = 0.0f;
    float direct_height_variation = 0.0f;
    // Ground inside these limits can be levelled first. Beyond them it is a
    // cliff or mountainside and stays unbuildable.
    float earthworks_slope = 0.0f;
    float earthworks_height_variation = 0.0f;
    float earthworks_material_per_meter = 0.0f;
    float earthworks_fuel_per_meter = 0.0f;
};

constexpr StructureSiteRules structure_site_rules(uint8_t structure_type) {
    switch (structure_type) {
        case 0: // outpost
        case 1: // radar
        case 3: // floodlight
            return {60.0f, 60.0f, 0.45f, 80.0f, 0.70f, 200.0f, 6.0f, 4.0f};
        case 2: // airfield
            return {250.0f, 750.0f, 0.40f, 180.0f, 0.70f, 300.0f, 12.0f, 8.0f};
        default:
            return {};
    }
}

inline constexpr float kEarthworksBaseMaterial = 40.0f;
inline constexpr float kEarthworksBaseFuel = 30.0f;
inline constexpr float kEarthworksBaseSeconds = 6.0f;
inline constexpr float kEarthworksSecondsPerMeter = 2.0f;
inline constexpr float kEarthworksMaximumSeconds = 90.0f;

inline float earthworks_seconds(float mean_depth_meters) {
    return std::min(kEarthworksMaximumSeconds,
        kEarthworksBaseSeconds + mean_depth_meters * kEarthworksSecondsPerMeter);
}

struct StructureSite {
    bool buildable = false;
    bool earthworks = false;
    float pad_height = 0.0f;
    float material = 0.0f;
    float fuel = 0.0f;
    float seconds = 0.0f;
};

} // namespace rts
