#pragma once

#include <cstdint>
#include <vector>

#include "ecs/components/factions.hpp"

namespace rts {

enum class ControlPointState : uint8_t {
    NEUTRAL = 0,
    FRIENDLY = 1,
    ENEMY = 2,
    CONTESTED = 3,
};

enum class ControlPointBattleResult : uint8_t {
    INACTIVE = 0,
    ACTIVE = 1,
    VICTORY = 2,
    DEFEAT = 3,
};

struct ControlPointDefinition {
    float x = 0.0f;
    float y = 0.0f;
    float radius = 20.0f;
};

struct ControlPointUnitPresence {
    FactionId faction = FactionId::ELITE_PRECISION;
    float x = 0.0f;
    float y = 0.0f;
};

struct ControlPointSnapshot {
    ControlPointDefinition definition;
    ControlPointState state = ControlPointState::NEUTRAL;
    // -1 is fully enemy control, +1 is fully player control. The value is
    // intentionally exposed so presentation can show native capture feedback.
    float capture_progress = 0.0f;
    uint32_t friendly_units = 0;
    uint32_t enemy_units = 0;
};

// Deterministic, match-local objective rules. This deliberately does not
// mutate the broader territorial-control manager: a tactical point is an
// explicit skirmish objective rather than a replacement territory system.
class ControlPointBattle {
public:
    static constexpr float kCaptureRatePerUnitPerSecond = 0.20f;

    bool begin(const std::vector<ControlPointDefinition>& points, FactionId player,
               FactionId enemy, float hold_duration_ms);
    void reset();
    void update(float delta_ms, const std::vector<ControlPointUnitPresence>& units);

    bool active() const { return result_ == ControlPointBattleResult::ACTIVE; }
    ControlPointBattleResult result() const { return result_; }
    float hold_elapsed_ms() const { return hold_elapsed_ms_; }
    float hold_duration_ms() const { return hold_duration_ms_; }
    const std::vector<ControlPointSnapshot>& points() const { return points_; }

private:
    FactionId player_ = FactionId::ELITE_PRECISION;
    FactionId enemy_ = FactionId::MASS_WARFARE;
    ControlPointBattleResult result_ = ControlPointBattleResult::INACTIVE;
    float hold_elapsed_ms_ = 0.0f;
    float hold_duration_ms_ = 0.0f;
    bool player_has_held_all_points_ = false;
    std::vector<ControlPointSnapshot> points_;
};

} // namespace rts
