#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ecs/components/factions.hpp"

namespace rts {

// What each faction can see right now. Rebuilt from scratch on every refresh, so
// ground stops being watched the moment nothing is looking at it.
//
// Ground is watched along sight lines over the terrain, so a ridge hides what is
// behind it. The sky layer ignores terrain: a plane over a hill can still be seen
// from the far side of that hill.
class VisionMap {
public:
    static constexpr float kCellMeters = 100.0f;
    static constexpr size_t kFactions = 3;
    // A ground target shows if this much of it clears the crest in front of it.
    static constexpr float kTargetHeightMeters = 4.0f;

    void configure(float world_width, float world_height) {
        half_width_ = world_width * 0.5f;
        half_height_ = world_height * 0.5f;
        width_ = std::max(1, static_cast<int>(std::ceil(world_width / kCellMeters)));
        height_ = std::max(1, static_cast<int>(std::ceil(world_height / kCellMeters)));
        const size_t cell_count = static_cast<size_t>(width_) * height_;
        for (auto& cells : cells_) cells.assign(cell_count, 0);
        for (auto& cells : sky_) cells.assign(cell_count, 0);
        heights_.assign(cell_count, 0.0f);
    }

    // Terrain is sampled once and again only when earthworks change it, because
    // sight lines read it far more often than the terrain changes.
    template <typename HeightAt>
    void sample_terrain(HeightAt height_at) {
        for (int cell_y = 0; cell_y < height_; ++cell_y) {
            const float y = (static_cast<float>(cell_y) + 0.5f) * kCellMeters - half_height_;
            for (int cell_x = 0; cell_x < width_; ++cell_x) {
                const float x = (static_cast<float>(cell_x) + 0.5f) * kCellMeters - half_width_;
                heights_[static_cast<size_t>(cell_y) * width_ + cell_x] = height_at(x, y);
            }
        }
    }

    void clear() {
        for (auto& cells : cells_) std::fill(cells.begin(), cells.end(), uint8_t{0});
        for (auto& cells : sky_) std::fill(cells.begin(), cells.end(), uint8_t{0});
    }

    void reveal(FactionId faction, float x, float y, float radius, float eye_height) {
        if (cells_[index(faction)].empty() || radius <= 0.0f) return;
        reveal_sky(faction, x, y, radius);
        auto& cells = cells_[index(faction)];
        mark(cells, x, y);
        const float eye = terrain_height(x, y) + eye_height;
        // Rays close enough together to touch every cell at the rim of the disc.
        const int rays = std::max(12, static_cast<int>(std::ceil(radius / kCellMeters * 9.5f)));
        const float turn = 6.2831853f / static_cast<float>(rays);
        const float turn_cos = std::cos(turn);
        const float turn_sin = std::sin(turn);
        float direction_x = 1.0f;
        float direction_y = 0.0f;
        const float step = kCellMeters * 0.75f;
        for (int ray = 0; ray < rays; ++ray) {
            float steepest = -1.0e9f;
            for (float distance = std::min(step, radius);; distance = std::min(distance + step, radius)) {
                const float sample_x = x + direction_x * distance;
                const float sample_y = y + direction_y * distance;
                if (!inside(sample_x, sample_y)) break;
                const float ground = terrain_height(sample_x, sample_y);
                if ((ground + kTargetHeightMeters - eye) / distance >= steepest) mark(cells, sample_x, sample_y);
                steepest = std::max(steepest, (ground - eye) / distance);
                if (distance >= radius) break;
            }
            const float next_x = direction_x * turn_cos - direction_y * turn_sin;
            direction_y = direction_x * turn_sin + direction_y * turn_cos;
            direction_x = next_x;
        }
    }

    bool visible(FactionId faction, float x, float y) const { return read(cells_[index(faction)], x, y); }
    bool visible_in_sky(FactionId faction, float x, float y) const { return read(sky_[index(faction)], x, y); }

    const std::vector<uint8_t>& cells(FactionId faction) const { return cells_[index(faction)]; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    static size_t index(FactionId faction) { return static_cast<size_t>(faction); }
    int column(float x) const { return static_cast<int>(std::floor((x + half_width_) / kCellMeters)); }
    int row(float y) const { return static_cast<int>(std::floor((y + half_height_) / kCellMeters)); }
    bool inside(float x, float y) const {
        const int cell_x = column(x);
        const int cell_y = row(y);
        return cell_x >= 0 && cell_y >= 0 && cell_x < width_ && cell_y < height_;
    }

    bool read(const std::vector<uint8_t>& cells, float x, float y) const {
        if (cells.empty() || !inside(x, y)) return false;
        return cells[static_cast<size_t>(row(y)) * width_ + column(x)] != 0;
    }

    void mark(std::vector<uint8_t>& cells, float x, float y) const {
        if (inside(x, y)) cells[static_cast<size_t>(row(y)) * width_ + column(x)] = 255;
    }

    void reveal_sky(FactionId faction, float x, float y, float radius) {
        auto& cells = sky_[index(faction)];
        const int reach = static_cast<int>(std::ceil(radius / kCellMeters));
        const int center_x = column(x);
        const int center_y = row(y);
        const float radius_squared = radius * radius;
        for (int cell_y = std::max(0, center_y - reach); cell_y <= std::min(height_ - 1, center_y + reach); ++cell_y) {
            const float dy = (static_cast<float>(cell_y) + 0.5f) * kCellMeters - half_height_ - y;
            for (int cell_x = std::max(0, center_x - reach); cell_x <= std::min(width_ - 1, center_x + reach); ++cell_x) {
                const float dx = (static_cast<float>(cell_x) + 0.5f) * kCellMeters - half_width_ - x;
                if (dx * dx + dy * dy <= radius_squared) cells[static_cast<size_t>(cell_y) * width_ + cell_x] = 255;
            }
        }
    }

    // Blended between cell centres. Reading the nearest cell turns slopes into
    // stairs, and each stair would hide the ground just above it.
    float terrain_height(float x, float y) const {
        const float column_position = std::clamp((x + half_width_) / kCellMeters - 0.5f, 0.0f, static_cast<float>(width_ - 1));
        const float row_position = std::clamp((y + half_height_) / kCellMeters - 0.5f, 0.0f, static_cast<float>(height_ - 1));
        const int left = static_cast<int>(column_position);
        const int top = static_cast<int>(row_position);
        const int right = std::min(left + 1, width_ - 1);
        const int bottom = std::min(top + 1, height_ - 1);
        const float blend_x = column_position - static_cast<float>(left);
        const float blend_y = row_position - static_cast<float>(top);
        const auto height_of = [this](int cell_x, int cell_y) {
            return heights_[static_cast<size_t>(cell_y) * width_ + cell_x];
        };
        const float upper = height_of(left, top) + (height_of(right, top) - height_of(left, top)) * blend_x;
        const float lower = height_of(left, bottom) + (height_of(right, bottom) - height_of(left, bottom)) * blend_x;
        return upper + (lower - upper) * blend_y;
    }

    std::array<std::vector<uint8_t>, kFactions> cells_{};
    std::array<std::vector<uint8_t>, kFactions> sky_{};
    std::vector<float> heights_;
    float half_width_ = 0.0f;
    float half_height_ = 0.0f;
    int width_ = 0;
    int height_ = 0;
};

} // namespace rts
