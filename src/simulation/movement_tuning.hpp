#pragma once

#include <cstdint>

namespace rts::movement {

// Shared command-level constraints for deterministic ground movement. Chassis
// speed and steering values remain authored per unit in unit_faction_stats.json.
inline constexpr float kArrivalRadiusMeters = 0.75f;
inline constexpr float kMinimumFormationSpacingMeters = 3.0f;
inline constexpr float kMaximumFormationSpacingMeters = 100.0f;

// Authored unit speeds are realistic cross-country speeds. On long moves away
// from the enemy, ground units march at their road speed instead, about 1.6
// times cross-country for real armoured vehicles (70 against 43 km/h), and
// drop back to cross-country speed near an enemy.
inline constexpr float kRoadMarchSpeedMultiplier = 1.6f;
inline constexpr float kRoadMarchMinimumDistanceMeters = 150.0f;
inline constexpr float kContactCellMeters = 450.0f;
inline constexpr int kContactGridCells = 128;
inline constexpr uint32_t kContactRefreshTicks = 10;

} // namespace rts::movement
