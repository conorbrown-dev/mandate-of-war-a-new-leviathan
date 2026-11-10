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
// Shipments fly in on the side's cargo aircraft from the west map edge and can
// be shot down on the way; this is only the HUD's fallback estimate.
inline constexpr float kFuelShipmentDurationMs = 45000.0f;
// The cargo aircraft delivers once it is this close to its airfield.
inline constexpr float kFuelShipmentLandingRadius = 200.0f;

struct FuelCapture {
    EntityId unit = INVALID_ENTITY;
    EntityId depot = INVALID_ENTITY;
};

// A Materials site changes hands the same way: when a unit ordered to it
// arrives, not when the order is given.
inline constexpr float kSiteCaptureRadius = 40.0f;

struct SiteCapture {
    EntityId unit = INVALID_ENTITY;
    EntityId node = INVALID_ENTITY;
    EntityId extractor = INVALID_ENTITY;
};

struct MaterialSiteStatus {
    EntityId id = INVALID_ENTITY;
    float x = 0.0f;
    float y = 0.0f;
    int owner = -1;
    bool capturing = false;
    // Which side is on its way to take it, so a view shows only its own.
    int capturing_faction = -1;
};

struct FuelShipment {
    FactionId faction = FactionId::ELITE_PRECISION;
    float x = 0.0f;
    float y = 0.0f;
    float remaining_ms = 0.0f;
    // The cargo aircraft carrying it. If it is destroyed the fuel is lost.
    EntityId transport = INVALID_ENTITY;
    float total_ms = 0.0f;
};

struct FuelDepotStatus {
    EntityId id = INVALID_ENTITY;
    float x = 0.0f;
    float y = 0.0f;
    float stock = 0.0f;
    float capacity = 0.0f;
    int owner = -1;
    bool capturing = false;
    int capturing_faction = -1;
};

} // namespace rts
