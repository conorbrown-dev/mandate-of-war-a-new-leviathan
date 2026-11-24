#pragma once

#include <cstddef>
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

// Ground-unit separation. Overlapping hulls are pushed apart by this fraction
// of their overlap each tick, at most kSeparationMaxPushMeters per unit per
// tick (8 m/s at 20 ticks/s). Units within kSeparationContactSlack of hull
// contact count as touching for crowded arrival: a unit stops when a touching
// unit occupies its destination, or when it is within kCrowdedArrivalMeters
// of its destination and touches a unit that has already stopped after a move
// to the same spot.
inline constexpr float kSeparationResolveFraction = 0.5f;
inline constexpr float kSeparationMaxPushMeters = 0.4f;
// A completely overfilled cell has quadratically many pairs. Resolve a
// deterministic bounded set per body each tick; ordinary platoons are below
// this limit and still check every nearby pair.
inline constexpr size_t kSeparationMaxNeighbors = 64;
inline constexpr float kSeparationContactSlack = 1.15f;
inline constexpr float kCrowdedArrivalMeters = 60.0f;
// A unit is on a destination when its centre is within this fraction of the
// two hulls' contact distance of it.
inline constexpr float kSeparationOnSpotFraction = 0.75f;
// Share of a push taken by a moving unit when it meets a stopped one, so
// columns slide around parked units instead of shoving them off their slots.
inline constexpr float kSeparationMoverShare = 0.9f;
// Making room. A stopped unit that a neighbour overlaps by more than
// (1 - kYieldOverlapFraction) of contact drives to a free spot instead of being
// slid: the neighbour that has stopped gives way to one that is still moving,
// and of two stopped units the higher id gives way. At most kYieldMaxPerTick
// new yields start each tick (lowest ids first, the rest wait a tick). A
// yielding unit gives up after kYieldMaxTicks and then waits kYieldCooldownTicks
// before it may be asked again. Spots are looked for on rings around the
// neighbour, kYieldDirections around, starting just clear of it and moving out
// one hull radius at a time for kYieldRings rings; the cheapest clear spot wins,
// counting its distance, how far the unit must turn, and a penalty for reversing.
// Only when no spot is clear, and for two moving units overlapping deeper than
// kSeparationDeepFraction of contact, are hulls pushed apart as before.
inline constexpr float kYieldOverlapFraction = 0.9f;
inline constexpr float kSeparationDeepFraction = 0.5f;
inline constexpr size_t kYieldMaxPerTick = 16;
inline constexpr uint8_t kYieldRetryTicks = 8;
inline constexpr uint16_t kYieldMaxTicks = 160;
inline constexpr uint16_t kYieldCooldownTicks = 12;
inline constexpr int kYieldDirections = 16;
inline constexpr int kYieldRings = 6;
inline constexpr float kYieldMarginMeters = 0.75f;
// A unit that stepped aside drives back to where it stood once that spot is
// clear, and gives up waiting after this many ticks (10 s).
inline constexpr uint16_t kYieldReturnWaitTicks = 200;
inline constexpr float kYieldHomeArrivalMeters = 1.0f;
// A unit standing within this of the spot it was last sent to counts that spot
// as its home when it steps aside.
inline constexpr float kYieldGoalSnapMeters = 3.0f;
inline constexpr float kYieldTurnPenaltyMeters = 6.0f; // for a half turn
inline constexpr float kYieldReversePenaltyMeters = 12.0f;
// Ground formation slots are at least this many hull lengths apart.
inline constexpr float kFormationHullSpacing = 1.25f;

} // namespace rts::movement
