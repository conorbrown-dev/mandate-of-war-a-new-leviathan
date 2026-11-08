#pragma once

#include <string>

#include "ecs/components/factions.hpp"
#include "ecs/entity.hpp"

namespace rts {

// Fuel is never produced in a match. It is shipped in through an airfield or
// taken from a depot, whose stock is finite and does not refill.
inline constexpr float kFuelDepotCaptureRadius = 40.0f;
inline constexpr float kFuelDepotDrawPerTick = 0.5f;

inline constexpr float kFuelShipmentLoad = 2000.0f;
inline constexpr float kFuelShipmentMaterialCost = 300.0f;
inline constexpr float kFuelShipmentDurationMs = 45000.0f;

struct FuelCapture {
    EntityId unit = INVALID_ENTITY;
    EntityId depot = INVALID_ENTITY;
};

struct FuelShipment {
    FactionId faction = FactionId::ELITE_PRECISION;
    float x = 0.0f;
    float y = 0.0f;
    float remaining_ms = 0.0f;
};

struct FuelDepotStatus {
    EntityId id = INVALID_ENTITY;
    float x = 0.0f;
    float y = 0.0f;
    float stock = 0.0f;
    float capacity = 0.0f;
    int owner = -1;
    bool capturing = false;
};

} // namespace rts
