#pragma once

namespace rts {

struct Material {
    float current;
    float max;
    float generation_rate;
};

struct Energy {
    float current;
    float max;
    float consumption_rate;
    float generation_rate;
};

struct Research {
    float current;
    float max;
    float research_points_per_tick;
};

} // namespace rts
