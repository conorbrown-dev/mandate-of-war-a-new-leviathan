#pragma once

namespace rts::movement {

// Shared command-level constraints for deterministic ground movement. Chassis
// speed and steering values remain authored per unit in unit_faction_stats.json.
inline constexpr float kArrivalRadiusMeters = 0.75f;
inline constexpr float kMinimumFormationSpacingMeters = 3.0f;
inline constexpr float kMaximumFormationSpacingMeters = 100.0f;

} // namespace rts::movement
