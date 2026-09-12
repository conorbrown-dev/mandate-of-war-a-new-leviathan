#include "test_framework.hpp"

#include "pathfinding/pathfinding.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using WorldPoint = std::pair<float, float>;

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

void require_direction(const WorldPoint& direction, float x, float y, const char* message) {
    if (!near(direction.first, x) || !near(direction.second, y)) {
        throw std::runtime_error(message);
    }
}

void require_valid_cardinal_path(
    const rts::Pathfinding& pathfinding,
    const std::vector<WorldPoint>& path,
    std::size_t expected_size) {
    if (path.size() != expected_size) {
        throw std::runtime_error("Path did not have the expected shortest length");
    }

    for (std::size_t i = 0; i < path.size(); ++i) {
        const int x = pathfinding.to_grid_x(path[i].first);
        const int y = pathfinding.to_grid_y(path[i].second);
        if (!pathfinding.is_walkable(x, y)) {
            throw std::runtime_error("Path traversed a blocked or out-of-bounds cell");
        }

        if (i > 0) {
            const int previous_x = pathfinding.to_grid_x(path[i - 1].first);
            const int previous_y = pathfinding.to_grid_y(path[i - 1].second);
            if (std::abs(x - previous_x) + std::abs(y - previous_y) != 1) {
                throw std::runtime_error("Path contained a non-cardinal step");
            }
        }
    }
}

} // namespace

TEST(pathfinding_astar_returns_shortest_exact_cell_path) {
    rts::Pathfinding pathfinding(5, 5);
    const auto path = pathfinding.find_path(0.5f, 0.5f, 4.5f, 0.5f);

    require_valid_cardinal_path(pathfinding, path, 5);
    if (!near(path.front().first, 0.5f) || !near(path.front().second, 0.5f) ||
        !near(path.back().first, 4.5f) || !near(path.back().second, 0.5f)) {
        throw std::runtime_error("Path endpoints must match the requested grid-cell centers");
    }
}

TEST(pathfinding_astar_routes_around_obstacles) {
    rts::Pathfinding pathfinding(5, 5);
    for (int y = 0; y < 4; ++y) {
        pathfinding.set_cell(1, y, false);
    }

    const auto path = pathfinding.find_path(0.5f, 0.5f, 4.5f, 0.5f);
    require_valid_cardinal_path(pathfinding, path, 13);
}

TEST(pathfinding_prefers_a_low_cost_road_route) {
    rts::Pathfinding pathfinding(7, 3, 1.0f, 0.0f, 0.0f);
    for (int x = 0; x < 7; ++x) {
        pathfinding.set_traversal_cost(x, 0, 0.25f);
    }

    const auto path = pathfinding.find_path(0.5f, 1.5f, 6.5f, 1.5f);
    require_valid_cardinal_path(pathfinding, path, 9);
    bool used_road = false;
    for (const auto& point : path) {
        if (pathfinding.to_grid_y(point.second) == 0) {
            used_road = true;
            break;
        }
    }
    if (!used_road) {
        throw std::runtime_error("Weighted pathfinding must prefer a sufficiently cheap road detour");
    }
    if (pathfinding.movement_speed_multiplier(2.5f, 0.5f) <= 1.0f) {
        throw std::runtime_error("Road cells must provide a measurable movement benefit");
    }
}

TEST(pathfinding_world_structure_block_updates_routes) {
    rts::Pathfinding pathfinding(7, 3, 1.0f, 0.0f, 0.0f);
    pathfinding.block_world_area(3.5f, 1.5f, 0.0f);
    if (pathfinding.is_walkable(3, 1)) {
        throw std::runtime_error("World structure blocker must close its navigation cell");
    }
    const auto path = pathfinding.find_path(0.5f, 1.5f, 6.5f, 1.5f);
    require_valid_cardinal_path(pathfinding, path, 9);
}

