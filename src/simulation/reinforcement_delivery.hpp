#pragma once

#include <cstdint>
#include <string>

#include "ecs/components/factions.hpp"

namespace rts {

enum class ReinforcementDeliveryState : uint8_t { INACTIVE = 0, READY = 1, SELECTED = 2, INCOMING = 3, COMPLETED = 4, REJECTED = 5 };

struct ReinforcementPackage {
    UnitType unit_type = UnitType::INDUSTRIAL_MBT;
    float material_cost = 260.0f;
    float energy_cost = 140.0f;
    float delivery_duration_ms = 4000.0f;
};

class ReinforcementDelivery {
public:
    bool configure(float zone_x, float zone_y, float zone_radius, FactionId owner);
    bool select_zone(float x, float y, bool is_land, bool is_blocked);
    bool request(bool has_resources);
    bool update(float delta_ms);
    void reset();

    ReinforcementDeliveryState state() const { return state_; }
    const std::string& reason() const { return reason_; }
    float zone_x() const { return zone_x_; }
    float zone_y() const { return zone_y_; }
    float zone_radius() const { return zone_radius_; }
    FactionId owner() const { return owner_; }
    const ReinforcementPackage& package() const { return package_; }
    float elapsed_ms() const { return elapsed_ms_; }
    bool delivery_completed() const { return delivery_completed_; }

private:
    ReinforcementDeliveryState state_ = ReinforcementDeliveryState::INACTIVE;
    FactionId owner_ = FactionId::ELITE_PRECISION;
    float zone_x_ = 0.0f, zone_y_ = 0.0f, zone_radius_ = 0.0f;
    float selected_x_ = 0.0f, selected_y_ = 0.0f;
    float elapsed_ms_ = 0.0f;
    bool delivery_completed_ = false;
    std::string reason_;
    ReinforcementPackage package_;
};

} // namespace rts
