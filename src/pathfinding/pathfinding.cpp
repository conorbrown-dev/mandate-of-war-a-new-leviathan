#include "pathfinding/pathfinding.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>

namespace rts {

namespace {

std::size_t checked_grid_size(int width, int height, float cell_size, float origin_x, float origin_y) {
    if (width <= 0 || height <= 0 || !std::isfinite(cell_size) || cell_size <= 0.0f ||
        !std::isfinite(origin_x) || !std::isfinite(origin_y)) {
        throw std::invalid_argument("Pathfinding grid dimensions, cell size, and origin must be finite and valid");
    }

    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > static_cast<std::size_t>(std::numeric_limits<int>::max()) / height_size) {
        throw std::length_error("Pathfinding grid is too large for indexed storage");
    }
    return width_size * height_size;
}

int world_to_grid_coordinate(float value, float cell_size, float origin) {
    const double scaled = (static_cast<double>(value) - static_cast<double>(origin)) /
        static_cast<double>(cell_size);
    if (!std::isfinite(scaled) ||
        scaled < static_cast<double>(std::numeric_limits<int>::min()) ||
        scaled > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(std::floor(scaled));
}

constexpr int NEIGHBOR_X[] = {0, 0, -1, 1};
constexpr int NEIGHBOR_Y[] = {-1, 1, 0, 0};

} // namespace

Pathfinding::Pathfinding(int grid_width, int grid_height, float cell_size, float origin_x, float origin_y)
    : grid_width_(grid_width),
      grid_height_(grid_height),
      cell_size_(cell_size),
      origin_x_(origin_x),
      origin_y_(origin_y),
      walkable_(checked_grid_size(grid_width, grid_height, cell_size, origin_x, origin_y), true),
      traversal_costs_(walkable_.size(), 1.0f) {}

int Pathfinding::index(int x, int y) const {
    return y * grid_width_ + x;
}

int Pathfinding::to_grid_x(float x) const {
    return world_to_grid_coordinate(x, cell_size_, origin_x_);
}

int Pathfinding::to_grid_y(float y) const {
    return world_to_grid_coordinate(y, cell_size_, origin_y_);
}

float Pathfinding::to_world_x(int x) const {
    return origin_x_ + x * cell_size_ + cell_size_ / 2.0f;
}

float Pathfinding::to_world_y(int y) const {
    return origin_y_ + y * cell_size_ + cell_size_ / 2.0f;
}

void Pathfinding::set_cell(int x, int y, bool walkable) {
    if (x < 0 || x >= grid_width_ || y < 0 || y >= grid_height_) {
        return;
    }

    const int cell_index = index(x, y);
    if (walkable_[cell_index] != walkable) {
        walkable_[cell_index] = walkable;
        clear_cache();
    }
}

void Pathfinding::set_traversal_cost(int x, int y, float cost) {
    if (x < 0 || x >= grid_width_ || y < 0 || y >= grid_height_ ||
        !std::isfinite(cost) || cost <= 0.0f) return;
    const float bounded_cost = std::clamp(cost, 0.25f, 4.0f);
    const int cell_index = index(x, y);
    if (traversal_costs_[cell_index] != bounded_cost) {
        traversal_costs_[cell_index] = bounded_cost;
        clear_cache();
    }
}

void Pathfinding::clear_traversal_costs() {
    std::fill(traversal_costs_.begin(), traversal_costs_.end(), 1.0f);
    clear_cache();
}

float Pathfinding::traversal_cost(int x, int y) const {
    if (x < 0 || x >= grid_width_ || y < 0 || y >= grid_height_) return 1.0f;
    return traversal_costs_[index(x, y)];
}

float Pathfinding::movement_speed_multiplier(float world_x, float world_y) const {
    const float cost = traversal_cost(to_grid_x(world_x), to_grid_y(world_y));
    return std::clamp(1.0f / cost, 0.5f, 1.75f);
}

void Pathfinding::block_world_area(float world_x, float world_y, float radius) {
    if (!std::isfinite(world_x) || !std::isfinite(world_y) || !std::isfinite(radius) || radius < 0.0f) return;
    const int center_x = to_grid_x(world_x);
    const int center_y = to_grid_y(world_y);
    const int cell_radius = std::max(0, static_cast<int>(std::ceil(radius / cell_size_)));
    for (int y = center_y - cell_radius; y <= center_y + cell_radius; ++y) {
        for (int x = center_x - cell_radius; x <= center_x + cell_radius; ++x) {
            if (x < 0 || x >= grid_width_ || y < 0 || y >= grid_height_) continue;
            if (std::hypot(static_cast<float>(x - center_x), static_cast<float>(y - center_y)) <= static_cast<float>(cell_radius) + 0.01f) {
                set_cell(x, y, false);
            }
        }
    }
}

void Pathfinding::block_world_rectangle(float world_x, float world_y, float half_width, float half_height) {
    if (!std::isfinite(world_x) || !std::isfinite(world_y) ||
        !std::isfinite(half_width) || !std::isfinite(half_height) ||
        half_width < 0.0f || half_height < 0.0f) return;
    const int min_x = to_grid_x(world_x - half_width - cell_size_ * 0.5f);
    const int max_x = to_grid_x(world_x + half_width + cell_size_ * 0.5f);
    const int min_y = to_grid_y(world_y - half_height - cell_size_ * 0.5f);
    const int max_y = to_grid_y(world_y + half_height + cell_size_ * 0.5f);
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            set_cell(x, y, false);
        }
    }
}

