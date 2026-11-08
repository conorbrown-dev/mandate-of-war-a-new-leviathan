#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace rts {
// A heightfield file is JSON: {"format":"heightfield","version":1,"width":W,
// "height":H,"heights":[...]}, in row order with heights in meters.
struct Heightfield {
    int width = 0;
    int height = 0;
    std::vector<float> heights;
};
Heightfield read_heightfield(const std::string& path);
void write_heightfield(const std::string& path, int width, int height, const std::vector<float>& heights);

class Terrain {
public:
    static constexpr int GRID_SIZE = 320;
    void load_from_file(const std::string& path);
    void set_world_bounds(float width, float height);
    float height_at(float x, float y) const;
    // Sets every height point touching the rectangle to one level.
    bool flatten_rect(float x, float y, float half_x, float half_y, float level);
    uint32_t revision() const { return revision_; }
    const std::vector<float>& data() const { return heights_; }
private:
    std::vector<float> heights_;
    float world_width_ = 320.0f;
    float world_height_ = 320.0f;
    uint32_t revision_ = 0;
};
}
