#pragma once

#include "ecs/entity.hpp"

namespace rts {

struct Aircraft {
    float x, y;
    float fuel;
    float max_fuel;
    float fuel_consumption_rate;
    float material;
    float max_material;
    float material_consumption_rate;
    float ammunition;
    float max_ammunition;
    float cruise_speed;
    float airborne_time_ms;
    float max_airborne_time_ms;
    float range;
    enum class Status {
        ON_GROUND,
        QUEUED_FOR_TAKEOFF,
        TAKING_OFF,
        AIRBORNE,
        RETURNING,
        QUEUED_FOR_LANDING,
        LANDING,
        RECOVERING,
        CRASHED
    } status;
    enum class Mission {
        PATROL,
        INTERCEPT,
        RECON,
        REFUEL
    } mission;
    enum class Type {
        CONVENTIONAL,
        VTOL
    } type;
    EntityId target_base;
    EntityId closest_recovery_facility;
    float predicted_return_energy;
    float predicted_return_material;
    float predicted_return_time_ms;
    bool safe_return;
    int queue_slot;
};

} // namespace rts
