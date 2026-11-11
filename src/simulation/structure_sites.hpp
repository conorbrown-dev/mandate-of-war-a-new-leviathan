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

// Sight from a finished structure, in metres. The Radar Mast is the long watch.
constexpr float structure_view_range(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return 400.0f;  // outpost
        case 1: return 3000.0f; // radar
        case 2: return 500.0f;  // airfield
        case 3: return 300.0f;  // floodlight
        default: return 0.0f;
    }
}

// What a finished structure draws from its side's power grid, in megawatts.
constexpr float structure_power_demand(uint8_t structure_type) {
    switch (structure_type) {
        case 0: return 5.0f;  // outpost
        case 1: return 15.0f; // radar
        case 2: return 25.0f; // airfield
        case 3: return 2.0f;  // floodlight
        default: return 0.0f;
    }
}

inline constexpr float kCommandPostViewRange = 400.0f;

// How far above the ground each kind of observer looks from, so a tall mast sees
// over rises that hide a tank. Aircraft see over nearly everything.
inline constexpr float kGroundEyeHeight = 4.0f;
inline constexpr float kAircraftEyeHeight = 1500.0f;
inline constexpr float kCommandPostEyeHeight = 15.0f;

inline constexpr float structure_eye_height(uint8_t structure_type) {
    switch (structure_type) {
        case 1: return 40.0f; // radar mast
        case 2: return 15.0f; // airfield tower
        case 3: return 15.0f; // floodlight
        default: return 20.0f; // forward outpost
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
