#include "renderer.hpp"
#include <chrono>
#include <cstdio>

namespace rts {

Renderer::Renderer() {
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize() {
    if (initialized_) {
        return true;
    }
    
    positions_.reserve(65536);
    colors_.reserve(65536);
    types_.reserve(65536);
    
    // Default camera
    camera_ = RenderCamera();
    camera_.position = {0, 20, 20};
    camera_.look_at = {0, 0, 0};
    
    fprintf(stderr, "Renderer initialized\n");
    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    if (!initialized_) {
        return;
    }
    
    positions_.clear();
    colors_.clear();
    types_.clear();
    
    fprintf(stderr, "Renderer shutdown\n");
    initialized_ = false;
}

void Renderer::set_camera(const RenderCamera& camera) {
    camera_ = camera;
}

void Renderer::set_world_bounds(float min_x, float min_y, float max_x, float max_y) {
    // Set world bounds for camera frustum culling
    // For now, just store - culling implemented in draw_all()
}

void Renderer::add_mesh_instance(const Mesh& mesh, const glm::vec3& position, const glm::vec3& color) {
    positions_.push_back(position);
    colors_.push_back(color);
    types_.push_back(0); // Type 0 = generic mesh
}

void Renderer::add_unit_instance(float x, float y, uint32_t unit_type) {
    positions_.push_back({x, 0, y});
    
    // Color by unit type
    glm::vec3 color;
    switch (unit_type % 4) {
        case 0: color = {0.0f, 0.8f, 0.0f}; break; // Blue - ally
        case 1: color = {0.8f, 0.0f, 0.0f}; break; // Red - enemy
        case 2: color = {0.0f, 0.0f, 0.8f}; break; // Green - neutral
        default: color = {0.5f, 0.5f, 0.5f}; break; // Gray - unknown
    }
    colors_.push_back(color);
    types_.push_back(unit_type);
}

void Renderer::begin_frame() {
    frame_start_ = std::chrono::high_resolution_clock::now();
    frame_time_ms_ = 0.0f;
}

void Renderer::draw_all() {
    if (!initialized_) {
        return;
    }
    
    // Simple debug rendering
    if (debug_mode_) {
        fprintf(stderr, "Renderer: drawing %zu instances\n", positions_.size());
    }
    
    // Simulate draw calls (actual rendering done via GDExtension later)
    for (size_t i = 0; i < positions_.size(); i++) {
        // In real implementation, this would:
        // 1. Update camera matrices
        // 2. Bind shader programs
        // 3. Set vertex buffers
        // 4. Issue draw calls via graphics API (Vulkan/Metal/DX12)
        
        if (debug_mode_ && i < 10) {
            fprintf(stderr, "  Instance %zu: pos=(%.1f, %.1f, %.1f), color=(%.2f, %.2f, %.2f)\n",
                i, positions_[i].x, positions_[i].y, positions_[i].z,
                colors_[i].x, colors_[i].y, colors_[i].z);
        }
    }
    
    if (debug_mode_ && positions_.size() > 10) {
        fprintf(stderr, "  ... and %zu more\n", positions_.size() - 10);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    frame_time_ms_ = std::chrono::duration<float, std::milli>(end - frame_start_).count();
}

void Renderer::end_frame() {
    // Present frame (swap buffers, etc.)
}

void Renderer::set_debug_mode(bool enabled) {
    debug_mode_ = enabled;
}

} // namespace rts

// C API exports for GDExtension
extern "C" {

void* renderer_create() {
    return new rts::Renderer();
}

void renderer_destroy(void* renderer) {
    delete static_cast<rts::Renderer*>(renderer);
}

void renderer_initialize(void* renderer) {
    static_cast<rts::Renderer*>(renderer)->initialize();
}

void renderer_shutdown(void* renderer) {
    static_cast<rts::Renderer*>(renderer)->shutdown();
}

void renderer_set_camera(void* renderer, float fov, float near_plane, float far_plane, float x, float y, float z, float lx, float ly, float lz, float zoom) {
    rts::RenderCamera camera;
    camera.fov = fov;
    camera.near_plane = near_plane;
    camera.far_plane = far_plane;
    camera.position = {x, y, z};
    camera.look_at = {lx, ly, lz};
    camera.zoom_level = zoom;
    static_cast<rts::Renderer*>(renderer)->set_camera(camera);
}

void renderer_add_unit_instance(void* renderer, float x, float y, uint32_t unit_type) {
    static_cast<rts::Renderer*>(renderer)->add_unit_instance(x, y, unit_type);
}

void renderer_begin_frame(void* renderer) {
    static_cast<rts::Renderer*>(renderer)->begin_frame();
}

void renderer_draw_all(void* renderer) {
    static_cast<rts::Renderer*>(renderer)->draw_all();
}

void renderer_end_frame(void* renderer) {
    static_cast<rts::Renderer*>(renderer)->end_frame();
}

void renderer_set_debug_mode(void* renderer, bool enabled) {
    static_cast<rts::Renderer*>(renderer)->set_debug_mode(enabled);
}

int renderer_get_instance_count(void* renderer) {
    return static_cast<rts::Renderer*>(renderer)->get_instance_count();
}

float renderer_get_frame_time_ms(void* renderer) {
    return static_cast<rts::Renderer*>(renderer)->get_frame_time_ms();
}

}
