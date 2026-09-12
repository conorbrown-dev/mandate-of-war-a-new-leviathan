#include "simulation/terrain.hpp"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace rts {
void Terrain::load_from_binary(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Heightmap file unavailable: " + path);
    
    heights_.resize(GRID_SIZE * GRID_SIZE);
    input.read(reinterpret_cast<char*>(heights_.data()), heights_.size() * sizeof(float));
    
    if (!input) throw std::runtime_error("Heightmap file read failed");
}

void Terrain::set_world_bounds(float width, float height) {
    if (width > 0.0f && height > 0.0f && std::isfinite(width) && std::isfinite(height)) {
        world_width_ = width;
        world_height_ = height;
    }
}

float Terrain::height_at(float x, float y) const {
    const int gx = static_cast<int>(std::floor((x / world_width_ + 0.5f) * static_cast<float>(GRID_SIZE)));
    const int gy = static_cast<int>(std::floor((y / world_height_ + 0.5f) * static_cast<float>(GRID_SIZE)));
    
    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) {
        return 0.0f;
    }
    
    if (heights_.empty()) {
        return 0.0f;
    }
    
    return heights_[gy * GRID_SIZE + gx];
}
}
