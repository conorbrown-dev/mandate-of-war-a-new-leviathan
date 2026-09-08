#ifndef RTS_RENDER_RENDERER_HPP
#define RTS_RENDER_RENDERER_HPP

#include "mesh.hpp"
#include <vector>
#include <memory>
#include <chrono>

namespace rts {

struct RenderCamera {
    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    glm::vec3 position = {0, 10, 0};
    glm::vec3 look_at = {0, 0, 0};
    float zoom_level = 1.0f;
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    bool initialize();
    void shutdown();
    
    void set_camera(const RenderCamera& camera);
    void set_world_bounds(float min_x, float min_y, float max_x, float max_y);
    
    void add_mesh_instance(const Mesh& mesh, const glm::vec3& position, const glm::vec3& color);
    void add_unit_instance(float x, float y, uint32_t unit_type);
    
    void begin_frame();
    void draw_all();
    void end_frame();
    
    void set_debug_mode(bool enabled);
    
    // Stats
    int get_instance_count() const { return static_cast<int>(positions_.size()); }
    float get_frame_time_ms() const { return frame_time_ms_; }

private:
    std::vector<glm::vec3> positions_;
    std::vector<glm::vec3> colors_;
    std::vector<uint32_t> types_;
    RenderCamera camera_;
    bool initialized_ = false;
    bool debug_mode_ = false;
    float frame_time_ms_ = 0.0f;
    std::chrono::high_resolution_clock::time_point frame_start_;
};

} // namespace rts

#endif // RTS_RENDER_RENDERER_HPP
