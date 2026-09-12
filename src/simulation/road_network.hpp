#pragma once

#include <cstdint>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/components/factions.hpp"

namespace rts {

struct RoadSettings {
    float width = 18.0f;
    float material_cost = 180.0f;
    float energy_cost = 120.0f;
    float construction_time_base_seconds = 8.0f;
    float construction_time_per_meter = 1.0f / 300.0f;
    float traversal_cost = 0.68f;
};

const RoadSettings& get_road_settings();

struct RoadSegment {
    uint32_t id = 0;
    EntityId engineer = INVALID_ENTITY;
    FactionId owner = FactionId::ELITE_PRECISION;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float end_x = 0.0f;
    float end_y = 0.0f;
    float width = 18.0f;
    float build_progress = 0.0f;
    float build_time_seconds = 10.0f;
    std::vector<uint32_t> connected_segments;
    bool completed = false;
};

class RoadNetwork {
public:
    bool queue(EntityId engineer, FactionId owner, float start_x, float start_y,
               float end_x, float end_y, float build_time_seconds);
    std::vector<RoadSegment> update(float delta_ms);
    void reset();

    const std::vector<RoadSegment>& segments() const { return segments_; }

private:
    uint32_t next_id_ = 1;
    std::vector<RoadSegment> segments_;
};

} // namespace rts
