#pragma once

#include <cstdint>

#include "simulation/simulation.hpp"

namespace rts {

class TickManager {
public:
    TickManager() = default;
    
    void set_target_tick_rate(float tick_rate_hz) {
        tick_interval_ms_ = 1000.0f / tick_rate_hz;
    }
    
    float get_tick_interval_ms() const { return tick_interval_ms_; }
    
    bool should_tick(float elapsed_ms) const {
        return elapsed_ms >= tick_interval_ms_;
    }
    
    float get_remaining_time(float elapsed_ms) const {
        return elapsed_ms - tick_interval_ms_;
    }
    
private:
    float tick_interval_ms_{50.0f};  // 20 Hz default
};

} // namespace rts
