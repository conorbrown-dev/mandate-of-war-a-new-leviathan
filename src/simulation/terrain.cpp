#include "simulation/terrain.hpp"
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

float Terrain::height_at(float x, float y) const {
    int gx = static_cast<int>(x + 160.0f);
    int gy = static_cast<int>(y + 160.0f);
    
    if (gx < 0 || gx >= GRID_SIZE || gy < 0 || gy >= GRID_SIZE) {
        return 0.0f;
    }
    
    if (heights_.empty()) {
        return 0.0f;
    }
    
    return heights_[gy * GRID_SIZE + gx];
}
}
