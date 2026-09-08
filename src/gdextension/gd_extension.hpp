#pragma once

#include <cstdint>

#include "ecs/entity.hpp"
#include "ecs/component_manager.hpp"
#include "spatial/spatial_grid.hpp"
#include "simulation/simulation.hpp"

// GDExtension
#include <godot/godot.hpp>
#include <godot/cpp/godot.hpp>
#include <godot/cpp/classes/world3d.hpp>
#include <godot/cpp/classes/camera3d.hpp>
#include <godot/cpp/classes/mesh_instance3d.hpp>
#include <godot/cpp/classes/vehicle_body3d.hpp>
#include <godot/cpp/classes/rigid_body3d.hpp>
#include <godot/cpp/classes/sprite2d.hpp>
#include <godot/cpp/classes/canvas_layer.hpp>

namespace rts {

using namespace godot;

class RtsExtension : public Object {
    GDCLASS(RtsExtension, Object)
    
protected:
    static void _bind_methods();
    
public:
    RtsExtension();
    ~RtsExtension();
    
    // Simulation control
    void start_simulation();
    void stop_simulation();
    void update_simulation(float delta_ms);
    
    // Entity management
    int create_unit(float x, float y);
    void move_unit(int entity_id, float x, float y);
    int get_entity_count() const;
    
    // Query
    Array query_units_in_region(float x, float y, float radius);
    
    // Production & Economy
    void economy_add_resource_node(int node_id, float x, float y, float amount, int type);
    void economy_add_extractor(int extractor_id, float x, float y, int node_id, float extraction_rate);
    void economy_add_storage(int storage_id, float x, float y, float metal_capacity, float energy_capacity, float research_capacity);
    void economy_add_production_line(int line_id, int storage_id, float build_speed_metal, float build_speed_energy, int max_jobs);
    void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick);
    void economy_update_all(float delta_ms);
    
    // Command systems
    size_t issue_move_commands(const Array& entity_ids, int player_id, float x, float y, float spacing);
    size_t issue_stop_commands(const Array& entity_ids, int player_id);
    size_t issue_attack_commands(const Array& entity_ids, int player_id, int target_entity_id);
    size_t issue_patrol_commands(const Array& entity_ids, int player_id, float x, float y);
    size_t issue_return_commands(const Array& entity_ids, int player_id);
    size_t issue_build_commands(const Array& entity_ids, int player_id, float x, float y, int64_t unit_type);
    size_t issue_harvest_commands(const Array& entity_ids, int player_id, float x, float y);
    size_t issue_defend_commands(const Array& entity_ids, int player_id, float x, float y);
    
private:
    Simulation simulation_;
};

} // namespace rts
