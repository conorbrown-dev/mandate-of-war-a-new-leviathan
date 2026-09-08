#pragma once

#include <cstdint>

namespace rts {

struct Weapon {
    float damage;
    float range;
    float fire_rate;
    float last_fired;
    float projectile_speed;
    float lead_bias;
    float aoe_radius;
    float cooldown_remaining;
};

} // namespace rts