TEST(pathfinding_world_structure_block_covers_footprint) {
    rts::Pathfinding pathfinding(9, 9, 1.0f, 0.0f, 0.0f);
    pathfinding.block_world_rectangle(4.5f, 4.5f, 1.2f, 2.2f);
    if (pathfinding.is_walkable(4, 3) || pathfinding.is_walkable(4, 4) || pathfinding.is_walkable(5, 5)) {
        throw std::runtime_error("Structure footprint must block every overlapping navigation cell");
    }
    if (!pathfinding.is_walkable(1, 4) || !pathfinding.is_walkable(7, 4)) {
        throw std::runtime_error("Structure footprint blocker must remain localized");
    }
    const auto path = pathfinding.find_path(0.5f, 4.5f, 8.5f, 4.5f);
    require_valid_cardinal_path(pathfinding, path, 17);
}

TEST(pathfinding_flow_direction_escapes_newly_blocked_occupied_cell) {
    rts::Pathfinding pathfinding(7, 3, 1.0f, 0.0f, 0.0f);
    pathfinding.block_world_area(2.5f, 1.5f, 0.0f);

    const auto direction = pathfinding.flow_direction(2.5f, 1.5f, 6.5f, 1.5f);
    if (std::fabs(direction.first) + std::fabs(direction.second) < 0.5f) {
        throw std::runtime_error("Units inside a newly blocked structure cell must be able to exit it");
    }
}

TEST(pathfinding_astar_rejects_unreachable_and_out_of_bounds_requests) {
    rts::Pathfinding pathfinding(5, 5);
    for (int y = 0; y < 5; ++y) {
        pathfinding.set_cell(1, y, false);
    }

    if (!pathfinding.find_path(0.5f, 0.5f, 4.5f, 0.5f).empty()) {
        throw std::runtime_error("Unreachable destinations must return no path");
    }
    if (!pathfinding.find_path(-0.25f, 0.5f, 0.5f, 0.5f).empty()) {
        throw std::runtime_error("Negative world coordinates must not truncate into the grid");
    }
    if (!pathfinding.find_path(0.5f, 0.5f, 5.0f, 0.5f).empty()) {
        throw std::runtime_error("Out-of-bounds destinations must return no path");
    }
}

TEST(pathfinding_astar_is_stable_across_repeated_calls) {
    rts::Pathfinding pathfinding(12, 12);
    for (int y = 1; y < 11; ++y) {
        pathfinding.set_cell(5, y, false);
    }

    const auto expected = pathfinding.find_path(1.5f, 6.5f, 10.5f, 6.5f);
    if (expected.empty()) {
        throw std::runtime_error("Repeated-call fixture must have a route");
    }

    for (int i = 0; i < 256; ++i) {
        if (pathfinding.find_path(1.5f, 6.5f, 10.5f, 6.5f) != expected) {
            throw std::runtime_error("Identical A* queries must return a stable route");
        }
    }
}

TEST(pathfinding_flow_field_points_to_immediate_walkable_cell) {
    rts::Pathfinding pathfinding(5, 5);
    for (int y = 0; y < 4; ++y) {
        pathfinding.set_cell(1, y, false);
    }

    const auto field = pathfinding.generate_flow_field(4.5f, 0.5f);
    require_direction(field[pathfinding.index(0, 0)], 0.0f, 1.0f,
                      "Flow field must route through the immediate BFS neighbor");
    require_direction(field[pathfinding.index(1, 0)], 0.0f, 0.0f,
                      "Blocked cells must not receive a flow direction");
    require_direction(field[pathfinding.index(4, 0)], 0.0f, 0.0f,
                      "Destination cell must have no outgoing direction");
}

TEST(pathfinding_flow_field_cache_keys_exact_destination) {
    rts::Pathfinding pathfinding(3, 3);

    const auto east = pathfinding.generate_flow_field(1.5f, 0.5f);
    const auto south = pathfinding.generate_flow_field(0.5f, 1.5f);

    require_direction(east[pathfinding.index(0, 0)], 1.0f, 0.0f,
                      "East destination should point east");
    require_direction(south[pathfinding.index(0, 0)], 0.0f, 1.0f,
                      "Distinct destinations in one sector need distinct cached fields");
}

