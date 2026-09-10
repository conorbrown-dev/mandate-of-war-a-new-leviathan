#include <cstdint>
#include <filesystem>
#include <optional>

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "map/map_loader.hpp"
#include "ecs/components/factions.hpp"

using namespace godot;

extern "C" {
void simulation_start();
void simulation_stop();
void simulation_update(float delta_ms);
void simulation_reset();
int simulation_create_unit(float x, float y);
void simulation_move_unit(int entity_id, float x, float y);
void simulation_move_units_formation(
    const int32_t* entity_ids,
    int entity_count,
    float center_x,
    float center_y,
    float spacing
);
int simulation_issue_move_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float center_x,
    float center_y,
    float spacing
);
int simulation_issue_stop_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id
);
int simulation_issue_attack_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    int64_t target_entity_id
);
int simulation_issue_patrol_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y
);
int simulation_issue_return_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id
);
int simulation_issue_build_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y,
    int64_t unit_type
);
int simulation_issue_harvest_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y
);
int simulation_issue_defend_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y
);
void simulation_destroy_unit(int entity_id);
int simulation_entity_count();
float simulation_last_tick_ms();
float simulation_get_unit_x(int entity_id);
float simulation_get_unit_y(int entity_id);
int simulation_get_unit_positions(
    const int32_t* entity_ids,
    int entity_count,
    float* positions_xy,
    int position_capacity
);
void render_add_unit(float x, float y, uint32_t unit_type);
void render_update();
int render_get_instance_count();
void set_debug_mode(bool enabled);
void simulation_initialize_faction(int faction_id, float x, float y);
int simulation_create_unit_with_type(float x, float y, int unit_type, int faction_id);
int logistics_carrier_deck_occupancy(int facility_id);
int logistics_takeoff_queue_size(int facility_id);
int logistics_landing_queue_size(int facility_id);
int logistics_active_runway_operations(int facility_id);
bool logistics_is_safe_return(int entity_id);
int logistics_get_intelligence_age(int entity_id);
bool logistics_is_intelligence_stale(int entity_id);
void economy_add_resource_node(int node_id, float x, float y, float amount, int type);
void economy_add_extractor(int extractor_id, float x, float y, int node_id, float extraction_rate);
void economy_add_storage(int storage_id, float x, float y, float metal_capacity, float energy_capacity, float research_capacity);
void economy_add_production_line(int line_id, int storage_id, float build_speed_metal, float build_speed_energy, int max_jobs);
void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick);
void economy_update_all(float delta_ms);
int economy_get_resource_node_count();
bool economy_get_resource_node_info(int node_id, float* out_x, float* out_y, float* out_amount, int* out_type);
bool economy_get_storage_info(int storage_id, float* out_metal_storage, float* out_energy_storage, float* out_research_storage, 
                              float* out_metal_capacity, float* out_energy_capacity, float* out_research_capacity);
int economy_get_queue_size(int line_id);
int economy_get_completed_build_count();
int simulation_get_unit_health(int entity_id, float* current, float* max);
int simulation_get_unit_is_dead(int entity_id);
    bool combat_get_unit_health(int entity_id, float* out_current, float* out_max);
    int combat_get_unit_is_dead(int entity_id);
    bool combat_apply_damage(int entity_id, float damage);
    int combat_get_unit_faction_id(int entity_id);

void ai_init();
void ai_update(float delta_ms);
void ai_reset();
void ai_set_faction_id(int faction_id);
int ai_get_visible_unit_count();
int ai_get_enemy_unit_count();
void network_send_visual_pack_handshake(const char* pack_id, uint32_t pack_version, const char* pack_hash);
void network_set_expected_visual_pack(const char* pack_id, uint32_t pack_version, const char* pack_hash);
}

