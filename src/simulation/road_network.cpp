#include "simulation/road_network.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "data/json_parser.hpp"

namespace rts {

const RoadSettings& get_road_settings() {
    static const RoadSettings settings = [] {
        RoadSettings result;
        const char* configured_root = std::getenv("RTS_DATA_ROOT");
        const std::string path = std::string(configured_root != nullptr && configured_root[0] != '\0' ? configured_root : "data") + "/road_network.json";
        std::ifstream input(path);
        if (!input) return result;
        std::stringstream contents;
        contents << input.rdbuf();
        const auto parsed = data::JsonParser::parse(contents.str());
        if (!parsed || parsed->type() != data::JsonValue::Type::Object) return result;
        const auto number = [&parsed](const char* key, float fallback) {
            const auto value = parsed->get(key);
            return value && value->type() == data::JsonValue::Type::Number && std::isfinite(value->as_number())
                ? static_cast<float>(value->as_number()) : fallback;
        };
        result.width = std::max(1.0f, number("width", result.width));
        result.material_cost = std::max(0.0f, number("material_cost", result.material_cost));
        result.energy_cost = std::max(0.0f, number("energy_cost", result.energy_cost));
        result.construction_time_base_seconds = std::max(0.1f, number("construction_time_base_seconds", result.construction_time_base_seconds));
        result.construction_time_per_meter = std::max(0.0f, number("construction_time_per_meter", result.construction_time_per_meter));
        result.traversal_cost = std::clamp(number("traversal_cost", result.traversal_cost), 0.25f, 4.0f);
        return result;
    }();
    return settings;
}

bool RoadNetwork::queue(EntityId engineer, FactionId owner, float start_x, float start_y,
                        float end_x, float end_y, float build_time_seconds) {
    if (engineer == INVALID_ENTITY || !std::isfinite(start_x) || !std::isfinite(start_y) ||
        !std::isfinite(end_x) || !std::isfinite(end_y) || !std::isfinite(build_time_seconds) ||
        build_time_seconds <= 0.0f) return false;
    RoadSegment segment;
    segment.id = next_id_++;
    segment.engineer = engineer;
    segment.owner = owner;
    segment.start_x = start_x;
    segment.start_y = start_y;
    segment.end_x = end_x;
    segment.end_y = end_y;
    segment.width = get_road_settings().width;
    segment.build_time_seconds = build_time_seconds;
    const auto endpoint_distance = [](float ax, float ay, float bx, float by) {
        return std::hypot(ax - bx, ay - by);
    };
    for (auto& existing : segments_) {
        const float tolerance = std::max(existing.width, segment.width) * 1.5f;
        const bool connected =
            endpoint_distance(segment.start_x, segment.start_y, existing.start_x, existing.start_y) <= tolerance ||
            endpoint_distance(segment.start_x, segment.start_y, existing.end_x, existing.end_y) <= tolerance ||
            endpoint_distance(segment.end_x, segment.end_y, existing.start_x, existing.start_y) <= tolerance ||
            endpoint_distance(segment.end_x, segment.end_y, existing.end_x, existing.end_y) <= tolerance;
        if (connected) {
            segment.connected_segments.push_back(existing.id);
            existing.connected_segments.push_back(segment.id);
        }
    }
    segments_.push_back(std::move(segment));
    return true;
}

std::vector<RoadSegment> RoadNetwork::update(float delta_ms) {
    std::vector<RoadSegment> completed;
    if (!std::isfinite(delta_ms) || delta_ms <= 0.0f) return completed;
    for (auto& segment : segments_) {
        if (segment.completed) continue;
        segment.build_progress = std::min(
            1.0f,
            segment.build_progress + delta_ms / (segment.build_time_seconds * 1000.0f)
        );
        if (segment.build_progress >= 1.0f) {
            segment.completed = true;
            completed.push_back(segment);
        }
    }
    return completed;
}

void RoadNetwork::reset() {
    segments_.clear();
    next_id_ = 1;
}

} // namespace rts