bool Pathfinding::is_walkable(int x, int y) const {
    if (x < 0 || x >= grid_width_ || y < 0 || y >= grid_height_) {
        return false;
    }
    return walkable_[index(x, y)];
}

void Pathfinding::clear_blocks() {
    std::fill(walkable_.begin(), walkable_.end(), true);
    clear_cache();
}

float Pathfinding::heuristic(int x, int y, int dx, int dy) const {
    // Traversal costs may be lower than one on completed roads. Scale the
    // Manhattan lower bound by the cheapest possible cell so A* remains
    // admissible when road routes are cheaper than direct off-road travel.
    return static_cast<float>(std::abs(x - dx) + std::abs(y - dy)) * 0.25f;
}

std::vector<std::pair<float, float>> Pathfinding::find_path(float sx, float sy, float dx, float dy) const {
    const int sx_grid = to_grid_x(sx);
    const int sy_grid = to_grid_y(sy);
    const int dx_grid = to_grid_x(dx);
    const int dy_grid = to_grid_y(dy);

    if (!is_walkable(sx_grid, sy_grid) || !is_walkable(dx_grid, dy_grid)) {
        return {};
    }

    if (sx_grid == dx_grid && sy_grid == dy_grid) {
        return {{to_world_x(dx_grid), to_world_y(dy_grid)}};
    }

    struct OpenNode {
        int cell;
        float g_score;
        float f_score;
    };
    struct LowerScoreFirst {
        bool operator()(const OpenNode& lhs, const OpenNode& rhs) const {
            if (lhs.f_score != rhs.f_score) {
                return lhs.f_score > rhs.f_score;
            }
            if (lhs.g_score != rhs.g_score) {
                return lhs.g_score > rhs.g_score;
            }
            return lhs.cell > rhs.cell;
        }
    };

    const int cell_count = grid_width_ * grid_height_;
    const int start_cell = index(sx_grid, sy_grid);
    const int destination_cell = index(dx_grid, dy_grid);
    std::vector<float> g_scores(cell_count, std::numeric_limits<float>::infinity());
    std::vector<int> came_from(cell_count, -1);
    std::vector<bool> closed(cell_count, false);
    std::priority_queue<OpenNode, std::vector<OpenNode>, LowerScoreFirst> open_set;

    g_scores[start_cell] = 0.0f;
    open_set.push({start_cell, 0.0f, heuristic(sx_grid, sy_grid, dx_grid, dy_grid)});

    while (!open_set.empty()) {
        const OpenNode current = open_set.top();
        open_set.pop();

        if (closed[current.cell] || current.g_score != g_scores[current.cell]) {
            continue;
        }

        if (current.cell == destination_cell) {
            std::vector<std::pair<float, float>> path;
            for (int cell = destination_cell; cell != -1; cell = came_from[cell]) {
                path.push_back({to_world_x(cell % grid_width_), to_world_y(cell / grid_width_)});
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        closed[current.cell] = true;
        const int current_x = current.cell % grid_width_;
        const int current_y = current.cell / grid_width_;
        for (int direction = 0; direction < 4; ++direction) {
            const int neighbor_x = current_x + NEIGHBOR_X[direction];
            const int neighbor_y = current_y + NEIGHBOR_Y[direction];
            if (!is_walkable(neighbor_x, neighbor_y)) {
                continue;
            }

            const int neighbor_cell = index(neighbor_x, neighbor_y);
            if (closed[neighbor_cell]) {
                continue;
            }

            const float tentative_g = current.g_score + traversal_cost(neighbor_x, neighbor_y);
            if (tentative_g < g_scores[neighbor_cell]) {
                g_scores[neighbor_cell] = tentative_g;
                came_from[neighbor_cell] = current.cell;
                open_set.push({
                    neighbor_cell,
                    tentative_g,
                    tentative_g + heuristic(neighbor_x, neighbor_y, dx_grid, dy_grid)
                });
            }
        }
    }

    return {};
}

std::vector<std::pair<float, float>> Pathfinding::generate_flow_field(float dx, float dy) const {
    const int dx_grid = to_grid_x(dx);
    const int dy_grid = to_grid_y(dy);
    const CachedFlowField* cached = ensure_flow_field(dx_grid, dy_grid);
    std::vector<std::pair<float, float>> flow_field(
        static_cast<std::size_t>(grid_width_ * grid_height_),
        {0.0f, 0.0f}
    );
    if (!cached) {
        return flow_field;
    }

    for (std::size_t cell = 0; cell < cached->size(); ++cell) {
        flow_field[cell] = {
            static_cast<float>((*cached)[cell].x),
            static_cast<float>((*cached)[cell].y)
        };
    }
    return flow_field;
}

bool Pathfinding::prewarm_flow_field(float dx, float dy) const {
    return ensure_flow_field(to_grid_x(dx), to_grid_y(dy)) != nullptr;
}

const Pathfinding::CachedFlowField* Pathfinding::ensure_flow_field(int dx_grid, int dy_grid) const {
    const GridPos destination{dx_grid, dy_grid};
    auto existing = flow_field_cache_.find(destination);
    if (existing != flow_field_cache_.end()) {
        return &existing->second;
    }
    if (!is_walkable(dx_grid, dy_grid)) {
        return nullptr;
    }

    ++flow_field_generation_count_;

    CachedFlowField flow_field(
        static_cast<std::size_t>(grid_width_ * grid_height_),
        CachedDirection{0, 0}
    );
    std::vector<int> came_from(grid_width_ * grid_height_, -1);
    std::vector<float> distance(grid_width_ * grid_height_, std::numeric_limits<float>::infinity());
    using QueueEntry = std::pair<float, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
    const int destination_cell = index(dx_grid, dy_grid);
    queue.push({0.0f, destination_cell});
    distance[destination_cell] = 0.0f;

    while (!queue.empty()) {
        const auto [current_distance, current_cell] = queue.top();
        queue.pop();
        if (current_distance != distance[current_cell]) continue;

        const int x = current_cell % grid_width_;
        const int y = current_cell / grid_width_;
        for (int direction = 0; direction < 4; ++direction) {
            const int neighbor_x = x + NEIGHBOR_X[direction];
            const int neighbor_y = y + NEIGHBOR_Y[direction];
            if (!is_walkable(neighbor_x, neighbor_y)) {
                continue;
            }

            const int neighbor_cell = index(neighbor_x, neighbor_y);
            const float candidate_distance = current_distance + traversal_cost(neighbor_x, neighbor_y);
            if (candidate_distance < distance[neighbor_cell]) {
                distance[neighbor_cell] = candidate_distance;
                came_from[neighbor_cell] = current_cell;
                queue.push({candidate_distance, neighbor_cell});
            }
        }
    }

    for (int y = 0; y < grid_height_; ++y) {
        for (int x = 0; x < grid_width_; ++x) {
            const int cell = index(x, y);
            if (x == dx_grid && y == dy_grid) {
                continue;
            }

            if (!std::isfinite(distance[cell])) {
                continue;
            }

            const int next_cell = came_from[cell];
            const int next_x = next_cell % grid_width_;
            const int next_y = next_cell / grid_width_;
            flow_field[cell] = {
                static_cast<std::int8_t>(next_x - x),
                static_cast<std::int8_t>(next_y - y)
            };
        }
    }

    if (flow_field_cache_.size() >= MAX_CACHED_FLOW_FIELDS) {
        flow_field_cache_.erase(flow_field_cache_order_.front());
        flow_field_cache_order_.pop_front();
    }
    auto [inserted, was_inserted] = flow_field_cache_.emplace(destination, std::move(flow_field));
    (void)was_inserted;
    flow_field_cache_order_.push_back(destination);

    return &inserted->second;
}

std::pair<float, float> Pathfinding::flow_direction(float x, float y, float dx, float dy) const {
    const int x_grid = to_grid_x(x);
    const int y_grid = to_grid_y(y);
    const int dx_grid = to_grid_x(dx);
    const int dy_grid = to_grid_y(dy);
    if (!is_walkable(dx_grid, dy_grid)) {
        std::fprintf(stderr, "DEBUG: flow_direction destination not walkable at (%d,%d)\n", dx_grid, dy_grid);
        return {0.0f, 0.0f};
    }

    const CachedFlowField* cached = ensure_flow_field(dx_grid, dy_grid);
    if (!cached) {
        return {0.0f, 0.0f};
    }

    // A structure can finish in the same coarse strategic cell as the
    // engineer that built it. Allow that unit to leave the newly blocked
    // cell, while keeping the structure cell unavailable to normal routes.
    if (!is_walkable(x_grid, y_grid)) {
        for (int direction = 0; direction < 4; ++direction) {
            const int neighbor_x = x_grid + NEIGHBOR_X[direction];
            const int neighbor_y = y_grid + NEIGHBOR_Y[direction];
            if (!is_walkable(neighbor_x, neighbor_y)) {
                continue;
            }
            const CachedDirection neighbor_direction = (*cached)[index(neighbor_x, neighbor_y)];
            if (neighbor_direction.x != 0 || neighbor_direction.y != 0) {
                return {
                    static_cast<float>(NEIGHBOR_X[direction]),
                    static_cast<float>(NEIGHBOR_Y[direction])
                };
            }
        }
        return {0.0f, 0.0f};
    }

    const CachedDirection direction = (*cached)[index(x_grid, y_grid)];
    return {static_cast<float>(direction.x), static_cast<float>(direction.y)};
}

bool Pathfinding::has_line_of_sight(float sx, float sy, float dx, float dy) const {
    int x = to_grid_x(sx);
    int y = to_grid_y(sy);
    const int destination_x = to_grid_x(dx);
    const int destination_y = to_grid_y(dy);
    if (!is_walkable(x, y) || !is_walkable(destination_x, destination_y)) {
        return false;
    }

    const double ray_x = static_cast<double>(dx) - static_cast<double>(sx);
    const double ray_y = static_cast<double>(dy) - static_cast<double>(sy);
    const int step_x = ray_x > 0.0 ? 1 : (ray_x < 0.0 ? -1 : 0);
    const int step_y = ray_y > 0.0 ? 1 : (ray_y < 0.0 ? -1 : 0);
    const double infinity = std::numeric_limits<double>::infinity();

    const double next_boundary_x = static_cast<double>(origin_x_) +
        static_cast<double>(x + (step_x > 0 ? 1 : 0)) * static_cast<double>(cell_size_);
    const double next_boundary_y = static_cast<double>(origin_y_) +
        static_cast<double>(y + (step_y > 0 ? 1 : 0)) * static_cast<double>(cell_size_);
    double next_x = step_x == 0 ? infinity : (next_boundary_x - static_cast<double>(sx)) / ray_x;
    double next_y = step_y == 0 ? infinity : (next_boundary_y - static_cast<double>(sy)) / ray_y;
    const double delta_x = step_x == 0 ? infinity : static_cast<double>(cell_size_) / std::abs(ray_x);
    const double delta_y = step_y == 0 ? infinity : static_cast<double>(cell_size_) / std::abs(ray_y);

    while (x != destination_x || y != destination_y) {
        // An endpoint on a grid boundary belongs to its destination cell.
        // Do not step past that cell while the other axis catches up.
        if (x == destination_x) next_x = infinity;
        if (y == destination_y) next_y = infinity;
        if (next_x < next_y) {
            x += step_x;
            next_x += delta_x;
        } else if (next_y < next_x) {
            y += step_y;
            next_y += delta_y;
        } else {
            if (!is_walkable(x + step_x, y) || !is_walkable(x, y + step_y)) {
                return false;
            }
            x += step_x;
            y += step_y;
            next_x += delta_x;
            next_y += delta_y;
        }

        if (!is_walkable(x, y)) {
            return false;
        }
    }

    return true;
}

void Pathfinding::clear_cache() {
    flow_field_cache_.clear();
    flow_field_cache_order_.clear();
}

std::size_t Pathfinding::cached_flow_field_bytes() const {
    return flow_field_cache_.size() * walkable_.size() * sizeof(CachedDirection);
}

} // namespace rts
