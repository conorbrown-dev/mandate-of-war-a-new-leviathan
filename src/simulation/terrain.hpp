#pragma once
#include <vector>
#include <string>

namespace rts {
class Terrain {
public:
    static constexpr int GRID_SIZE = 320;
    void load_from_binary(const std::string& path);
    float height_at(float x, float y) const;
    const std::vector<float>& data() const { return heights_; }
private:
    std::vector<float> heights_;
};
}
