#include "test_framework.hpp"
#include "simulation/terrain.hpp"
#include <fstream>

using namespace rts;
namespace {
int checks=0;
void verify(bool condition,const char* reason) { ++checks; if(!condition) throw std::runtime_error(reason); }
}

TEST(terrain_load_binary_heightmap) {
    std::vector<float> test_data(320 * 320);
    for (size_t i = 0; i < test_data.size(); ++i) {
        test_data[i] = static_cast<float>(i);
    }
    
    std::string path = "/tmp/test_heightmap.bin";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(test_data.data()), test_data.size() * sizeof(float));
    output.close();
    
    Terrain terrain;
    terrain.load_from_binary(path);
    
    verify(terrain.data().size() == 320 * 320, "terrain data size matches grid");
    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 320; ++x) {
            verify(std::abs(terrain.data()[y * 320 + x] - static_cast<float>(y * 320 + x)) < 0.001f, 
                   "terrain height value matches test data");
        }
    }
    std::cout << "terrain_load_binary_heightmap checks=" << checks << '\n';
}

TEST(terrain_height_at_coordinates) {
    std::vector<float> test_data(320 * 320);
    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 320; ++x) {
            test_data[y * 320 + x] = static_cast<float>(y * 320 + x);
        }
    }
    
    std::string path = "/tmp/test_heightmap.bin";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(test_data.data()), test_data.size() * sizeof(float));
    output.close();
    
    Terrain terrain;
    terrain.load_from_binary(path);
    
    // grid coordinates at world (0,0): gx=160, gy=160, so value = 160*320+160 = 51360
    verify(std::abs(terrain.height_at(-160.0f, -160.0f) - 0.0f) < 0.001f, "height at corner (-160,-160)");
    verify(std::abs(terrain.height_at(0.0f, 0.0f) - 51360.0f) < 0.001f, "height at center (0,0)");
    verify(std::abs(terrain.height_at(159.0f, 159.0f) - 102399.0f) < 0.001f, "height at corner (159,159)");
    std::cout << "terrain_height_at_coordinates checks=" << checks << '\n';
}

TEST(terrain_out_of_bounds_returns_zero) {
    std::vector<float> test_data(320 * 320, 50.0f);
    std::string path = "/tmp/test_heightmap.bin";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(test_data.data()), test_data.size() * sizeof(float));
    output.close();
    
    Terrain terrain;
    terrain.load_from_binary(path);
    
    verify(std::abs(terrain.height_at(-200.0f, 0.0f)) < 0.001f, "out-of-bounds west");
    verify(std::abs(terrain.height_at(200.0f, 0.0f)) < 0.001f, "out-of-bounds east");
    verify(std::abs(terrain.height_at(0.0f, -200.0f)) < 0.001f, "out-of-bounds north");
    verify(std::abs(terrain.height_at(0.0f, 200.0f)) < 0.001f, "out-of-bounds south");
    std::cout << "terrain_out_of_bounds_returns_zero checks=" << checks << '\n';
}