class RtsExtension final : public RefCounted {
    GDCLASS(RtsExtension, RefCounted);

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("start_simulation"), &RtsExtension::start_simulation);
        ClassDB::bind_method(D_METHOD("stop_simulation"), &RtsExtension::stop_simulation);
        ClassDB::bind_method(D_METHOD("reset_simulation"), &RtsExtension::reset_simulation);
        ClassDB::bind_method(D_METHOD("update_simulation", "delta_ms"), &RtsExtension::update_simulation);
        ClassDB::bind_method(D_METHOD("create_unit", "x", "y"), &RtsExtension::create_unit);
        ClassDB::bind_method(D_METHOD("move_unit", "entity_id", "x", "y"), &RtsExtension::move_unit);
        ClassDB::bind_method(
            D_METHOD("move_units_formation", "entity_ids", "center_x", "center_y", "spacing"),
            &RtsExtension::move_units_formation
        );
        ClassDB::bind_method(
            D_METHOD("issue_move_commands", "entity_ids", "player_id", "center_x", "center_y", "spacing"),
            &RtsExtension::issue_move_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_stop_commands", "entity_ids", "player_id"),
            &RtsExtension::issue_stop_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_attack_commands", "entity_ids", "player_id", "target_entity_id"),
            &RtsExtension::issue_attack_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_patrol_commands", "entity_ids", "player_id", "x", "y"),
            &RtsExtension::issue_patrol_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_return_commands", "entity_ids", "player_id"),
            &RtsExtension::issue_return_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_build_commands", "entity_ids", "player_id", "x", "y", "unit_type"),
            &RtsExtension::issue_build_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_harvest_commands", "entity_ids", "player_id", "x", "y"),
            &RtsExtension::issue_harvest_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_defend_commands", "entity_ids", "player_id", "x", "y"),
            &RtsExtension::issue_defend_commands
        );
        ClassDB::bind_method(D_METHOD("destroy_unit", "entity_id"), &RtsExtension::destroy_unit);
        ClassDB::bind_method(D_METHOD("get_entity_count"), &RtsExtension::get_entity_count);
        ClassDB::bind_method(D_METHOD("get_simulation_tick_ms"), &RtsExtension::get_simulation_tick_ms);
        ClassDB::bind_method(D_METHOD("get_unit_x", "entity_id"), &RtsExtension::get_unit_x);
        ClassDB::bind_method(D_METHOD("get_unit_y", "entity_id"), &RtsExtension::get_unit_y);
        ClassDB::bind_method(D_METHOD("get_unit_positions", "entity_ids"), &RtsExtension::get_unit_positions);
        ClassDB::bind_method(D_METHOD("initialize_faction", "faction_id", "x", "y"), &RtsExtension::initialize_faction);
        ClassDB::bind_method(D_METHOD("create_unit_with_type", "x", "y", "unit_type", "faction_id"), &RtsExtension::create_unit_with_type);
        ClassDB::bind_method(D_METHOD("get_unit_visual_id", "unit_type"), &RtsExtension::get_unit_visual_id);
        ClassDB::bind_method(D_METHOD("send_visual_pack_handshake", "pack_id", "pack_version", "sha256"), &RtsExtension::send_visual_pack_handshake);
        ClassDB::bind_method(D_METHOD("set_expected_visual_pack", "pack_id", "pack_version", "sha256"), &RtsExtension::set_expected_visual_pack);
        ClassDB::bind_method(D_METHOD("logistics_carrier_deck_occupancy", "facility_id"), &RtsExtension::logistics_carrier_deck_occupancy);
        ClassDB::bind_method(D_METHOD("logistics_takeoff_queue_size", "facility_id"), &RtsExtension::logistics_takeoff_queue_size);
        ClassDB::bind_method(D_METHOD("logistics_landing_queue_size", "facility_id"), &RtsExtension::logistics_landing_queue_size);
        ClassDB::bind_method(D_METHOD("logistics_active_runway_operations", "facility_id"), &RtsExtension::logistics_active_runway_operations);
        ClassDB::bind_method(D_METHOD("logistics_is_safe_return", "entity_id"), &RtsExtension::logistics_is_safe_return);
        ClassDB::bind_method(D_METHOD("logistics_get_intelligence_age", "entity_id"), &RtsExtension::logistics_get_intelligence_age);
        ClassDB::bind_method(D_METHOD("logistics_is_intelligence_stale", "entity_id"), &RtsExtension::logistics_is_intelligence_stale);
        ClassDB::bind_method(D_METHOD("economy_add_resource_node", "node_id", "x", "y", "amount", "type"), &RtsExtension::economy_add_resource_node);
        ClassDB::bind_method(D_METHOD("economy_add_extractor", "extractor_id", "x", "y", "node_id", "extraction_rate"), &RtsExtension::economy_add_extractor);
        ClassDB::bind_method(D_METHOD("economy_add_storage", "storage_id", "x", "y", "metal_capacity", "energy_capacity", "research_capacity"), &RtsExtension::economy_add_storage);
        ClassDB::bind_method(D_METHOD("economy_add_production_line", "line_id", "storage_id", "build_speed_metal", "build_speed_energy", "max_jobs"), &RtsExtension::economy_add_production_line);
        ClassDB::bind_method(D_METHOD("economy_enqueue_construction", "line_id", "entity_id", "type", "metal_cost", "energy_cost", "metal_per_tick", "energy_per_tick"), &RtsExtension::economy_enqueue_construction);
        ClassDB::bind_method(D_METHOD("economy_update_all", "delta_ms"), &RtsExtension::economy_update_all);
        ClassDB::bind_method(D_METHOD("economy_get_resource_node_count"), &RtsExtension::economy_get_resource_node_count);
        ClassDB::bind_method(D_METHOD("economy_get_resource_node_info", "node_id"), &RtsExtension::economy_get_resource_node_info);
        ClassDB::bind_method(D_METHOD("economy_get_storage_info", "storage_id"), &RtsExtension::economy_get_storage_info);
        ClassDB::bind_method(D_METHOD("economy_get_queue_size", "line_id"), &RtsExtension::economy_get_queue_size);
        ClassDB::bind_method(D_METHOD("economy_get_completed_build_count"), &RtsExtension::economy_get_completed_build_count);
    ClassDB::bind_method(D_METHOD("get_unit_health", "entity_id"), &RtsExtension::get_unit_health);
    ClassDB::bind_method(D_METHOD("get_unit_is_dead", "entity_id"), &RtsExtension::get_unit_is_dead);
    ClassDB::bind_method(D_METHOD("apply_damage", "entity_id", "damage"), &RtsExtension::apply_damage);
    ClassDB::bind_method(D_METHOD("get_unit_faction_id", "entity_id"), &RtsExtension::get_unit_faction_id);
        ClassDB::bind_method(D_METHOD("render_add_unit", "x", "y", "unit_type"), &RtsExtension::render_add_unit_instance);
        ClassDB::bind_method(D_METHOD("render_update"), &RtsExtension::update_renderer);
        ClassDB::bind_method(D_METHOD("render_get_instance_count"), &RtsExtension::get_render_instance_count);
        ClassDB::bind_method(D_METHOD("set_debug_mode", "enabled"), &RtsExtension::set_renderer_debug_mode);
        ClassDB::bind_method(D_METHOD("map_loader_load_map", "path"), &RtsExtension::map_loader_load_map);
        ClassDB::bind_method(D_METHOD("ai_init"), &RtsExtension::ai_init);
        ClassDB::bind_method(D_METHOD("ai_update", "delta_ms"), &RtsExtension::ai_update);
        ClassDB::bind_method(D_METHOD("ai_reset"), &RtsExtension::ai_reset);
        ClassDB::bind_method(D_METHOD("ai_set_faction_id", "faction_id"), &RtsExtension::ai_set_faction_id);
        ClassDB::bind_method(D_METHOD("ai_get_visible_unit_count"), &RtsExtension::ai_get_visible_unit_count);
        ClassDB::bind_method(D_METHOD("ai_get_enemy_unit_count"), &RtsExtension::ai_get_enemy_unit_count);
    }

