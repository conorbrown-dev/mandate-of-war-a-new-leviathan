#pragma once

namespace rts {

struct Material {
    float current;
    float max;
    float generation_rate;
};

// Authoritative tactical economy state introduced by the resource-model
// migration. Research is intentionally absent: it belongs to campaign-level
// technology authorization, not an in-battle wallet.
struct Fuel {
    float current = 0.0f;
    float capacity = 0.0f;
};

struct Power {
    float generation_mw = 0.0f;
    float demand_mw = 0.0f;

    float reserve_mw() const { return generation_mw - demand_mw; }
    bool in_deficit() const { return reserve_mw() < 0.0f; }
};

struct TacticalResources {
    Material materials{};
    Fuel fuel{};
    Power power{};
};

} // namespace rts
