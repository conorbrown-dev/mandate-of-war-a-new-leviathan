#include "simulation/control_point_battle.hpp"

#include <algorithm>
#include <cmath>

namespace rts {

bool ControlPointBattle::begin(const std::vector<ControlPointDefinition>& definitions,
                               FactionId player, FactionId enemy, float hold_duration_ms) {
    if (definitions.empty() || definitions.size() > 3 || player == enemy ||
        !std::isfinite(hold_duration_ms) || hold_duration_ms <= 0.0f) {
        return false;
    }

    points_.clear();
    points_.reserve(definitions.size());
    for (const auto& definition : definitions) {
        if (!std::isfinite(definition.x) || !std::isfinite(definition.y) ||
            !std::isfinite(definition.radius) || definition.radius <= 0.0f) {
            reset();
            return false;
        }
        points_.push_back(ControlPointSnapshot{definition});
    }
    player_ = player;
    enemy_ = enemy;
    hold_duration_ms_ = hold_duration_ms;
    hold_elapsed_ms_ = 0.0f;
    player_has_held_all_points_ = false;
    result_ = ControlPointBattleResult::ACTIVE;
    return true;
}

void ControlPointBattle::reset() {
    points_.clear();
    hold_elapsed_ms_ = 0.0f;
    hold_duration_ms_ = 0.0f;
    player_has_held_all_points_ = false;
    result_ = ControlPointBattleResult::INACTIVE;
}

void ControlPointBattle::update(float delta_ms, const std::vector<ControlPointUnitPresence>& units) {
    if (result_ != ControlPointBattleResult::ACTIVE || !std::isfinite(delta_ms) || delta_ms <= 0.0f) return;

    const float delta_progress = kCaptureRatePerUnitPerSecond * (delta_ms / 1000.0f);
    bool player_holding_all = true;
    bool enemy_holding_all = true;
    for (auto& point : points_) {
        point.friendly_units = 0;
        point.enemy_units = 0;
        const float radius_squared = point.definition.radius * point.definition.radius;
        for (const auto& unit : units) {
            const float dx = unit.x - point.definition.x;
            const float dy = unit.y - point.definition.y;
            if (dx * dx + dy * dy > radius_squared) continue;
            if (unit.faction == player_) ++point.friendly_units;
            else if (unit.faction == enemy_) ++point.enemy_units;
        }

        if (point.friendly_units > 0 && point.enemy_units > 0) {
            point.state = ControlPointState::CONTESTED;
        } else if (point.friendly_units > 0) {
            point.capture_progress = std::min(1.0f, point.capture_progress + delta_progress * point.friendly_units);
            point.state = point.capture_progress >= 1.0f ? ControlPointState::FRIENDLY : ControlPointState::CONTESTED;
        } else if (point.enemy_units > 0) {
            point.capture_progress = std::max(-1.0f, point.capture_progress - delta_progress * point.enemy_units);
            point.state = point.capture_progress <= -1.0f ? ControlPointState::ENEMY : ControlPointState::CONTESTED;
        } else if (std::abs(point.capture_progress) < 0.0001f) {
            point.capture_progress = 0.0f;
            point.state = ControlPointState::NEUTRAL;
        } else {
            point.state = point.capture_progress > 0.0f ? ControlPointState::FRIENDLY : ControlPointState::ENEMY;
        }

        player_holding_all = player_holding_all && point.state == ControlPointState::FRIENDLY &&
            point.friendly_units > 0 && point.enemy_units == 0;
        enemy_holding_all = enemy_holding_all && point.state == ControlPointState::ENEMY &&
            point.enemy_units > 0 && point.friendly_units == 0;
    }

    if (player_holding_all) {
        player_has_held_all_points_ = true;
        hold_elapsed_ms_ += delta_ms;
    } else if (enemy_holding_all && player_has_held_all_points_) {
        hold_elapsed_ms_ -= delta_ms;
    } else {
        hold_elapsed_ms_ = 0.0f;
    }

    if (hold_elapsed_ms_ >= hold_duration_ms_) result_ = ControlPointBattleResult::VICTORY;
    if (hold_elapsed_ms_ <= -hold_duration_ms_) result_ = ControlPointBattleResult::DEFEAT;
}

} // namespace rts