TEST(pathfinding_flow_field_cache_invalidates_on_terrain_change) {
    rts::Pathfinding pathfinding(4, 3);

    const auto direct = pathfinding.generate_flow_field(2.5f, 0.5f);
    require_direction(direct[pathfinding.index(0, 0)], 1.0f, 0.0f,
                      "Open route should initially point at the destination");

    pathfinding.set_cell(1, 0, false);
    const auto detour = pathfinding.generate_flow_field(2.5f, 0.5f);
    require_direction(detour[pathfinding.index(0, 0)], 0.0f, 1.0f,
                      "Blocking the next cell must invalidate the cached route");

    pathfinding.clear_blocks();
    const auto restored = pathfinding.generate_flow_field(2.5f, 0.5f);
    require_direction(restored[pathfinding.index(0, 0)], 1.0f, 0.0f,
                      "Clearing blocks must invalidate and restore the direct route");
}

TEST(pathfinding_rejects_invalid_grid_and_non_finite_coordinates) {
    bool rejected_grid = false;
    try {
        rts::Pathfinding invalid(0, 4);
    } catch (const std::invalid_argument&) {
        rejected_grid = true;
    }
    if (!rejected_grid) {
        throw std::runtime_error("Invalid grid dimensions must be rejected before allocation");
    }

    rts::Pathfinding pathfinding(4, 4);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (!pathfinding.find_path(nan, 0.5f, 1.5f, 1.5f).empty()) {
        throw std::runtime_error("Non-finite path coordinates must be rejected");
    }
    require_direction(pathfinding.flow_direction(0.5f, 0.5f, nan, 1.5f), 0.0f, 0.0f,
                      "Non-finite flow destinations must be rejected");
}

TEST(pathfinding_flow_field_cache_is_bounded) {
    rts::Pathfinding pathfinding(20, 20);
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            pathfinding.generate_flow_field(
                pathfinding.to_world_x(x),
                pathfinding.to_world_y(y)
            );
        }
    }

    if (pathfinding.cached_flow_field_count() != rts::Pathfinding::MAX_CACHED_FLOW_FIELDS) {
        throw std::runtime_error("Flow-field cache must retain only its configured bounded capacity");
    }
    const std::size_t expected_bytes = rts::Pathfinding::MAX_CACHED_FLOW_FIELDS * 20U * 20U * 2U;
    if (pathfinding.cached_flow_field_bytes() != expected_bytes) {
        throw std::runtime_error("Flow-field cache byte accounting must match compact cardinal directions");
    }
}

TEST(pathfinding_supports_a_world_origin_offset) {
    rts::Pathfinding pathfinding(320, 320, 1.0f, -160.0f, -160.0f);

    if (pathfinding.to_grid_x(-159.5f) != 0 || pathfinding.to_grid_y(-159.5f) != 0 ||
        pathfinding.to_grid_x(159.5f) != 319 || pathfinding.to_grid_y(159.5f) != 319) {
        throw std::runtime_error("Origin-offset world coordinates did not map to the expected grid cells");
    }
    if (!near(pathfinding.to_world_x(0), -159.5f) ||
        !near(pathfinding.to_world_y(319), 159.5f)) {
        throw std::runtime_error("Origin-offset grid cells did not map back to world-space centers");
    }
    if (pathfinding.is_walkable(pathfinding.to_grid_x(-160.1f), 0) ||
        pathfinding.is_walkable(pathfinding.to_grid_x(160.0f), 0)) {
        throw std::runtime_error("Origin-offset pathfinding must reject coordinates beyond the terrain");
    }
}

TEST(pathfinding_line_of_sight_respects_blocked_and_invalid_cells) {
    rts::Pathfinding pathfinding(5, 5);
    pathfinding.set_cell(2, 2, false);

    if (pathfinding.has_line_of_sight(0.5f, 0.5f, 4.5f, 4.5f)) {
        throw std::runtime_error("Line of sight must not cross a blocked cell");
    }
    if (!pathfinding.has_line_of_sight(0.5f, 0.5f, 4.5f, 0.5f)) {
        throw std::runtime_error("Line of sight should accept an unobstructed row");
    }
    if (pathfinding.has_line_of_sight(-0.5f, 0.5f, 4.5f, 0.5f)) {
        throw std::runtime_error("Line of sight must reject out-of-bounds endpoints");
    }
}
