#include "simulation/reinforcement_delivery.hpp"

#include <cmath>

namespace rts {

bool ReinforcementDelivery::configure(float zone_x, float zone_y, float zone_radius, FactionId owner) {
    reset();
    if (!std::isfinite(zone_x) || !std::isfinite(zone_y) || !std::isfinite(zone_radius) || zone_radius <= 0.0f) {
        reason_ = "INVALID DELIVERY ZONE";
        state_ = ReinforcementDeliveryState::REJECTED;
        return false;
    }
    zone_x_ = zone_x; zone_y_ = zone_y; zone_radius_ = zone_radius; owner_ = owner;
    state_ = ReinforcementDeliveryState::READY;
    reason_ = "SELECT A FRIENDLY DELIVERY ZONE";
    return true;
}

bool ReinforcementDelivery::select_zone(float x, float y, bool is_land, bool is_blocked) {
    if (state_ != ReinforcementDeliveryState::READY && state_ != ReinforcementDeliveryState::SELECTED) return false;
    const float dx = x - zone_x_, dy = y - zone_y_;
    if (dx * dx + dy * dy > zone_radius_ * zone_radius_) reason_ = "ZONE IS NOT FRIENDLY";
    else if (!is_land) reason_ = "DELIVERY REQUIRES LAND";
    else if (is_blocked) reason_ = "DELIVERY ZONE IS BLOCKED";
    else {
        selected_x_ = x; selected_y_ = y;
        state_ = ReinforcementDeliveryState::SELECTED;
        reason_ = "ZONE SELECTED // PRESS F TO REQUEST";
        return true;
    }
    state_ = ReinforcementDeliveryState::READY;
    return false;
}

bool ReinforcementDelivery::request(bool has_resources) {
    if (state_ != ReinforcementDeliveryState::SELECTED) { reason_ = "SELECT A VALID ZONE FIRST"; return false; }
    if (!has_resources) { reason_ = "INSUFFICIENT MATERIALS OR ENERGY"; return false; }
    state_ = ReinforcementDeliveryState::INCOMING;
    elapsed_ms_ = 0.0f;
    delivery_completed_ = false;
    reason_ = "TRANSPORT INBOUND";
    return true;
}

bool ReinforcementDelivery::update(float delta_ms) {
    if (state_ != ReinforcementDeliveryState::INCOMING || !std::isfinite(delta_ms) || delta_ms <= 0.0f) return false;
    elapsed_ms_ += delta_ms;
    if (elapsed_ms_ < package_.delivery_duration_ms) return false;
    elapsed_ms_ = package_.delivery_duration_ms;
    state_ = ReinforcementDeliveryState::COMPLETED;
    reason_ = "REINFORCEMENT DELIVERED";
    delivery_completed_ = true;
    return true;
}

void ReinforcementDelivery::reset() {
    state_ = ReinforcementDeliveryState::INACTIVE;
    elapsed_ms_ = 0.0f;
    delivery_completed_ = false;
    reason_.clear();
}

} // namespace rts
