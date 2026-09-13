#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rts {

class Pathfinding {
public:
    static constexpr std::size_t MAX_CACHED_FLOW_FIELDS = 128;

    Pathfinding(
        int grid_width,
        int grid_height,
        float cell_size = 1.0f,
        float origin_x = 0.0f,
        float origin_y = 0.0f
    );
    
    std::vector<std::pair<float, float>> find_path(float sx, float sy, float dx, float dy) const;
    std::vector<std::pair<float, float>> generate_flow_field(float dx, float dy) const;
    bool prewarm_flow_field(float dx, float dy) const;
    std::pair<float, float> flow_direction(float x, float y, float dx, float dy) const;
    bool has_line_of_sight(float sx, float sy, float dx, float dy) const;
    
    void set_cell(int x, int y, bool walkable);
    void set_traversal_cost(int x, int y, float cost);
    void clear_traversal_costs();
    float traversal_cost(int x, int y) const;
    float movement_speed_multiplier(float world_x, float world_y) const;
    void block_world_area(float world_x, float world_y, float radius);
    void block_world_rectangle(float world_x, float world_y, float half_width, float half_height);
    bool is_walkable(int x, int y) const;
    void clear_blocks();
    void clear_cache();
    std::size_t cached_flow_field_count() const { return flow_field_cache_.size(); }
    std::size_t cached_flow_field_bytes() const;
    std::size_t flow_field_generation_count() const { return flow_field_generation_count_; }
    void reset_flow_field_generation_count() const { flow_field_generation_count_ = 0; }
    
    float cell_size() const { return cell_size_; }
    int to_grid_x(float x) const;
    int to_grid_y(float y) const;
    float to_world_x(int x) const;
    float to_world_y(int y) const;
    float min_world_x_center() const { return to_world_x(0); }
    float max_world_x_center() const { return to_world_x(grid_width_ - 1); }
    float min_world_y_center() const { return to_world_y(0); }
    float max_world_y_center() const { return to_world_y(grid_height_ - 1); }
    int index(int x, int y) const;
    
private:
    int grid_width_;
    int grid_height_;
    float cell_size_;
    float origin_x_;
    float origin_y_;
    std::vector<bool> walkable_;
    std::vector<float> traversal_costs_;
    
    float heuristic(int x, int y, int dx, int dy) const;
    
    struct GridPos {
        int x, y;
        bool operator==(const GridPos& other) const { return x == other.x && y == other.y; }
    };
    
    struct GridPosHash {
        std::size_t operator()(const GridPos& pos) const {
            const auto x_hash = std::hash<int>{}(pos.x);
            const auto y_hash = std::hash<int>{}(pos.y);
            return x_hash ^ (y_hash + 0x9e3779b9U + (x_hash << 6U) + (x_hash >> 2U));
        }
    };

    struct CachedDirection {
        std::int8_t x;
        std::int8_t y;
    };
    using CachedFlowField = std::vector<CachedDirection>;

    const CachedFlowField* ensure_flow_field(int destination_x, int destination_y) const;

    mutable std::unordered_map<GridPos, CachedFlowField, GridPosHash> flow_field_cache_;
    mutable std::deque<GridPos> flow_field_cache_order_;
    mutable std::size_t flow_field_generation_count_{0};
};

} // namespace rts
