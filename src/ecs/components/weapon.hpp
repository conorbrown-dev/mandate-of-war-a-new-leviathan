#pragma once

#include <cstdint>

namespace rts {

// A weapon reaches this altitude unless its unit data says otherwise. Ordinary
// aircraft fly below it; only strategic recon flies above it.
constexpr float DEFAULT_WEAPON_CEILING_METERS = 15000.0f;

struct Weapon {
    float damage;
    float range;
    float fire_rate;
    float last_fired;
    float projectile_speed;
    float lead_bias;
    float aoe_radius;
    float cooldown_remaining;
    // What it can engage: targets on the ground (or water), aircraft in flight.
    bool hits_ground = true;
    bool hits_air = false;
    // Highest target altitude this weapon can engage, in metres.
    float max_target_altitude = DEFAULT_WEAPON_CEILING_METERS;
};

} // namespace rts