public:
    ~RtsExtension() override {
        stop_simulation();
    }

    void start_simulation() {
        if (!started_) {
            simulation_start();
            started_ = true;
        }
    }

    void stop_simulation() {
        if (started_) {
            simulation_stop();
            started_ = false;
        }
    }

    void reset_simulation() {
        started_ = false;
        simulation_reset();
    }

    void update_simulation(double delta_ms) const {
        if (started_) {
            simulation_update(static_cast<float>(delta_ms));
        }
    }

    int64_t create_unit(double x, double y) const {
        return simulation_create_unit(static_cast<float>(x), static_cast<float>(y));
    }

    void move_unit(int64_t entity_id, double x, double y) const {
        simulation_move_unit(static_cast<int>(entity_id), static_cast<float>(x), static_cast<float>(y));
    }

    void move_units_formation(
        const PackedInt32Array& entity_ids,
        double center_x,
        double center_y,
        double spacing) const {
        simulation_move_units_formation(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<float>(center_x),
            static_cast<float>(center_y),
            static_cast<float>(spacing)
        );
    }

    int64_t issue_move_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id,
        double center_x,
        double center_y,
        double spacing) const {
        return simulation_issue_move_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(center_x),
            static_cast<float>(center_y),
            static_cast<float>(spacing)
        );
    }

    int64_t issue_stop_commands(const PackedInt32Array& entity_ids, int64_t player_id) const {
        return simulation_issue_stop_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id)
        );
    }

    int64_t issue_attack_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id,
        int64_t target_entity_id
    ) const {
        return simulation_issue_attack_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            target_entity_id
        );
    }

    int64_t issue_patrol_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id,
        double x,
        double y
    ) const {
        return simulation_issue_patrol_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(x),
            static_cast<float>(y)
        );
    }

    int64_t issue_return_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id
    ) const {
        return simulation_issue_return_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id)
        );
    }

    int64_t issue_build_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id,
        double x,
        double y,
        int64_t unit_type
    ) const {
        return simulation_issue_build_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<int>(unit_type)
        );
    }

    int64_t issue_harvest_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id,
        double x,
        double y
    ) const {
        return simulation_issue_harvest_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(x),
            static_cast<float>(y)
        );
    }

    int64_t issue_defend_commands(
        const PackedInt32Array& entity_ids,
        int64_t player_id,
        double x,
        double y
    ) const {
        return simulation_issue_defend_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(x),
            static_cast<float>(y)
        );
    }

    void initialize_faction(int64_t faction_id, double x, double y) {
        simulation_initialize_faction(static_cast<int>(faction_id), static_cast<float>(x), static_cast<float>(y));
    }

    int64_t create_unit_with_type(double x, double y, int64_t unit_type, int64_t faction_id) {
        return simulation_create_unit_with_type(
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<int>(unit_type),
            static_cast<int>(faction_id)
        );
    }

    int64_t logistics_carrier_deck_occupancy(int64_t facility_id) const {
        return ::logistics_carrier_deck_occupancy(static_cast<int>(facility_id));
    }

    int64_t logistics_takeoff_queue_size(int64_t facility_id) const {
        return ::logistics_takeoff_queue_size(static_cast<int>(facility_id));
    }

    int64_t logistics_landing_queue_size(int64_t facility_id) const {
        return ::logistics_landing_queue_size(static_cast<int>(facility_id));
    }

    int64_t logistics_active_runway_operations(int64_t facility_id) const {
        return ::logistics_active_runway_operations(static_cast<int>(facility_id));
    }

    bool logistics_is_safe_return(int64_t entity_id) const {
        return ::logistics_is_safe_return(static_cast<int>(entity_id)) != 0;
    }

    int64_t logistics_get_intelligence_age(int64_t entity_id) const {
        return ::logistics_get_intelligence_age(static_cast<int>(entity_id));
    }
    
    bool logistics_is_intelligence_stale(int64_t entity_id) const {
        return ::logistics_is_intelligence_stale(static_cast<int>(entity_id)) != 0;
    }

    void economy_add_resource_node(int64_t node_id, double x, double y, double amount, int64_t type) {
        ::economy_add_resource_node(
            static_cast<int>(node_id),
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(amount),
            static_cast<int>(type)
        );
    }

    void economy_add_extractor(int64_t extractor_id, double x, double y, int64_t node_id, double extraction_rate) {
        ::economy_add_extractor(
            static_cast<int>(extractor_id),
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<int>(node_id),
            static_cast<float>(extraction_rate)
        );
    }

    void economy_add_storage(int64_t storage_id, double x, double y, double metal_capacity, double energy_capacity, double research_capacity) {
        ::economy_add_storage(
            static_cast<int>(storage_id),
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(metal_capacity),
            static_cast<float>(energy_capacity),
            static_cast<float>(research_capacity)
        );
    }

    void economy_add_production_line(int64_t line_id, int64_t storage_id, double build_speed_metal, double build_speed_energy, int64_t max_jobs) {
        ::economy_add_production_line(
            static_cast<int>(line_id),
            static_cast<int>(storage_id),
            static_cast<float>(build_speed_metal),
            static_cast<float>(build_speed_energy),
            static_cast<int>(max_jobs)
        );
    }

    void economy_enqueue_construction(int64_t line_id, int64_t entity_id, int64_t type, double metal_cost, double energy_cost, double metal_per_tick, double energy_per_tick) {
        ::economy_enqueue_construction(
            static_cast<int>(line_id),
            static_cast<int>(entity_id),
            static_cast<int>(type),
            static_cast<float>(metal_cost),
            static_cast<float>(energy_cost),
            static_cast<float>(metal_per_tick),
            static_cast<float>(energy_per_tick)
        );
    }

    void economy_update_all(double delta_ms) {
        ::economy_update_all(static_cast<float>(delta_ms));
    }

    void destroy_unit(int64_t entity_id) const {
        simulation_destroy_unit(static_cast<int>(entity_id));
    }

    int64_t get_entity_count() const {
        return simulation_entity_count();
    }

    double get_simulation_tick_ms() const {
        return simulation_last_tick_ms();
    }

    double get_unit_x(int64_t entity_id) const {
        return simulation_get_unit_x(static_cast<int>(entity_id));
    }

    double get_unit_y(int64_t entity_id) const {
        return simulation_get_unit_y(static_cast<int>(entity_id));
    }

    PackedFloat32Array get_unit_positions(const PackedInt32Array& entity_ids) const {
        PackedFloat32Array positions;
        positions.resize(entity_ids.size() * 2);
        const int copied = simulation_get_unit_positions(
            entity_ids.ptr(),
            entity_ids.size(),
            positions.ptrw(),
            positions.size()
        );
        if (copied != entity_ids.size()) {
            positions.clear();
        }
        return positions;
    }

    void render_add_unit_instance(double x, double y, int64_t unit_type) const {
        ::render_add_unit(
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<uint32_t>(unit_type)
        );
    }

    void update_renderer() const {
        ::render_update();
    }

    int64_t get_render_instance_count() const {
        return ::render_get_instance_count();
    }

    void set_renderer_debug_mode(bool enabled) const {
        ::set_debug_mode(enabled);
    }

    Dictionary map_loader_load_map(const String& path) {
        std::filesystem::path fs_path(path.utf8().get_data());
        rts::MapLoader loader;
        auto result = loader.load_map(fs_path);
        
        Dictionary dict;
        if (result.has_value()) {
            const auto& map_data = result.value();
            dict["ok"] = true;
            dict["id"] = String(map_data.id.c_str());
            dict["name"] = String(map_data.name.c_str());
            dict["description"] = String(map_data.description.c_str());
            dict["author"] = String(map_data.author.c_str());
            dict["map_version"] = String(map_data.map_version.c_str());
            dict["width"] = map_data.width;
            dict["height"] = map_data.height;
            dict["tile_size"] = map_data.tile_size;
            dict["max_elevation"] = map_data.max_elevation;
            dict["tiles_x"] = map_data.tiles_x;
            dict["tiles_y"] = map_data.tiles_y;
            
            PackedFloat32Array spawn_x_list;
            PackedFloat32Array spawn_y_list;
            PackedStringArray spawn_faction_list;
            for (const auto& spawn : map_data.spawn_points) {
                spawn_x_list.append(spawn.x);
                spawn_y_list.append(spawn.y);
                spawn_faction_list.append(String(spawn.faction.c_str()));
            }
            dict["spawn_x"] = spawn_x_list;
            dict["spawn_y"] = spawn_y_list;
            dict["spawn_faction"] = spawn_faction_list;
            
            PackedFloat32Array landmass_ids;
            for (const auto& biome_entry : map_data.biomes) {
                landmass_ids.append(std::stoi(biome_entry.first));
            }
            dict["landmass_ids"] = landmass_ids;
        } else {
            dict["ok"] = false;
            dict["error"] = "Map load failed";
        }
        
        return dict;
    }

    void ai_init() {
        ::ai_init();
    }

    void ai_update(double delta_ms) {
        ::ai_update(static_cast<float>(delta_ms));
    }

    void ai_reset() {
        ::ai_reset();
    }

    void ai_set_faction_id(int64_t faction_id) {
        ::ai_set_faction_id(static_cast<int>(faction_id));
    }

    int64_t ai_get_visible_unit_count() const {
        return ::ai_get_visible_unit_count();
    }

    int64_t ai_get_enemy_unit_count() const {
        return ::ai_get_enemy_unit_count();
    }

    int economy_get_resource_node_count() const {
        return ::economy_get_resource_node_count();
    }
    
    Array economy_get_resource_node_info(int64_t node_id) const {
        float x, y, amount;
        int type;
        if (!::economy_get_resource_node_info(static_cast<int>(node_id), &x, &y, &amount, &type)) {
            return Array();
        }
        Array info;
        info.append(x);
        info.append(y);
        info.append(amount);
        info.append(type);
        return info;
    }
    
    Array economy_get_storage_info(int64_t storage_id) const {
        float metal_storage, energy_storage, research_storage;
        float metal_capacity, energy_capacity, research_capacity;
        if (!::economy_get_storage_info(static_cast<int>(storage_id),
                &metal_storage, &energy_storage, &research_storage,
                &metal_capacity, &energy_capacity, &research_capacity)) {
            return Array();
        }
        Array info;
        info.append(metal_storage);
        info.append(energy_storage);
        info.append(research_storage);
        info.append(metal_capacity);
        info.append(energy_capacity);
        info.append(research_capacity);
        return info;
    }
    
    int economy_get_queue_size(int64_t line_id) const {
        return ::economy_get_queue_size(static_cast<int>(line_id));
    }
    
    int economy_get_completed_build_count() const {
        return ::economy_get_completed_build_count();
    }
    
    Array get_unit_health(int64_t entity_id) const {
        float current, max;
        if (!::combat_get_unit_health(static_cast<int>(entity_id), &current, &max)) {
            return Array();
        }
        Array info;
        info.append(current);
        info.append(max);
        return info;
    }
    
    bool get_unit_is_dead(int64_t entity_id) const {
        return ::combat_get_unit_is_dead(static_cast<int>(entity_id)) != 0;
    }
    
    bool apply_damage(int64_t entity_id, float damage) const {
        return ::combat_apply_damage(static_cast<int>(entity_id), damage) != 0;
    }
    
    int64_t get_unit_faction_id(int64_t entity_id) const {
         return ::combat_get_unit_faction_id(static_cast<int>(entity_id));
    }

    String get_unit_visual_id(int64_t unit_type) const {
        if (unit_type < 0 || unit_type > 255) return String();
        const auto& prototypes = rts::get_unit_prototypes();
        const auto it = prototypes.find(static_cast<rts::UnitType>(unit_type));
        return it == prototypes.end() ? String() : String(it->second.visual_id.c_str());
    }

    bool send_visual_pack_handshake(const String& pack_id, int64_t pack_version, const String& sha256) const {
        if (pack_id.is_empty() || sha256.length() != 64 || pack_version < 0) return false;
        const CharString id = pack_id.utf8();
        const CharString hash = sha256.utf8();
        ::network_send_visual_pack_handshake(id.get_data(), static_cast<uint32_t>(pack_version), hash.get_data());
        return true;
    }

    bool set_expected_visual_pack(const String& pack_id, int64_t pack_version, const String& sha256) const {
        if (pack_id.is_empty() || sha256.length() != 64 || pack_version < 0) return false;
        const CharString id = pack_id.utf8();
        const CharString hash = sha256.utf8();
        ::network_set_expected_visual_pack(id.get_data(), static_cast<uint32_t>(pack_version), hash.get_data());
        return true;
    }

private:
    bool started_ = false;
};

void initialize_rts_extension_module(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(RtsExtension);
    }
}

void uninitialize_rts_extension_module(ModuleInitializationLevel level) {
    (void)level;
}

extern "C" GDExtensionBool GDE_EXPORT rts_extension_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization
) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_rts_extension_module);
    init.register_terminator(uninitialize_rts_extension_module);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
