#include <algorithm>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <unordered_set>
#include <vector>
#include <limits>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "map/map_loader.hpp"
#include "ecs/components/factions.hpp"
#include "simulation/skirmish.hpp"
#include "simulation/simulation.hpp"
#include "stats/stats_manager.hpp"

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
    float spacing,
    int queued
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
int simulation_issue_install_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y,
    int installation_type
);
int simulation_issue_requisition_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y,
    int unit_type
);
void simulation_destroy_unit(int entity_id);
int simulation_entity_count();
float simulation_last_tick_ms();
float simulation_get_unit_x(int entity_id);
float simulation_get_unit_y(int entity_id);
int simulation_has_unit_move_target(int entity_id);
int simulation_get_unit_route(int entity_id, int player_id, float* points_xy, int point_capacity);
int simulation_get_unit_waypoints(int entity_id, int player_id, float* points_xy, int point_capacity);
int simulation_get_unit_positions(
    const int32_t* entity_ids,
    int entity_count,
    float* positions_xy,
    int position_capacity
);
int simulation_get_unit_headings(const int32_t* entity_ids, int entity_count, float* headings, int heading_capacity);
int simulation_get_unit_steering_state(int entity_id, float* heading, float* desired_heading, float* speed);
int simulation_get_unit_off_road_state(int entity_id, float* wear, float* distance, float* speed_multiplier);
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
void economy_add_tactical_storage(int storage_id, float x, float y, float materials_capacity, float fuel_capacity,
                                  float power_generation_mw, float power_demand_mw);
void economy_add_production_line(int line_id, int storage_id, float build_speed_metal, float build_speed_energy, int max_jobs);
void economy_enqueue_construction(int line_id, int entity_id, int type, float metal_cost, float energy_cost, float metal_per_tick, float energy_per_tick);
void economy_update_all(float delta_ms);
int economy_get_resource_node_count();
bool economy_get_resource_node_info(int node_id, float* out_x, float* out_y, float* out_amount, int* out_type);
bool economy_get_tactical_resources(int storage_id, float* out_materials, float* out_materials_capacity,
                                    float* out_fuel, float* out_fuel_capacity,
                                    float* out_power_generation_mw, float* out_power_demand_mw,
                                    float* out_power_reserve_mw);
int economy_get_queue_size(int line_id);
int economy_get_completed_build_count();
int territory_get_zone_count();
bool territory_get_zone_info(int zone_id, float* out_x, float* out_y, int* out_state, int* out_type, int* out_security);
bool territory_get_installation_info(float x, float y, int* out_type, int* out_faction, int* out_active, int* out_constructing, float* out_progress, float* out_cost);
Array territory_get_all_zones();

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
bool territory_get_zone_info(int zone_id, float* out_x, float* out_y, int* out_state, int* out_type, int* out_security);
}

class RtsExtension final : public RefCounted {
    GDCLASS(RtsExtension, RefCounted);

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("skirmish_save_replay", "path"), &RtsExtension::skirmish_save_replay);
        ClassDB::bind_method(D_METHOD("skirmish_verify_replay", "path"), &RtsExtension::skirmish_verify_replay);
        ClassDB::bind_method(D_METHOD("skirmish_load", "path"), &RtsExtension::skirmish_load);
        ClassDB::bind_method(D_METHOD("skirmish_update", "delta_ms"), &RtsExtension::skirmish_update);
        ClassDB::bind_method(D_METHOD("skirmish_state"), &RtsExtension::skirmish_state);
        ClassDB::bind_method(D_METHOD("skirmish_stats_summary"), &RtsExtension::skirmish_stats_summary);
        ClassDB::bind_method(D_METHOD("skirmish_command", "type", "ids", "x", "y", "extra"), &RtsExtension::skirmish_command);
        ClassDB::bind_method(D_METHOD("skirmish_update_intelligence", "entity_id", "x", "y", "tick"), &RtsExtension::skirmish_update_intelligence);
        ClassDB::bind_method(D_METHOD("skirmish_archive_intelligence", "entity_id"), &RtsExtension::skirmish_archive_intelligence);
        ClassDB::bind_method(D_METHOD("start_simulation"), &RtsExtension::start_simulation);
        ClassDB::bind_method(D_METHOD("stop_simulation"), &RtsExtension::stop_simulation);
        ClassDB::bind_method(D_METHOD("reset_simulation"), &RtsExtension::reset_simulation);
        ClassDB::bind_method(D_METHOD("configure_world_size", "width", "height"), &RtsExtension::configure_world_size);
        ClassDB::bind_method(D_METHOD("configure_theater_landmasses", "first_center_x", "first_center_y", "first_width", "first_height", "second_center_x", "second_center_y", "second_width", "second_height"), &RtsExtension::configure_theater_landmasses);
        ClassDB::bind_method(D_METHOD("load_terrain_heightmap", "path"), &RtsExtension::load_terrain_heightmap);
        ClassDB::bind_method(D_METHOD("get_structure_site", "structure_type", "x", "y"), &RtsExtension::get_structure_site);
        ClassDB::bind_method(D_METHOD("get_terrain_revision"), &RtsExtension::get_terrain_revision);
        ClassDB::bind_method(D_METHOD("get_terrain_heights"), &RtsExtension::get_terrain_heights);
        ClassDB::bind_method(D_METHOD("skirmish_set_ai_enabled", "enabled"), &RtsExtension::skirmish_set_ai_enabled);
        ClassDB::bind_method(D_METHOD("is_land_position", "x", "y"), &RtsExtension::is_land_position);
        ClassDB::bind_method(D_METHOD("block_civilian_area", "x", "y", "radius"), &RtsExtension::block_civilian_area);
        ClassDB::bind_method(D_METHOD("validate_structure_placement", "structure_type", "x", "y"), &RtsExtension::validate_structure_placement);
        ClassDB::bind_method(D_METHOD("validate_engineer_placement", "x", "y"), &RtsExtension::validate_engineer_placement);
        ClassDB::bind_method(D_METHOD("validate_road_placement", "start_x", "start_y", "end_x", "end_y"), &RtsExtension::validate_road_placement);
        ClassDB::bind_method(D_METHOD("queue_road", "engineer_id", "faction_id", "start_x", "start_y", "end_x", "end_y"), &RtsExtension::queue_road);
        ClassDB::bind_method(D_METHOD("get_road_segments"), &RtsExtension::get_road_segments);
        ClassDB::bind_method(D_METHOD("update_simulation", "delta_ms"), &RtsExtension::update_simulation);
        ClassDB::bind_method(D_METHOD("create_unit", "x", "y"), &RtsExtension::create_unit);
        ClassDB::bind_method(D_METHOD("create_faction_base", "faction_id", "x", "y"), &RtsExtension::create_faction_base);
        ClassDB::bind_method(D_METHOD("destroy_resource_site", "entity_id", "x", "y"), &RtsExtension::destroy_resource_site);
        ClassDB::bind_method(D_METHOD("destroy_unit", "entity_id"), &RtsExtension::destroy_unit);
        ClassDB::bind_method(D_METHOD("get_entity_ids"), &RtsExtension::get_entity_ids);
        ClassDB::bind_method(D_METHOD("get_unpresented_entities", "known_entity_ids"), &RtsExtension::get_unpresented_entities);
        ClassDB::bind_method(
            D_METHOD("move_units_formation", "entity_ids", "center_x", "center_y", "spacing"),
            &RtsExtension::move_units_formation
        );
        // queued=true appends a waypoint (Shift + right click) instead of
        // replacing each unit's move order.
        ClassDB::bind_method(
            D_METHOD("issue_move_commands", "entity_ids", "player_id", "center_x", "center_y", "spacing", "queued"),
            &RtsExtension::issue_move_commands,
            DEFVAL(false)
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
            D_METHOD("issue_build_commands", "entity_ids", "player_id", "x", "y", "unit_type"),
            &RtsExtension::issue_build_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_harvest_commands", "entity_ids", "player_id", "x", "y"),
            &RtsExtension::issue_harvest_commands
        );
        ClassDB::bind_method(
            D_METHOD("issue_install_commands", "entity_ids", "player_id", "x", "y", "installation_type"),
            &RtsExtension::issue_install_commands
        );
        ClassDB::bind_method(D_METHOD("get_entity_count"), &RtsExtension::get_entity_count);
        ClassDB::bind_method(D_METHOD("get_unit_position", "entity_id"), &RtsExtension::get_unit_position);
        ClassDB::bind_method(D_METHOD("has_unit_move_target", "entity_id"), &RtsExtension::has_unit_move_target);
        ClassDB::bind_method(D_METHOD("get_unit_routes", "entity_ids", "player_id"), &RtsExtension::get_unit_routes);
        ClassDB::bind_method(D_METHOD("get_unit_waypoints", "entity_ids", "player_id"), &RtsExtension::get_unit_waypoints);
        ClassDB::bind_method(D_METHOD("get_unit_transforms", "entity_ids"), &RtsExtension::get_unit_transforms);
        ClassDB::bind_method(D_METHOD("get_unit_steering_state", "entity_id"), &RtsExtension::get_unit_steering_state);
        ClassDB::bind_method(D_METHOD("create_unit_with_type", "x", "y", "unit_type", "faction_id"), &RtsExtension::create_unit_with_type);
        ClassDB::bind_method(D_METHOD("get_unit_visual_id", "unit_type"), &RtsExtension::get_unit_visual_id);
        ClassDB::bind_method(D_METHOD("get_unit_display_name", "entity_id"), &RtsExtension::get_unit_display_name);
        ClassDB::bind_method(D_METHOD("send_visual_pack_handshake", "pack_id", "pack_version", "sha256"), &RtsExtension::send_visual_pack_handshake);
        ClassDB::bind_method(D_METHOD("set_expected_visual_pack", "pack_id", "pack_version", "sha256"), &RtsExtension::set_expected_visual_pack);
        ClassDB::bind_method(D_METHOD("economy_add_resource_node", "node_id", "x", "y", "amount", "type"), &RtsExtension::economy_add_resource_node);
        ClassDB::bind_method(D_METHOD("economy_add_tactical_storage", "storage_id", "x", "y", "materials_capacity", "fuel_capacity", "power_generation_mw", "power_demand_mw"), &RtsExtension::economy_add_tactical_storage);
        ClassDB::bind_method(D_METHOD("economy_add_production_line", "line_id", "storage_id", "build_speed_metal", "build_speed_energy", "max_jobs"), &RtsExtension::economy_add_production_line);
        ClassDB::bind_method(D_METHOD("economy_get_resource_node_count"), &RtsExtension::economy_get_resource_node_count);
        ClassDB::bind_method(D_METHOD("economy_get_tactical_resources", "storage_id"), &RtsExtension::economy_get_tactical_resources);
        ClassDB::bind_method(D_METHOD("territory_get_zone_count"), &RtsExtension::territory_get_zone_count);
        ClassDB::bind_method(D_METHOD("territory_get_zone_info", "zone_id"), &RtsExtension::territory_get_zone_info);
        ClassDB::bind_method(D_METHOD("territory_get_installation_info", "x", "y"), &RtsExtension::territory_get_installation_info);
        ClassDB::bind_method(D_METHOD("control_point_battle_begin", "points", "player_faction", "enemy_faction", "hold_duration_ms"), &RtsExtension::control_point_battle_begin);
        ClassDB::bind_method(D_METHOD("control_point_battle_reset"), &RtsExtension::control_point_battle_reset);
        ClassDB::bind_method(D_METHOD("control_point_battle_state"), &RtsExtension::control_point_battle_state);
        ClassDB::bind_method(D_METHOD("reinforcement_delivery_configure", "x", "y", "radius", "faction_id"), &RtsExtension::reinforcement_delivery_configure);
        ClassDB::bind_method(D_METHOD("reinforcement_delivery_select_zone", "faction_id", "x", "y"), &RtsExtension::reinforcement_delivery_select_zone);
        ClassDB::bind_method(D_METHOD("reinforcement_delivery_request", "faction_id"), &RtsExtension::reinforcement_delivery_request);
        ClassDB::bind_method(D_METHOD("reinforcement_delivery_set_resources", "faction_id", "material", "energy"), &RtsExtension::reinforcement_delivery_set_resources);
        ClassDB::bind_method(D_METHOD("reinforcement_delivery_state"), &RtsExtension::reinforcement_delivery_state);
        ClassDB::bind_method(D_METHOD("match_begin", "player_post", "enemy_post"), &RtsExtension::match_begin);
        ClassDB::bind_method(D_METHOD("match_configure_key_sites", "site_ids", "required", "hold_seconds"), &RtsExtension::match_configure_key_sites);
        ClassDB::bind_method(D_METHOD("match_state"), &RtsExtension::match_state);
        ClassDB::bind_method(D_METHOD("ai_configure_assault", "min_x", "min_y", "max_x", "max_y", "first_wave", "wave_growth", "max_wave"), &RtsExtension::ai_configure_assault);
        ClassDB::bind_method(D_METHOD("ai_styles"), &RtsExtension::ai_styles);
        ClassDB::bind_method(D_METHOD("ai_set_style", "doctrine", "difficulty"), &RtsExtension::ai_set_style);
        ClassDB::bind_method(D_METHOD("ai_style"), &RtsExtension::ai_style);
        ClassDB::bind_method(D_METHOD("match_record_set_info", "key", "value"), &RtsExtension::match_record_set_info);
        ClassDB::bind_method(D_METHOD("match_record_state"), &RtsExtension::match_record_state);
        ClassDB::bind_method(D_METHOD("match_record_json"), &RtsExtension::match_record_json);
        ClassDB::bind_method(D_METHOD("vision_state", "faction_id"), &RtsExtension::vision_state);
        ClassDB::bind_method(D_METHOD("vision_revision"), &RtsExtension::vision_revision);
        ClassDB::bind_method(D_METHOD("can_see", "faction_id", "x", "y", "airborne"), &RtsExtension::can_see, DEFVAL(false));
        ClassDB::bind_method(D_METHOD("get_combat_events", "after_sequence", "faction_id"), &RtsExtension::get_combat_events);
        ClassDB::bind_method(D_METHOD("get_radio_events", "after_sequence", "faction_id"), &RtsExtension::get_radio_events);
        ClassDB::bind_method(D_METHOD("get_removed_entities", "entity_ids"), &RtsExtension::get_removed_entities);
        ClassDB::bind_method(D_METHOD("get_airborne_flags", "entity_ids"), &RtsExtension::get_airborne_flags);
        ClassDB::bind_method(D_METHOD("get_unit_range_profile", "entity_id"), &RtsExtension::get_unit_range_profile);
        ClassDB::bind_method(D_METHOD("get_aircraft_approach_poses", "entity_ids"), &RtsExtension::get_aircraft_approach_poses);
        ClassDB::bind_method(D_METHOD("forces_by_transport_set", "enabled"), &RtsExtension::forces_by_transport_set);
        ClassDB::bind_method(D_METHOD("configure_transport_route", "faction_id", "mode", "entry_x", "entry_y", "terminal_x", "terminal_y"), &RtsExtension::configure_transport_route);
        ClassDB::bind_method(D_METHOD("transport_route", "faction_id", "mode"), &RtsExtension::transport_route);
        ClassDB::bind_method(D_METHOD("order_ground_transport", "faction_id", "unit_type", "mode"), &RtsExtension::order_ground_transport);
        ClassDB::bind_method(D_METHOD("landing_zone_set", "faction_id", "x", "y"), &RtsExtension::landing_zone_set);
        ClassDB::bind_method(D_METHOD("delivery_state", "faction_id"), &RtsExtension::delivery_state);
        ClassDB::bind_method(D_METHOD("ground_fuel_burn_set", "enabled"), &RtsExtension::ground_fuel_burn_set);
        ClassDB::bind_method(D_METHOD("material_sites_state", "player_id"), &RtsExtension::material_sites_state);
        ClassDB::bind_method(D_METHOD("enemy_airfields_in_sight", "player_id"), &RtsExtension::enemy_airfields_in_sight);
        ClassDB::bind_method(D_METHOD("paradrop_refusal", "faction_id", "unit_type", "x", "y"), &RtsExtension::paradrop_refusal);
        ClassDB::bind_method(D_METHOD("order_paradrop", "faction_id", "unit_type", "x", "y"), &RtsExtension::order_paradrop);
        ClassDB::bind_method(D_METHOD("get_tick_clock"), &RtsExtension::get_tick_clock);
        ClassDB::bind_method(D_METHOD("set_game_speed", "scale"), &RtsExtension::set_game_speed);
        ClassDB::bind_method(D_METHOD("get_game_speed"), &RtsExtension::get_game_speed);
        ClassDB::bind_method(D_METHOD("is_multiplayer_session"), &RtsExtension::is_multiplayer_session);
        ClassDB::bind_method(D_METHOD("fuel_shipment_request", "faction_id"), &RtsExtension::fuel_shipment_request);
        ClassDB::bind_method(D_METHOD("fuel_supply_state", "faction_id"), &RtsExtension::fuel_supply_state);
        ClassDB::bind_method(D_METHOD("economy_assign_fuel_depot", "depot_id", "faction_id"), &RtsExtension::economy_assign_fuel_depot);
        ClassDB::bind_method(D_METHOD("power_add_node", "id", "kind", "x", "y", "owner", "output_mw", "demand_mw"), &RtsExtension::power_add_node);
        ClassDB::bind_method(D_METHOD("power_add_line", "id", "from", "to", "kind", "owner", "via"), &RtsExtension::power_add_line);
        ClassDB::bind_method(D_METHOD("power_grid_state", "faction_id"), &RtsExtension::power_grid_state);
        ClassDB::bind_method(D_METHOD("power_order", "entity_id", "faction_id", "order", "target"), &RtsExtension::power_order);
        ClassDB::bind_method(D_METHOD("economy_assign_material_site", "site_id", "faction_id"), &RtsExtension::economy_assign_material_site);
        ClassDB::bind_method(D_METHOD("get_build_catalog", "faction_id"), &RtsExtension::get_build_catalog);
        ClassDB::bind_method(D_METHOD("queue_faction_structure", "faction_id", "structure_type", "x", "y"), &RtsExtension::queue_faction_structure);
        ClassDB::bind_method(D_METHOD("queue_faction_unit", "faction_id", "unit_type", "x", "y"), &RtsExtension::queue_faction_unit);
        ClassDB::bind_method(D_METHOD("get_hud_state", "faction_id", "storage_id", "selected_entity_id", "fob_x", "fob_y", "include_fob"), &RtsExtension::get_hud_state);
        ClassDB::bind_method(D_METHOD("get_unit_health", "entity_id"), &RtsExtension::get_unit_health);
        ClassDB::bind_method(D_METHOD("map_loader_load_map", "path"), &RtsExtension::map_loader_load_map);
    }
    
    Array territory_get_zone_info(int64_t zone_id) const {
        float x, y;
        int state, type, security;
        if (!::territory_get_zone_info(static_cast<int>(zone_id), &x, &y, &state, &type, &security)) {
            return Array();
        }
        Array info;
        info.append(x);
        info.append(y);
        info.append(state);
        info.append(type);
        info.append(security);
        return info;
    }

    Array territory_get_installation_info(double x, double y) const {
        int type, faction, active, constructing;
        float progress, cost;
        if (!::territory_get_installation_info(static_cast<float>(x), static_cast<float>(y), &type, &faction,
                                               &active, &constructing, &progress, &cost)) return Array();
        Array info;
        info.append(type);
        info.append(faction);
        info.append(active);
        info.append(constructing);
        info.append(progress);
        info.append(cost);
        return info;
    }
    
    int territory_get_zone_count() const {
        return ::territory_get_zone_count();
    }

    bool control_point_battle_begin(const PackedFloat32Array& packed_points, int64_t player_faction,
                                    int64_t enemy_faction, double hold_duration_ms) const {
        if (packed_points.is_empty() || packed_points.size() % 3 != 0 ||
            player_faction < 0 || player_faction > 2 || enemy_faction < 0 || enemy_faction > 2 ||
            !std::isfinite(hold_duration_ms)) return false;
        std::vector<rts::ControlPointDefinition> points;
        points.reserve(static_cast<size_t>(packed_points.size() / 3));
        for (int index = 0; index < packed_points.size(); index += 3) {
            points.push_back({packed_points[index], packed_points[index + 1], packed_points[index + 2]});
        }
        return rts::runtime_simulation()->begin_control_point_battle(
            points, static_cast<rts::FactionId>(player_faction),
            static_cast<rts::FactionId>(enemy_faction), static_cast<float>(hold_duration_ms));
    }

    void control_point_battle_reset() const {
        rts::runtime_simulation()->reset_control_point_battle();
    }

    Dictionary control_point_battle_state() const {
        const auto& battle = rts::runtime_simulation()->control_point_battle();
        Dictionary state;
        state["result"] = static_cast<int>(battle.result());
        state["hold_elapsed_ms"] = battle.hold_elapsed_ms();
        state["hold_duration_ms"] = battle.hold_duration_ms();
        Array points;
        for (const auto& point : battle.points()) {
            Dictionary value;
            value["x"] = point.definition.x;
            value["y"] = point.definition.y;
            value["radius"] = point.definition.radius;
            value["state"] = static_cast<int>(point.state);
            value["capture_progress"] = point.capture_progress;
            value["friendly_units"] = point.friendly_units;
            value["enemy_units"] = point.enemy_units;
            points.append(value);
        }
        state["points"] = points;
        return state;
    }

    bool reinforcement_delivery_configure(double x, double y, double radius, int64_t faction_id) const {
        if (faction_id < 0 || faction_id > 2) return false;
        return rts::runtime_simulation()->configure_reinforcement_delivery(static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(radius), static_cast<rts::FactionId>(faction_id));
    }

    bool reinforcement_delivery_select_zone(int64_t faction_id, double x, double y) const {
        if (faction_id < 0 || faction_id > 2) return false;
        record_action("reinforcement_delivery_select_zone", {static_cast<double>(faction_id), x, y});
        return rts::runtime_simulation()->select_reinforcement_delivery_zone(static_cast<rts::FactionId>(faction_id),
            static_cast<float>(x), static_cast<float>(y));
    }

    bool reinforcement_delivery_request(int64_t faction_id) const {
        if (faction_id < 0 || faction_id > 2) return false;
        record_action("reinforcement_delivery_request", {static_cast<double>(faction_id)});
        return rts::runtime_simulation()->request_reinforcement_delivery(static_cast<rts::FactionId>(faction_id));
    }

    void reinforcement_delivery_set_resources(int64_t faction_id, double material, double energy) const {
        if (faction_id < 0 || faction_id > 2) return;
        record_action("reinforcement_delivery_set_resources", {static_cast<double>(faction_id), material, energy});
        rts::runtime_simulation()->set_reinforcement_resources(static_cast<rts::FactionId>(faction_id),
            static_cast<float>(material), static_cast<float>(energy));
    }

    void match_begin(int64_t player_post, int64_t enemy_post) const {
        rts::runtime_simulation()->begin_match(static_cast<rts::EntityId>(player_post), static_cast<rts::EntityId>(enemy_post));
    }

    bool match_configure_key_sites(const Array& site_ids, int64_t required, int64_t hold_seconds) const {
        if (required <= 0 || hold_seconds <= 0 || hold_seconds > 86400) return false;
        std::vector<rts::EntityId> ids;
        ids.reserve(site_ids.size());
        for (int index = 0; index < site_ids.size(); ++index) {
            const int64_t id = site_ids[index];
            if (id < 0 || id > std::numeric_limits<rts::EntityId>::max()) return false;
            ids.push_back(static_cast<rts::EntityId>(id));
        }
        return rts::runtime_simulation()->configure_key_site_victory(ids, static_cast<int>(required),
            static_cast<uint32_t>(hold_seconds * 20));
    }

    Dictionary match_state() const {
        auto* simulation = rts::runtime_simulation();
        Dictionary state;
        state["result"] = simulation->match_result();
        state["victory_reason"] = simulation->match_victory_reason();
        state["key_sites_required"] = simulation->key_sites_required();
        state["key_sites_player"] = simulation->key_sites_held(rts::FactionId::ELITE_PRECISION);
        state["key_sites_enemy"] = simulation->key_sites_held(rts::FactionId::MASS_WARFARE);
        state["key_site_hold_player_seconds"] = simulation->key_site_hold_ticks(rts::FactionId::ELITE_PRECISION) / 20.0;
        state["key_site_hold_enemy_seconds"] = simulation->key_site_hold_ticks(rts::FactionId::MASS_WARFARE) / 20.0;
        state["key_site_hold_target_seconds"] = simulation->key_site_hold_ticks_required() / 20.0;
        state["seconds"] = simulation->match_seconds();
        state["waves"] = simulation->ai_manager().waves_launched();
        return state;
    }

    void ai_configure_assault(double min_x, double min_y, double max_x, double max_y, int64_t first_wave, int64_t wave_growth, int64_t max_wave) const {
        rts::runtime_simulation()->ai_manager().configure_assault(static_cast<float>(min_x), static_cast<float>(min_y),
            static_cast<float>(max_x), static_cast<float>(max_y), static_cast<int>(first_wave), static_cast<int>(wave_growth), static_cast<int>(max_wave));
    }

    int64_t vision_revision() const { return rts::runtime_simulation()->vision_revision(); }

    bool can_see(int64_t faction_id, double x, double y, bool airborne = false) const {
        if (faction_id < 0 || faction_id > 2) return false;
        const auto* simulation = rts::runtime_simulation();
        return airborne ? simulation->can_see_sky(static_cast<rts::FactionId>(faction_id), x, y)
                        : simulation->can_see(static_cast<rts::FactionId>(faction_id), x, y);
    }

    Dictionary get_combat_events(int64_t after_sequence, int64_t faction_id) const {
        Dictionary state;
        Array events;
        if (after_sequence < 0 || faction_id < 0 || faction_id > 2) return state;
        const auto* simulation = rts::runtime_simulation();
        for (const auto& event : simulation->combat_events_after(after_sequence, static_cast<rts::FactionId>(faction_id))) {
            Dictionary item;
            item["sequence"] = static_cast<int64_t>(event.sequence);
            item["tick"] = event.tick;
            item["kind"] = static_cast<int>(event.kind);
            item["entity"] = event.entity;
            item["unit_type"] = event.unit_type;
            item["faction"] = event.faction;
            item["x"] = event.x; item["y"] = event.y;
            item["vx"] = event.velocity_x; item["vy"] = event.velocity_y;
            item["airborne"] = event.airborne;
            events.append(item);
        }
        state["cursor"] = static_cast<int64_t>(simulation->combat_event_cursor(static_cast<rts::FactionId>(faction_id)));
        state["tick"] = simulation->simulation_tick();
        state["events"] = events;
        return state;
    }

    // Presentation only: what this side's command network has been told.
    Dictionary get_radio_events(int64_t after_sequence, int64_t faction_id) const {
        Dictionary state;
        Array events;
        if (after_sequence < 0 || faction_id < 0 || faction_id > 2) return state;
        const auto* simulation = rts::runtime_simulation();
        const auto listener = static_cast<rts::FactionId>(faction_id);
        for (const auto& event : simulation->radio_events_after(after_sequence, listener)) {
            Dictionary item;
            item["sequence"] = static_cast<int64_t>(event.sequence);
            item["tick"] = event.tick;
            item["kind"] = static_cast<int>(event.kind);
            item["x"] = event.x; item["y"] = event.y;
            item["has_reference"] = event.has_reference;
            item["reference_x"] = event.reference_x; item["reference_y"] = event.reference_y;
            item["count"] = event.count;
            item["subject"] = event.subject;
            events.append(item);
        }
        state["cursor"] = static_cast<int64_t>(simulation->radio_event_cursor(listener));
        state["tick"] = simulation->simulation_tick();
        state["events"] = events;
        return state;
    }

    Dictionary vision_state(int64_t faction_id) const {
        Dictionary state;
        if (faction_id < 0 || faction_id > 2) return state;
        const auto* simulation = rts::runtime_simulation();
        const auto faction = static_cast<rts::FactionId>(faction_id);
        const auto& map = simulation->vision_map();
        const auto& cells = map.cells(faction);
        PackedByteArray bytes;
        bytes.resize(static_cast<int64_t>(cells.size()));
        if (!cells.empty()) std::memcpy(bytes.ptrw(), cells.data(), cells.size());
        PackedInt32Array seen;
        for (const auto entity : simulation->seen_enemies(faction)) seen.append(static_cast<int32_t>(entity));
        state["width"] = map.width();
        state["height"] = map.height();
        state["cells"] = bytes;
        state["seen_enemies"] = seen;
        // x, y, seconds since seen and unit type for each enemy now out of sight.
        // Fixed structures are left out: found ones stay on the map anyway.
        PackedFloat32Array last_known;
        const auto tick = simulation->simulation_tick();
        for (const auto& [id, sighting] : simulation->last_known(faction)) {
            (void)id;
            if (sighting.in_view || !sighting.mobile) continue;
            last_known.append(sighting.x);
            last_known.append(sighting.y);
            last_known.append(static_cast<float>(tick - sighting.tick) / 20.0f);
            last_known.append(static_cast<float>(sighting.unit_type));
        }
        state["last_known"] = last_known;
        PackedFloat32Array radar;
        for (const auto& contact : simulation->radar_contacts(faction)) {
            radar.append(contact[0]); radar.append(contact[1]);
        }
        state["radar_contacts"] = radar;
        return state;
    }

    // Every style and difficulty in data/ai_doctrines.json, for menus.
    Dictionary ai_styles() const {
        const auto& catalog = rts::get_ai_doctrines();
        Array doctrines, difficulties;
        for (const auto& doctrine : catalog.doctrines) {
            Dictionary entry;
            entry["key"] = String(doctrine.key.c_str());
            entry["name"] = String(doctrine.name.c_str());
            doctrines.push_back(entry);
        }
        for (const auto& difficulty : catalog.difficulties) {
            Dictionary entry;
            entry["key"] = String(difficulty.key.c_str());
            entry["name"] = String(difficulty.name.c_str());
            difficulties.push_back(entry);
        }
        Dictionary result;
        result["doctrines"] = doctrines;
        result["difficulties"] = difficulties;
        result["default_doctrine"] = String(catalog.default_doctrine.c_str());
        result["default_difficulty"] = String(catalog.default_difficulty.c_str());
        return result;
    }

    // Unknown keys change nothing and return false.
    bool ai_set_style(const String& doctrine_key, const String& difficulty_key) const {
        const auto* doctrine = rts::find_ai_doctrine(doctrine_key.utf8().get_data());
        const auto* difficulty = rts::find_ai_difficulty(difficulty_key.utf8().get_data());
        if (!doctrine || !difficulty) return false;
        auto* simulation = rts::runtime_simulation();
        simulation->ai_manager().set_doctrine(*doctrine);
        simulation->ai_manager().set_difficulty(*difficulty);
        simulation->match_record().set_info("ai_doctrine", doctrine->key);
        simulation->match_record().set_info("ai_difficulty", difficulty->key);
        return true;
    }

    Dictionary ai_style() const {
        const auto& ai = rts::runtime_simulation()->ai_manager();
        Dictionary state;
        state["doctrine"] = String(ai.doctrine().key.c_str());
        state["doctrine_name"] = String(ai.doctrine().name.c_str());
        state["difficulty"] = String(ai.difficulty().key.c_str());
        state["difficulty_name"] = String(ai.difficulty().name.c_str());
        return state;
    }

    void match_record_set_info(const String& key, const String& value) const {
        rts::runtime_simulation()->match_record().set_info(key.utf8().get_data(), value.utf8().get_data());
    }

    Dictionary match_record_state() const {
        const auto& record = rts::runtime_simulation()->match_record();
        Dictionary state;
        state["started"] = record.started();
        state["recording"] = record.recording();
        state["finished"] = record.started() && !record.recording();
        state["commands"] = static_cast<int64_t>(record.commands().size());
        state["actions"] = static_cast<int64_t>(record.actions().size());
        state["hashes"] = static_cast<int64_t>(record.hashes().size());
        state["result"] = record.result();
        return state;
    }

    String match_record_json() const {
        return String::utf8(rts::runtime_simulation()->match_record().to_json().c_str());
    }

    static void record_action(const char* name, std::vector<double> args) {
        rts::runtime_simulation()->record_external_action(name, std::move(args));
    }

    bool fuel_shipment_request(int64_t faction_id) const {
        if (faction_id < 0 || faction_id > 2) return false;
        record_action("fuel_shipment_request", {static_cast<double>(faction_id)});
        return rts::runtime_simulation()->request_fuel_shipment(static_cast<rts::FactionId>(faction_id));
    }

    bool economy_assign_material_site(int64_t site_id, int64_t faction_id) const {
        if (site_id <= 0 || faction_id < 0 || faction_id > 2) return false;
        record_action("economy_assign_material_site", {static_cast<double>(site_id), static_cast<double>(faction_id)});
        return rts::runtime_simulation()->assign_material_site(static_cast<rts::EntityId>(site_id), static_cast<rts::FactionId>(faction_id));
    }

    bool economy_assign_fuel_depot(int64_t depot_id, int64_t faction_id) const {
        if (depot_id <= 0 || faction_id < 0 || faction_id > 2) return false;
        record_action("economy_assign_fuel_depot", {static_cast<double>(depot_id), static_cast<double>(faction_id)});
        return rts::runtime_simulation()->assign_fuel_depot(
            static_cast<rts::EntityId>(depot_id), static_cast<rts::FactionId>(faction_id));
    }

    // Scenario setup for the power grid: stations (kind 0), junctions (1) and
    // loads (2), then the lines that join them. `via` is x,y pairs.
    bool power_add_node(int64_t id, int64_t kind, double x, double y, int64_t owner, double output_mw, double demand_mw) const {
        if (id <= 0 || id > 0xFFFF || kind < 0 || kind > 2 || owner < -1 || owner > 2) return false;
        record_action("power_add_node", {static_cast<double>(id), static_cast<double>(kind), x, y,
            static_cast<double>(owner), output_mw, demand_mw});
        rts::PowerNode node;
        node.id = static_cast<uint32_t>(id);
        node.kind = static_cast<rts::PowerNodeKind>(kind);
        node.x = static_cast<float>(x);
        node.y = static_cast<float>(y);
        node.owner = static_cast<int>(owner);
        node.output_mw = static_cast<float>(output_mw);
        node.demand_mw = static_cast<float>(demand_mw);
        return rts::runtime_simulation()->power_grid().add_node(node);
    }

    bool power_add_line(int64_t id, int64_t from, int64_t to, int64_t kind, int64_t owner, const PackedFloat32Array& via) const {
        if (id <= 0 || id > 0xFFFF || from <= 0 || to <= 0 || kind < 0 || kind > 1 || owner < -1 || owner > 2 ||
            via.size() % 2 != 0) return false;
        std::vector<double> args{static_cast<double>(id), static_cast<double>(from), static_cast<double>(to),
            static_cast<double>(kind), static_cast<double>(owner)};
        std::vector<std::array<float, 2>> points;
        for (int64_t index = 0; index + 1 < via.size(); index += 2) {
            points.push_back({via[index], via[index + 1]});
            args.push_back(via[index]);
            args.push_back(via[index + 1]);
        }
        record_action("power_add_line", std::move(args));
        return rts::runtime_simulation()->power_grid().add_line(static_cast<uint32_t>(id), static_cast<uint32_t>(from),
            static_cast<uint32_t>(to), static_cast<rts::PowerLineKind>(kind), static_cast<int>(owner), points);
    }

    // Sends a unit to work on the grid: 1 capture station, 2 capture line,
    // 3 cut line, 4 repair line. Answers at once with why not, or "" when the
    // recorded order went out.
    String power_order(int64_t entity_id, int64_t faction_id, int64_t order, int64_t target) const {
        if (entity_id <= 0 || faction_id < 0 || faction_id > 2 || order < 1 || order > 4 || target <= 0 || target > 0xFFFF)
            return "INVALID POWER ORDER";
        auto* simulation = rts::runtime_simulation();
        rts::FactionId owner = rts::FactionId::ELITE_PRECISION;
        if (!simulation->get_unit_faction_id(static_cast<rts::EntityId>(entity_id), owner) ||
            static_cast<int64_t>(owner) != faction_id) return "NO UNIT TO SEND";
        const auto work = static_cast<rts::Simulation::PowerOrder>(order);
        const std::string refusal = simulation->power_order_refusal(static_cast<rts::EntityId>(entity_id), work, static_cast<uint32_t>(target));
        if (!refusal.empty()) return String(refusal.c_str());
        return simulation->issue_power_command(static_cast<rts::EntityId>(entity_id), owner, work, static_cast<uint32_t>(target)) == 1
            ? String() : String("POWER ORDER REFUSED");
    }

    Dictionary power_grid_state(int64_t faction_id) const {
        Dictionary state;
        if (faction_id < 0 || faction_id > 2) return state;
        const auto* simulation = rts::runtime_simulation();
        const rts::PowerGrid& grid = simulation->power_grid();
        const rts::PowerSupply supply = grid.supply(static_cast<int>(faction_id));
        state["active"] = grid.active();
        state["generation_mw"] = supply.generation_mw;
        state["demand_mw"] = supply.demand_mw;
        state["notice"] = String(simulation->power_notice(static_cast<rts::FactionId>(faction_id)).c_str());
        Array nodes;
        for (const auto& [id, node] : grid.nodes()) {
            Dictionary entry;
            entry["id"] = static_cast<int64_t>(id);
            entry["kind"] = static_cast<int>(node.kind);
            entry["x"] = node.x;
            entry["y"] = node.y;
            entry["owner"] = node.owner;
            entry["output_mw"] = node.output_mw;
            entry["demand_mw"] = node.demand_mw;
            entry["powered"] = node.kind == rts::PowerNodeKind::LOAD && grid.load_powered(id);
            nodes.append(entry);
        }
        state["nodes"] = nodes;
        Array lines;
        for (const auto& [id, line] : grid.lines()) {
            Dictionary entry;
            entry["id"] = static_cast<int64_t>(id);
            entry["from"] = static_cast<int64_t>(line.from);
            entry["to"] = static_cast<int64_t>(line.to);
            entry["kind"] = static_cast<int>(line.kind);
            entry["owner"] = line.owner;
            entry["intact"] = line.intact;
            entry["length"] = line.length;
            entry["cut_x"] = line.cut_x;
            entry["cut_y"] = line.cut_y;
            entry["repairing"] = line.repair_remaining_ms > 0.0f;
            entry["repair_remaining_ms"] = line.repair_remaining_ms;
            entry["repair_cost"] = grid.repair_cost(id);
            entry["repair_seconds"] = grid.repair_seconds(id);
            // Only this side's own errands: the enemy's moves are not shown.
            int work = 0;
            for (const auto& task : simulation->power_tasks()) {
                rts::FactionId owner = rts::FactionId::ELITE_PRECISION;
                if (task.target != id || task.order == rts::Simulation::PowerOrder::CAPTURE_STATION ||
                    !rts::runtime_simulation()->get_unit_faction_id(task.unit, owner) ||
                    static_cast<int64_t>(owner) != faction_id) continue;
                work = static_cast<int>(task.order);
            }
            entry["work"] = work;
            lines.append(entry);
        }
        state["lines"] = lines;
        return state;
    }

    Dictionary fuel_supply_state(int64_t faction_id) const {
        Dictionary state;
        if (faction_id < 0 || faction_id > 2) return state;
        const auto* simulation = rts::runtime_simulation();
        const auto faction = static_cast<rts::FactionId>(faction_id);
        state["reason"] = String(simulation->fuel_supply_reason(faction).c_str());
        state["shipment_load"] = rts::kFuelShipmentLoad;
        state["shipment_material_cost"] = rts::kFuelShipmentMaterialCost;
        state["shipment_duration_ms"] = simulation->fuel_shipment(faction) ? simulation->fuel_shipment(faction)->total_ms : rts::kFuelShipmentDurationMs;
        state["ground_fuel_burn"] = simulation->ground_fuel_burn();
        state["starved_units"] = simulation->fuel_starved_units(faction);
        if (const auto* shipment = simulation->fuel_shipment(faction)) {
            state["shipment_remaining_ms"] = shipment->remaining_ms;
            state["shipment_x"] = shipment->x;
            state["shipment_y"] = shipment->y;
            state["shipment_transport"] = static_cast<int64_t>(shipment->transport);
        }
        Array depots;
        for (const auto& status : simulation->fuel_depots()) {
            Dictionary depot;
            depot["id"] = static_cast<int64_t>(status.id);
            depot["x"] = status.x;
            depot["y"] = status.y;
            depot["stock"] = status.stock;
            depot["capacity"] = status.capacity;
            depot["owner"] = status.owner;
            // Only this side's own capture: the enemy's moves are not shown.
            depot["capturing"] = status.capturing && status.capturing_faction == static_cast<int>(faction_id);
            depots.append(depot);
        }
        state["depots"] = depots;
        return state;
    }

    Dictionary reinforcement_delivery_state() const {
        const auto& delivery = rts::runtime_simulation()->reinforcement_delivery();
        Dictionary state;
        state["state"] = static_cast<int>(delivery.state());
        state["reason"] = String(delivery.reason().c_str());
        state["x"] = delivery.zone_x(); state["y"] = delivery.zone_y(); state["radius"] = delivery.zone_radius();
        state["delivery_x"] = delivery.selected_x(); state["delivery_y"] = delivery.selected_y();
        state["elapsed_ms"] = delivery.elapsed_ms(); state["duration_ms"] = delivery.package().delivery_duration_ms;
        state["material_cost"] = delivery.package().material_cost;
        state["unit_type"] = static_cast<int>(delivery.package().unit_type);
        return state;
    }

public:
    std::unique_ptr<rts::Skirmish> skirmish_;
    String skirmish_save_replay(const String& path) {
        String resolved_path = path;
        if (path.contains("://")) resolved_path = ProjectSettings::get_singleton()->globalize_path(path);
        if(!skirmish_) return "No match";
        return skirmish_->save_replay(resolved_path.utf8().get_data()) ? String() : String(skirmish_->error().c_str());
    }
    String skirmish_verify_replay(const String& path) {
        String resolved_path = path;
        if (path.contains("://")) resolved_path = ProjectSettings::get_singleton()->globalize_path(path);
        if(!skirmish_) skirmish_ = std::make_unique<rts::Skirmish>(*rts::runtime_simulation());
        return skirmish_->replay(resolved_path.utf8().get_data()) ? String() : String(skirmish_->error().c_str());
    }
    String skirmish_load(const String& path) {
        String resolved_path = path;
        if (path.begins_with("res://")) {
            String relative_path = path.substr(6);
            resolved_path = "res://" + relative_path;
            resolved_path = ProjectSettings::get_singleton()->globalize_path(resolved_path);
        }
        if (!skirmish_) skirmish_ = std::make_unique<rts::Skirmish>(*rts::runtime_simulation());
        if (!skirmish_->load(resolved_path.utf8().get_data())) return String(skirmish_->error().c_str());
        recorded_result_ = -1;
        started_ = true;
        return "";
    }
    void skirmish_update(double delta_ms) {
        if (!skirmish_) return;
        skirmish_->update(delta_ms);
        if (skirmish_->result() < 0 || recorded_result_ == skirmish_->result()) return;
        if (!stats_) {
            const auto path = ProjectSettings::get_singleton()->globalize_path("user://matches");
            stats_ = std::make_unique<rts::StatsManager>(path.utf8().get_data());
        }
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
        stats_->record_match(rts::MatchStats{
            skirmish_->name(), static_cast<uint64_t>(seconds), "player", "Elite Precision",
            skirmish_->result() == 0, static_cast<uint32_t>(skirmish_->checksums().size()),
            0, 0, 0, 0, 0, 0});
        recorded_result_ = skirmish_->result();
    }
    void skirmish_update_intelligence(int64_t entity_id, double x, double y, int64_t tick) {
        if (!skirmish_ || entity_id <= 0 || tick < 0) return;
        rts::runtime_simulation()->logistics_manager().update_intelligence(
            static_cast<rts::EntityId>(entity_id), static_cast<float>(x), static_cast<float>(y), static_cast<uint32_t>(tick));
    }
    void skirmish_archive_intelligence(int64_t entity_id) {
        if (!skirmish_ || entity_id <= 0) return;
        rts::runtime_simulation()->logistics_manager().archive_intelligence(static_cast<rts::EntityId>(entity_id));
    }
    int64_t skirmish_command(int64_t type, const PackedInt32Array& ids, double x, double y, int64_t extra) {
        if (!skirmish_ || skirmish_->result() != -1 || type < 0 || type > 8 || extra < 0 || extra > UINT32_MAX) return 0;
        std::vector<rts::EntityId> entities;
        for (int index=0; index<ids.size(); ++index) { if (ids[index]<=0) return 0; entities.push_back(ids[index]); }
        return rts::runtime_simulation()->issue_commands(entities,rts::FactionId::ELITE_PRECISION,
            static_cast<rts::CommandType>(type),x,y,extra);
    }
    bool skirmish_position_visible(double x, double y) {
        return skirmish_ && rts::runtime_simulation()->is_position_visible_to(rts::FactionId::ELITE_PRECISION, x, y);
    }
    Dictionary skirmish_state() {
        Dictionary state;
        if (!skirmish_) return state;
        auto& simulation = *rts::runtime_simulation();
        state["result"] = skirmish_->result(); state["tick"] = simulation.simulation_tick();
        state["base"] = skirmish_->base(0);
        auto& logistics=simulation.logistics_manager();
        state["takeoff_queue"]=static_cast<int64_t>(logistics.takeoff_queue_size(skirmish_->base(0)));
        state["landing_queue"]=static_cast<int64_t>(logistics.landing_queue_size(skirmish_->base(0)));
        state["runway_active"]=static_cast<int64_t>(logistics.active_runway_operations(skirmish_->base(0)));
        // Keep the fog-of-war memory authoritative: visible contacts refresh their
        // last-known position, while contacts leaving sensor range become stale
        // records and never leak their live position to the presentation.
        for (auto id : simulation.get_entity_list()) {
            rts::FactionId faction;
            if (simulation.get_unit_is_dead(id) || !simulation.get_unit_faction_id(id, faction) || faction == rts::FactionId::ELITE_PRECISION) continue;
            if (simulation.is_visible_to(rts::FactionId::ELITE_PRECISION, id)) {
                logistics.update_intelligence(id, simulation.get_unit_x(id), simulation.get_unit_y(id), simulation.simulation_tick());
            } else if (logistics.get_intelligence(id) && logistics.get_intelligence(id)->currently_observed) {
                logistics.archive_intelligence(id);
            }
        }
        Array intelligence;
        for (const auto& intel : logistics.intelligence_snapshot()) {
            Dictionary record;
            record["id"] = intel.entity_id;
            record["x"] = intel.last_x;
            record["y"] = intel.last_y;
            record["last_seen_tick"] = static_cast<int64_t>(intel.last_seen_tick);
            record["age"] = static_cast<int64_t>(simulation.simulation_tick() - intel.last_seen_tick);
            record["freshness"] = intel.freshness;
            record["currently_observed"] = intel.currently_observed;
            record["stale"] = !intel.currently_observed && (simulation.simulation_tick() - intel.last_seen_tick) > 120;
            intelligence.append(record);
        }
        state["intelligence"] = intelligence;
        Array units;
        for (auto id : simulation.get_entity_list()) {
            rts::FactionId faction;
            if (simulation.get_unit_is_dead(id) || !simulation.get_unit_faction_id(id,faction)) continue;
            if (faction != rts::FactionId::ELITE_PRECISION && !simulation.is_visible_to(rts::FactionId::ELITE_PRECISION,id)) continue;
            Dictionary unit;
            unit["id"]=id; unit["faction"]=static_cast<int>(faction);
            unit["x"]=simulation.get_unit_x(id); unit["y"]=simulation.get_unit_y(id);
            float hp=0,max_hp=0; simulation.get_unit_health(id,hp,max_hp);
            unit["health"]=hp; unit["max_health"]=max_hp;
            unit["base"]=id==skirmish_->base(static_cast<int>(faction));
            if (faction != rts::FactionId::ELITE_PRECISION) {
                unit["intel_age"] = ::logistics_get_intelligence_age(static_cast<int>(id));
                unit["intel_stale"] = ::logistics_is_intelligence_stale(static_cast<int>(id));
            }
            unit["kind"]="ground";
            if(auto* aircraft=simulation.component_manager().get_component<rts::Aircraft>(id)) {
                unit["kind"]="air"; unit["fuel"]=aircraft->fuel; unit["max_fuel"]=aircraft->max_fuel;
                unit["safe_return"]=aircraft->safe_return; unit["status"]=static_cast<int>(aircraft->status);
                unit["return_energy"] = aircraft->predicted_return_energy;
                unit["return_material"] = aircraft->predicted_return_material;
                unit["return_time_ms"] = aircraft->predicted_return_time_ms;
            }
            if(auto* vessel=simulation.component_manager().get_component<rts::NavalVessel>(id)) {
                unit["kind"]="sea"; unit["fuel"]=vessel->fuel; unit["max_fuel"]=vessel->max_fuel;
                unit["stranded"]=vessel->is_stranded;
            }
            units.append(unit);
        }
        state["units"]=units;
        Array shots;
        for (const auto& projectile : simulation.combat_manager().projectile_manager().projectiles()) {
            if(!projectile.active || (projectile.faction_id != rts::FactionId::ELITE_PRECISION &&
                !simulation.is_position_visible_to(rts::FactionId::ELITE_PRECISION, projectile.x, projectile.y))) continue;
            Dictionary shot; shot["faction"]=static_cast<int>(projectile.faction_id); shot["x"]=projectile.x; shot["y"]=projectile.y; shots.append(shot);
        }
        state["projectiles"]=shots;
        auto& production=simulation.production_manager();
        const auto base=skirmish_->base(0);
        const auto funds=production.storages().find(base);
        if(funds!=production.storages().end()) {
            const auto& tactical = funds->second.tactical;
            state["materials"] = tactical.materials.current;
            state["materials_capacity"] = tactical.materials.max;
            state["fuel"] = tactical.fuel.current;
            state["fuel_capacity"] = tactical.fuel.capacity;
            state["power_generation_mw"] = tactical.power.generation_mw;
            state["power_demand_mw"] = tactical.power.demand_mw;
            state["power_reserve_mw"] = tactical.power.reserve_mw();
            state["power_deficit"] = tactical.power.in_deficit();
            float rates[3]{};
            for (const auto& [id, extractor] : production.extractors()) {
                const auto node=production.resource_nodes().find(extractor.resource_node_id);
                if(extractor.storage_id==base && extractor.active && node!=production.resource_nodes().end() && !node->second.depleted)
                    rates[static_cast<int>(node->second.type)] += extractor.extraction_rate * 20;
            }
            state["material_income"] = rates[0]; state["fuel_income"] = rates[1];
        }
        Array queue;
        const auto line=production.production_lines().find(base);
        if(line!=production.production_lines().end()) {
            auto pending=line->second.queue;
            while(!pending.empty()) {
                Dictionary entry; entry["name"]=String(rts::get_unit_prototypes().at(pending.front().unit_type).name.c_str());
                entry["progress"]=pending.front().build_progress; queue.append(entry); pending.pop();
            }
        }
        state["queue"]=queue;
        Array resources;
        for (const auto& [id, node] : production.resource_nodes()) {
            Dictionary resource;
            resource["id"] = id;
            resource["x"] = node.x;
            resource["y"] = node.y;
            resource["amount"] = node.amount;
            resource["type"] = static_cast<int>(node.type);
            resource["depleted"] = node.depleted;
            resources.append(resource);
        }
        state["resources"] = resources;
        return state;
    }

    Dictionary skirmish_stats_summary() {
        Dictionary summary;
        if (!stats_) {
            const auto path = ProjectSettings::get_singleton()->globalize_path("user://matches");
            stats_ = std::make_unique<rts::StatsManager>(path.utf8().get_data());
        }
        const auto value = stats_->get_summary();
        summary["total_matches"] = static_cast<int64_t>(value.total_matches);
        summary["total_wins"] = static_cast<int64_t>(value.total_wins);
        summary["total_losses"] = static_cast<int64_t>(value.total_losses);
        summary["average_duration_ticks"] = value.average_match_duration_ticks;
        return summary;
    }

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

    void configure_world_size(double width, double height) const {
        rts::runtime_simulation()->configure_world_size(
            static_cast<float>(width), static_cast<float>(height));
    }

    void configure_theater_landmasses(double first_center_x, double first_center_y, double first_width, double first_height,
                                      double second_center_x, double second_center_y, double second_width, double second_height) const {
        rts::runtime_simulation()->configure_theater_landmasses(
            static_cast<float>(first_center_x), static_cast<float>(first_center_y), static_cast<float>(first_width), static_cast<float>(first_height),
            static_cast<float>(second_center_x), static_cast<float>(second_center_y), static_cast<float>(second_width), static_cast<float>(second_height));
    }

    void skirmish_set_ai_enabled(bool enabled) {
        rts::runtime_simulation()->enable_ai(enabled);
    }

    bool load_terrain_heightmap(const String& path) const {
        String resolved_path = path;
        if (path.begins_with("res://")) {
            resolved_path = ProjectSettings::get_singleton()->globalize_path(path);
        }
        try {
            rts::runtime_simulation()->terrain().load_from_file(resolved_path.utf8().get_data());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool is_land_position(double x, double y) const {
        return rts::runtime_simulation()->is_land_position(static_cast<float>(x), static_cast<float>(y));
    }

    void block_civilian_area(double x, double y, double radius) const {
        rts::runtime_simulation()->block_civilian_area(
            static_cast<float>(x), static_cast<float>(y), static_cast<float>(radius));
    }

    void update_simulation(double delta_ms) const {
        if (started_) {
            simulation_update(static_cast<float>(delta_ms));
        }
    }

    int64_t create_unit(double x, double y) const {
        return simulation_create_unit(static_cast<float>(x), static_cast<float>(y));
    }

    int64_t create_faction_base(int64_t faction_id, double x, double y) const {
        if (faction_id < 0 || faction_id > 2) return -1;
        const auto id = rts::runtime_simulation()->create_faction_base(
            static_cast<rts::FactionId>(faction_id), static_cast<float>(x), static_cast<float>(y));
        return id == rts::INVALID_ENTITY ? -1 : static_cast<int64_t>(id);
    }

    Array get_build_catalog(int64_t faction_id) const {
        Array catalog;
        if (faction_id < 0 || faction_id > 2) return catalog;
        const auto faction = static_cast<rts::FactionId>(faction_id);
        const auto& production = rts::runtime_simulation()->production_manager();
        const auto line = production.faction_line(faction);
        for (const auto& [type, prototype] : rts::get_unit_prototypes()) {
            if (prototype.faction != faction || prototype.cargo_transport) continue;
            Dictionary entry;
            entry["type"] = static_cast<int>(type);
            entry["name"] = String(prototype.name.c_str());
            entry["material"] = production.get_unit_metal_cost(faction, type);
            entry["build_seconds"] = prototype.build_time_seconds;
            entry["is_aircraft"] = prototype.is_aircraft;
            const bool airfield_ready = !prototype.is_aircraft || !prototype.requires_runway || prototype.recon_loiter_seconds > 0.0f ||
                rts::runtime_simulation()->territorial_control_manager().has_active_installation(faction, rts::InstallationType::AIRFIELD);
            const auto& simulation = *rts::runtime_simulation();
            const bool delivered = simulation.needs_delivery(type);
            const std::string refusal = delivered ? simulation.delivery_refusal(faction, type) : std::string();
            entry["available"] = line != rts::INVALID_ENTITY && airfield_ready && production.can_queue_unit(line, faction, type) && refusal.empty();
            entry["delivered"] = delivered;
            entry["reason"] = String(refusal.c_str());
            if (delivered) {
                entry["road_reason"] = String(simulation.delivery_refusal_via(faction, type,
                    rts::Simulation::GroundTransport::ROAD).c_str());
                entry["rail_reason"] = String(simulation.delivery_refusal_via(faction, type,
                    rts::Simulation::GroundTransport::RAIL).c_str());
            }
            entry["airdrop"] = delivered && prototype.airdrop;
            entry["is_structure"] = false;
            entry["nato_symbol_id"] = String(prototype.nato_symbol.c_str());
            catalog.append(entry);
        }
        for (uint8_t structure = 0; structure < 4; ++structure) {
            Dictionary entry;
            entry["type"] = 100 + static_cast<int>(structure);
            entry["structure_type"] = static_cast<int>(structure);
            entry["is_structure"] = true;
            entry["name"] = structure == 0 ? String("FORWARD OUTPOST") : structure == 1 ? String("RADAR MAST") : structure == 2 ? String("AIRFIELD") : String("FLOODLIGHT");
            entry["material"] = structure == 0 ? 300.0 : structure == 1 ? 450.0 : structure == 2 ? 600.0 : 380.0;
            entry["build_seconds"] = structure == 0 ? 20.0 : structure == 1 ? 28.0 : structure == 2 ? 36.0 : 24.0;
            entry["available"] = line != rts::INVALID_ENTITY && production.can_queue_structure(line, faction, structure);
            entry["icon_id"] = structure == 0 ? String("construction.fortification") : structure == 1 ? String("intel.radar") : structure == 2 ? String("infrastructure.airbase") : String("intel.sensors");
            catalog.append(entry);
        }
        return catalog;
    }

    bool queue_structure(int64_t line_id, int64_t faction_id, int64_t structure_type, double x, double y) const {
        if (line_id <= 0 || faction_id < 0 || faction_id > 2 || structure_type < 0 || structure_type > 3) return false;
        return rts::runtime_simulation()->queue_structure(
            static_cast<rts::EntityId>(line_id), static_cast<rts::FactionId>(faction_id), static_cast<uint8_t>(structure_type), static_cast<float>(x), static_cast<float>(y));
    }

    Dictionary get_structure_site(int64_t structure_type, double x, double y) const {
        Dictionary result;
        if (structure_type < 0 || structure_type > 3) return result;
        const auto site = rts::runtime_simulation()->assess_structure_site(
            static_cast<uint8_t>(structure_type), static_cast<float>(x), static_cast<float>(y));
        result["buildable"] = site.buildable;
        result["claimed"] = rts::runtime_simulation()->structure_site_claimed(
            static_cast<uint8_t>(structure_type), static_cast<float>(x), static_cast<float>(y));
        result["earthworks"] = site.earthworks;
        result["material"] = site.material;
        result["fuel"] = site.fuel;
        result["seconds"] = site.seconds;
        // Half-extents of ground that stays walkable around the finished
        // structure: its footprint, the navigation cells blocking rounds out
        // to, and one clear cell for the builder to stand on.
        const auto rules = rts::structure_site_rules(static_cast<uint8_t>(structure_type));
        const float cell = rts::runtime_simulation()->pathfinding().cell_size();
        result["clearance_x"] = rules.footprint_x * 0.5f + cell * 1.5f;
        result["clearance_y"] = rules.footprint_y * 0.5f + cell * 1.5f;
        result["footprint_x"] = rules.footprint_x;
        result["footprint_y"] = rules.footprint_y;
        result["claim_margin"] = cell;
        return result;
    }

    int64_t get_terrain_revision() const {
        return rts::runtime_simulation()->terrain().revision();
    }

    PackedFloat32Array get_terrain_heights() const {
        const auto& heights = rts::runtime_simulation()->terrain().data();
        PackedFloat32Array result;
        result.resize(static_cast<int64_t>(heights.size()));
        std::copy(heights.begin(), heights.end(), result.ptrw());
        return result;
    }

    bool queue_faction_structure(int64_t faction_id, int64_t structure_type, double x, double y) const {
        record_action("queue_faction_structure", {static_cast<double>(faction_id), static_cast<double>(structure_type), x, y});
        return queue_structure(get_faction_production_line(faction_id), faction_id, structure_type, x, y);
    }

    bool queue_faction_unit(int64_t faction_id, int64_t unit_type, double x, double y) const {
        auto line_id = get_faction_production_line(faction_id);
        if (line_id <= 0) return false;
        if (unit_type < 0 || unit_type > 255) return false;
        record_action("queue_faction_unit", {static_cast<double>(faction_id), static_cast<double>(unit_type), x, y});
        return rts::runtime_simulation()->order_unit(
            static_cast<rts::FactionId>(faction_id), static_cast<rts::UnitType>(unit_type), static_cast<float>(x), static_cast<float>(y));
    }

    bool validate_structure_placement(int64_t structure_type, double x, double y) const {
        if (structure_type < 0 || structure_type > 3) return false;
        return rts::runtime_simulation()->validate_structure_placement(
            static_cast<uint8_t>(structure_type), static_cast<float>(x), static_cast<float>(y));
    }

    bool validate_engineer_placement(double x, double y) const {
        return rts::runtime_simulation()->validate_engineer_placement(
            static_cast<float>(x), static_cast<float>(y));
    }

    bool validate_road_placement(double start_x, double start_y, double end_x, double end_y) const {
        return rts::runtime_simulation()->validate_road_placement(
            static_cast<float>(start_x), static_cast<float>(start_y),
            static_cast<float>(end_x), static_cast<float>(end_y));
    }

    bool queue_road(int64_t engineer_id, int64_t faction_id, double start_x, double start_y,
                    double end_x, double end_y) const {
        if (engineer_id <= 0 || faction_id < 0 || faction_id > 2) return false;
        record_action("queue_road", {static_cast<double>(engineer_id), static_cast<double>(faction_id), start_x, start_y, end_x, end_y});
        return rts::runtime_simulation()->queue_road(
            static_cast<rts::EntityId>(engineer_id), static_cast<rts::FactionId>(faction_id),
            static_cast<float>(start_x), static_cast<float>(start_y),
            static_cast<float>(end_x), static_cast<float>(end_y));
    }

    PackedFloat32Array get_road_segments() const {
        PackedFloat32Array segments;
        for (const auto& segment : rts::runtime_simulation()->road_network().segments()) {
            if (!segment.completed) continue;
            segments.append(static_cast<float>(segment.id));
            segments.append(segment.start_x);
            segments.append(segment.start_y);
            segments.append(segment.end_x);
            segments.append(segment.end_y);
            segments.append(segment.width);
        }
        return segments;
    }

    int64_t get_faction_production_line(int64_t faction_id) const {
        if (faction_id < 0 || faction_id > 2) return -1;
        return static_cast<int64_t>(rts::runtime_simulation()->production_manager().faction_line(static_cast<rts::FactionId>(faction_id)));
    }

    Array get_production_queue(int64_t line_id) const {
        Array queue;
        if (line_id <= 0) return queue;
        const auto& production = rts::runtime_simulation()->production_manager();
        const auto line = production.production_lines().find(static_cast<rts::EntityId>(line_id));
        if (line == production.production_lines().end()) return queue;
        auto pending = line->second.queue;
        int index = 0;
        while (!pending.empty()) {
            const auto& item = pending.front();
            Dictionary entry;
            entry["index"] = index++;
            entry["type"] = item.type == rts::ConstructionQueueEntry::Type::BUILDING ? 100 + static_cast<int>(item.structure_type) : static_cast<int>(item.unit_type);
            entry["is_structure"] = item.type == rts::ConstructionQueueEntry::Type::BUILDING;
            entry["name"] = item.display_name.empty() ? String(rts::get_unit_prototypes().at(item.unit_type).name.c_str()) : String(item.display_name.c_str());
            entry["progress"] = item.build_progress;
            entry["remaining_seconds"] = std::max(0.0f, item.build_time_seconds * (1.0f - item.build_progress));
            entry["reserved_material"] = item.total_cost_metal;
            entry["reserved_fuel"] = item.total_cost_fuel;
            entry["target_x"] = item.target_x;
            entry["target_y"] = item.target_y;
            queue.append(entry);
            pending.pop();
        }
        return queue;
    }

    Dictionary get_hud_state(int64_t faction_id, int64_t storage_id, int64_t selected_entity_id,
                             double fob_x, double fob_y, bool include_fob) const {
        Dictionary state;
        if (faction_id < 0 || faction_id > 2) return state;

        state["tick_ms"] = get_simulation_tick_ms();
        state["tactical_resources"] = economy_get_tactical_resources(storage_id);
        state["build_catalog"] = get_build_catalog(faction_id);
        state["production_queue"] = get_production_queue(get_faction_production_line(faction_id));
        state["selected_health"] = selected_entity_id > 0 ? get_unit_health(selected_entity_id) : Array();
        state["selected_off_road"] = selected_entity_id > 0 ? get_unit_off_road_state(selected_entity_id) : PackedFloat32Array();
        state["fob_installation"] = include_fob ? territory_get_installation_info(fob_x, fob_y) : Array();
        return state;
    }

    bool destroy_resource_site(int64_t entity_id, double x, double y) const {
        if (entity_id <= 0) return false;
        record_action("destroy_resource_site", {static_cast<double>(entity_id), x, y});
        return rts::runtime_simulation()->destroy_resource_site(
            static_cast<rts::EntityId>(entity_id), static_cast<float>(x), static_cast<float>(y));
    }

    PackedInt32Array get_entity_ids() const {
        PackedInt32Array ids;
        const auto entities = rts::runtime_simulation()->get_entity_list();
        ids.resize(static_cast<int>(entities.size()));
        for (int index = 0; index < ids.size(); ++index) ids[index] = static_cast<int32_t>(entities[index]);
        return ids;
    }

    // [tick, fraction]: the last completed simulation tick and how far the
    // clock has run toward the next one (0 to 1), for frame interpolation.
    bool set_game_speed(double scale) const {
        return rts::runtime_simulation()->set_game_speed(static_cast<float>(scale));
    }
    double get_game_speed() const { return rts::runtime_simulation()->game_speed(); }
    bool is_multiplayer_session() const {
        return rts::runtime_simulation()->network_manager().is_multiplayer_session();
    }

    PackedFloat64Array get_tick_clock() const {
        PackedFloat64Array clock;
        clock.resize(2);
        const auto* simulation = rts::runtime_simulation();
        clock.set(0, simulation ? static_cast<double>(simulation->simulation_tick()) : 0.0);
        clock.set(1, simulation ? std::clamp(static_cast<double>(simulation->tick_fraction()), 0.0, 1.0) : 0.0);
        return clock;
    }

    void forces_by_transport_set(bool enabled) const {
        record_action("forces_by_transport_set", {enabled ? 1.0 : 0.0});
        rts::runtime_simulation()->set_forces_by_transport(enabled);
    }

    bool configure_transport_route(int64_t faction_id, int64_t mode, double entry_x, double entry_y,
                                   double terminal_x, double terminal_y) const {
        if (faction_id < 0 || faction_id >= 3 || mode < 1 || mode > 2) return false;
        const bool accepted = rts::runtime_simulation()->configure_transport_route(
            static_cast<rts::FactionId>(faction_id), static_cast<rts::Simulation::GroundTransport>(mode),
            static_cast<float>(entry_x), static_cast<float>(entry_y),
            static_cast<float>(terminal_x), static_cast<float>(terminal_y));
        if (accepted) record_action("configure_transport_route", {static_cast<double>(faction_id), static_cast<double>(mode),
            entry_x, entry_y, terminal_x, terminal_y});
        return accepted;
    }

    Dictionary transport_route(int64_t faction_id, int64_t mode) const {
        Dictionary result;
        if (faction_id < 0 || faction_id >= 3 || mode < 1 || mode > 2) return result;
        const auto route = rts::runtime_simulation()->transport_route(
            static_cast<rts::FactionId>(faction_id), static_cast<rts::Simulation::GroundTransport>(mode));
        result["enabled"] = route.enabled;
        result["entry"] = Vector2(route.entry_x, route.entry_y);
        result["terminal"] = Vector2(route.terminal_x, route.terminal_y);
        return result;
    }

    bool order_ground_transport(int64_t faction_id, int64_t unit_type, int64_t mode) const {
        if (faction_id < 0 || faction_id >= 3 || unit_type < 0 || unit_type > 255 || mode < 1 || mode > 2) return false;
        record_action("order_ground_transport", {static_cast<double>(faction_id), static_cast<double>(unit_type), static_cast<double>(mode)});
        return rts::runtime_simulation()->issue_ground_transport_command(
            static_cast<rts::FactionId>(faction_id), static_cast<rts::UnitType>(unit_type),
            static_cast<rts::Simulation::GroundTransport>(mode)) == 1;
    }

    // Every Materials site's true holder, for the presentation to remember;
    // the map shows only the holder it had when last seen.
    Array material_sites_state(int64_t player_id) const {
        Array sites;
        for (const auto& status : rts::runtime_simulation()->material_sites()) {
            Dictionary site;
            site["id"] = static_cast<int64_t>(status.id);
            site["owner"] = status.owner;
            site["capturing"] = status.capturing && status.capturing_faction == static_cast<int>(player_id);
            sites.append(site);
        }
        return sites;
    }

    void ground_fuel_burn_set(bool enabled) const {
        record_action("ground_fuel_burn_set", {enabled ? 1.0 : 0.0});
        rts::runtime_simulation()->set_ground_fuel_burn(enabled);
    }

    bool landing_zone_set(int64_t faction_id, double x, double y) const {
        if (faction_id < 0 || faction_id >= 3) return false;
        record_action("landing_zone_set", {static_cast<double>(faction_id), x, y});
        return rts::runtime_simulation()->set_landing_zone(static_cast<rts::FactionId>(faction_id),
            static_cast<float>(x), static_cast<float>(y));
    }

    // Empty when this unit can be dropped at this point now, else why not.
    String paradrop_refusal(int64_t faction_id, int64_t unit_type, double x, double y) const {
        if (faction_id < 0 || faction_id > 2 || unit_type < 0 || unit_type > 255) return String("UNKNOWN UNIT");
        return String(rts::runtime_simulation()->paradrop_refusal(static_cast<rts::FactionId>(faction_id),
            static_cast<rts::UnitType>(unit_type), static_cast<float>(x), static_cast<float>(y)).c_str());
    }

    // Orders a paratroop drop as a recorded command, which runs on the next
    // tick. Answers at once with the refusal, or "" when the order was sent.
    String order_paradrop(int64_t faction_id, int64_t unit_type, double x, double y) const {
        const String refusal = paradrop_refusal(faction_id, unit_type, x, y);
        if (!refusal.is_empty()) return refusal;
        if (rts::runtime_simulation()->issue_paradrop_command(static_cast<rts::FactionId>(faction_id),
                static_cast<rts::UnitType>(unit_type), static_cast<float>(x), static_cast<float>(y)) == 0) {
            return String("ORDER NOT SENT");
        }
        return String();
    }

    // Other sides' finished airfields that this player can see right now, as
    // x, y pairs. The presentation remembers them once seen, as it does depots.
    PackedFloat32Array enemy_airfields_in_sight(int64_t player_id) const {
        PackedFloat32Array result;
        if (player_id < 0 || player_id > 2) return result;
        const auto player = static_cast<rts::FactionId>(player_id);
        const auto& simulation = *rts::runtime_simulation();
        const float half_length = rts::structure_site_rules(2).footprint_y * 0.5f;
        for (const auto& installation : rts::runtime_simulation()->territorial_control_manager().active_installations()) {
            if (installation.faction == player || installation.type != rts::InstallationType::AIRFIELD) continue;
            bool seen = false;
            for (const float along : {0.0f, -half_length, half_length}) {
                seen = seen || simulation.can_see(player, installation.x, installation.y + along);
            }
            if (!seen) continue;
            result.append(installation.x);
            result.append(installation.y);
        }
        return result;
    }

    // The transport rule for one faction: whether it is on, where forces land,
    // and every force still on its way, soonest first.
    Dictionary delivery_state(int64_t faction_id) const {
        Dictionary state;
        if (faction_id < 0 || faction_id >= 3) return state;
        const auto faction = static_cast<rts::FactionId>(faction_id);
        const auto& simulation = *rts::runtime_simulation();
        state["enabled"] = simulation.forces_by_transport();
        float x = 0.0f, y = 0.0f;
        const bool site = simulation.landing_site(faction, x, y);
        state["has_site"] = site;
        if (site) {
            state["x"] = x;
            state["y"] = y;
        }
        Array inbound;
        std::vector<rts::Simulation::InboundForce> own;
        for (const auto& force : simulation.inbound_forces()) {
            if (force.faction == faction) own.push_back(force);
        }
        std::sort(own.begin(), own.end(), [](const auto& a, const auto& b) { return a.arrive_tick < b.arrive_tick; });
        for (const auto& force : own) {
            Dictionary entry;
            entry["type"] = static_cast<int>(force.type);
            entry["route_mode"] = static_cast<int>(force.route_mode);
            entry["order_id"] = static_cast<int64_t>(force.order_id);
            entry["airdrop"] = force.airdrop;
            entry["released"] = force.released;
            if (force.airdrop) {
                entry["x"] = force.drop_x;
                entry["y"] = force.drop_y;
            } else if (force.route_mode == 1 || force.route_mode == 2) {
                const auto route = simulation.transport_route(faction,
                    static_cast<rts::Simulation::GroundTransport>(force.route_mode));
                entry["entry"] = Vector2(route.entry_x, route.entry_y);
                entry["terminal"] = Vector2(route.terminal_x, route.terminal_y);
                const auto elapsed = simulation.simulation_tick() > force.launch_tick
                    ? simulation.simulation_tick() - force.launch_tick : 0;
                const auto duration = force.arrive_tick > force.launch_tick
                    ? force.arrive_tick - force.launch_tick : 1;
                entry["progress"] = std::clamp(static_cast<double>(elapsed) / static_cast<double>(duration), 0.0, 1.0);
                entry["in_transit"] = simulation.simulation_tick() >= force.launch_tick;
            }
            entry["remaining_ms"] = force.arrive_tick > simulation.simulation_tick()
                ? static_cast<double>(force.arrive_tick - simulation.simulation_tick()) * 50.0 : 0.0;
            inbound.append(entry);
        }
        state["inbound"] = inbound;
        state["airdrops_lost"] = simulation.paradrops_lost(faction);
        return state;
    }

    // Presentation-only native approach altitude (AGL), pitch and bank. A -1
    // altitude means the unit uses the normal airborne/ground presentation.
    PackedFloat32Array get_aircraft_approach_poses(const PackedInt32Array& entity_ids) const {
        PackedFloat32Array poses;
        poses.resize(entity_ids.size() * 3);
        const auto& simulation = *rts::runtime_simulation();
        for (int index = 0; index < entity_ids.size(); ++index) {
            float altitude = -1.0f, pitch = 0.0f, bank = 0.0f;
            simulation.get_aircraft_approach_pose(static_cast<rts::EntityId>(entity_ids[index]), altitude, pitch, bank);
            poses.set(index*3, altitude);
            poses.set(index*3+1, pitch);
            poses.set(index*3+2, bank);
        }
        return poses;
    }

    PackedFloat32Array get_unit_range_profile(int64_t entity_id) const {
        PackedFloat32Array ranges;
        ranges.resize(3);
        auto& sim = *rts::runtime_simulation();
        const auto* data = sim.component_manager().get_component<rts::UnitData>(static_cast<rts::EntityId>(entity_id));
        const auto* weapon = sim.component_manager().get_component<rts::Weapon>(static_cast<rts::EntityId>(entity_id));
        ranges.set(0, weapon && weapon->damage > 0 ? weapon->range : 0.0f);
        ranges.set(1, data ? data->view_range : 0.0f);
        ranges.set(2, data ? data->intel_range : 0.0f);
        return ranges;
    }

    PackedByteArray get_airborne_flags(const PackedInt32Array& entity_ids) const {
        PackedByteArray flags;
        flags.resize(entity_ids.size());
        auto& simulation = *rts::runtime_simulation();
        for (int index = 0; index < entity_ids.size(); ++index) {
            const auto* aircraft = simulation.component_manager().get_component<rts::Aircraft>(
                static_cast<rts::EntityId>(entity_ids[index]));
            const bool airborne = aircraft && aircraft->status != rts::Aircraft::Status::ON_GROUND &&
                aircraft->status != rts::Aircraft::Status::QUEUED_FOR_TAKEOFF &&
                aircraft->status != rts::Aircraft::Status::CRASHED;
            flags.set(index, airborne ? 1 : 0);
        }
        return flags;
    }

    // Presented units whose entity is gone, such as a recon plane that left the map.
    PackedInt32Array get_removed_entities(const PackedInt32Array& entity_ids) const {
        PackedInt32Array removed;
        auto& simulation = *rts::runtime_simulation();
        std::unordered_set<int32_t> live;
        for (const auto entity_id : simulation.get_entity_list()) live.insert(static_cast<int32_t>(entity_id));
        for (int index = 0; index < entity_ids.size(); ++index) {
            if (!live.contains(entity_ids[index])) removed.append(entity_ids[index]);
        }
        return removed;
    }

    Array get_unpresented_entities(const PackedInt32Array& known_entity_ids) const {
        std::unordered_set<int32_t> known;
        known.reserve(static_cast<size_t>(known_entity_ids.size()));
        for (int index = 0; index < known_entity_ids.size(); ++index) {
            if (known_entity_ids[index] > 0) known.insert(known_entity_ids[index]);
        }

        Array entities;
        auto& simulation = *rts::runtime_simulation();
        for (const auto entity_id : simulation.get_entity_list()) {
            const auto id = static_cast<int32_t>(entity_id);
            if (id <= 0 || known.contains(id)) continue;
            rts::FactionId faction_id;
            if (!simulation.get_unit_faction_id(entity_id, faction_id)) continue;
            Dictionary entity;
            entity["id"] = id;
            entity["faction_id"] = static_cast<int>(faction_id);
            const auto* data = simulation.component_manager().get_component<rts::UnitData>(entity_id);
            entity["unit_type"] = data ? data->unit_type : -1;
            entity["x"] = simulation.get_unit_x(entity_id);
            entity["y"] = simulation.get_unit_y(entity_id);
            entities.append(entity);
        }
        return entities;
    }

    void move_unit(int64_t entity_id, double x, double y) const {
        simulation_move_unit(static_cast<int>(entity_id), static_cast<float>(x), static_cast<float>(y));
    }

    void move_units_formation(
        const PackedInt32Array& entity_ids,
        double center_x,
        double center_y,
        double spacing) const {
        std::vector<double> args{center_x, center_y, spacing};
        for (int64_t index = 0; index < entity_ids.size(); ++index) args.push_back(entity_ids[index]);
        record_action("move_units_formation", std::move(args));
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
        double spacing,
        bool queued) const {
        if (player_id < 0 || player_id > 2) return 0;
        return simulation_issue_move_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(center_x),
            static_cast<float>(center_y),
            static_cast<float>(spacing),
            queued ? 1 : 0
        );
    }

    int64_t issue_stop_commands(const PackedInt32Array& entity_ids, int64_t player_id) const {
        if (player_id < 0 || player_id > 2) return 0;
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
        if (player_id < 0 || player_id > 2) return 0;
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
        if (player_id < 0 || player_id > 2) return 0;
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
        if (player_id < 0 || player_id > 2) return 0;
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
        if (unit_type < 0 || unit_type > 255 || player_id < 0 || player_id > 2) return 0;
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
        if (player_id < 0 || player_id > 2) return 0;
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
        if (player_id < 0 || player_id > 2) return 0;
        return simulation_issue_defend_commands(
            entity_ids.ptr(),
            entity_ids.size(),
            static_cast<int>(player_id),
            static_cast<float>(x),
            static_cast<float>(y)
        );
    }

    int64_t issue_install_commands(const PackedInt32Array& entity_ids, int64_t player_id,
                                   double x, double y, int64_t installation_type) const {
        if (player_id < 0 || player_id > 2 || installation_type < 0 || installation_type > 255) return 0;
        return simulation_issue_install_commands(entity_ids.ptr(), entity_ids.size(), static_cast<int>(player_id),
                                                  static_cast<float>(x), static_cast<float>(y), static_cast<int>(installation_type));
    }

    int64_t issue_requisition_commands(const PackedInt32Array& entity_ids, int64_t player_id,
                                       double x, double y, int64_t unit_type) const {
        if (player_id < 0 || player_id > 2) return 0;
        return simulation_issue_requisition_commands(entity_ids.ptr(), entity_ids.size(), static_cast<int>(player_id),
                                                      static_cast<float>(x), static_cast<float>(y), static_cast<int>(unit_type));
    }

    void initialize_faction(int64_t faction_id, double x, double y) {
        simulation_initialize_faction(static_cast<int>(faction_id), static_cast<float>(x), static_cast<float>(y));
    }

    int64_t create_unit_with_type(double x, double y, int64_t unit_type, int64_t faction_id) {
        record_action("create_unit_with_type", {x, y, static_cast<double>(unit_type), static_cast<double>(faction_id)});
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

    void economy_add_tactical_storage(int64_t storage_id, double x, double y, double materials_capacity, double fuel_capacity,
                                      double power_generation_mw, double power_demand_mw) {
        ::economy_add_tactical_storage(
            static_cast<int>(storage_id),
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(materials_capacity),
            static_cast<float>(fuel_capacity),
            static_cast<float>(power_generation_mw),
            static_cast<float>(power_demand_mw)
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

    PackedFloat32Array get_unit_position(int64_t entity_id) const {
        PackedFloat32Array position;
        position.resize(2);
        position.set(0, static_cast<float>(get_unit_x(entity_id)));
        position.set(1, static_cast<float>(get_unit_y(entity_id)));
        return position;
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

    PackedFloat32Array get_unit_headings(const PackedInt32Array& entity_ids) const {
        PackedFloat32Array headings;
        headings.resize(entity_ids.size());
        if (simulation_get_unit_headings(entity_ids.ptr(), entity_ids.size(), headings.ptrw(), headings.size()) != entity_ids.size()) {
            headings.clear();
        }
        return headings;
    }

    PackedFloat32Array get_unit_transforms(const PackedInt32Array& entity_ids) const {
        const auto positions = get_unit_positions(entity_ids);
        const auto headings = get_unit_headings(entity_ids);
        if (positions.size() != entity_ids.size() * 2 || headings.size() != entity_ids.size()) {
            return PackedFloat32Array();
        }
        PackedFloat32Array transforms;
        transforms.resize(entity_ids.size() * 3);
        for (int index = 0; index < entity_ids.size(); ++index) {
            transforms.set(index * 3, positions[index * 2]);
            transforms.set(index * 3 + 1, positions[index * 2 + 1]);
            transforms.set(index * 3 + 2, headings[index]);
        }
        return transforms;
    }

    PackedFloat32Array get_unit_steering_state(int64_t entity_id) const {
        PackedFloat32Array state;
        state.resize(3);
        float* values = state.ptrw();
        if (!simulation_get_unit_steering_state(
                static_cast<int>(entity_id), &values[0], &values[1], &values[2])) {
            state.clear();
        }
        return state;
    }

    bool has_unit_move_target(int64_t entity_id) const {
        return simulation_has_unit_move_target(static_cast<int>(entity_id)) != 0;
    }

    // Presentation-only: the remaining route of each of the player's own units
    // as simulation (x, y) points, starting at the unit and ending at its
    // arrival point. The result is parallel to entity_ids; an entry is empty
    // when that unit has no move order or belongs to someone else.
    Array get_unit_routes(const PackedInt32Array& entity_ids, int64_t player_id) const {
        constexpr int ROUTE_CAPACITY = 128;
        constexpr int64_t MAX_ROUTE_QUERIES = 512;
        Array routes;
        const int64_t count = std::min<int64_t>(entity_ids.size(), MAX_ROUTE_QUERIES);
        for (int64_t entry = 0; entry < count; ++entry) {
            float points[ROUTE_CAPACITY * 2];
            const int point_count = simulation_get_unit_route(
                entity_ids[entry], static_cast<int>(player_id), points, ROUTE_CAPACITY);
            PackedVector2Array route;
            route.resize(point_count);
            for (int index = 0; index < point_count; ++index) {
                route.set(index, Vector2(points[index * 2], points[index * 2 + 1]));
            }
            routes.append(route);
        }
        return routes;
    }

    // Per queried unit: its current destination, then each queued waypoint,
    // in driving order. Empty for an idle unit or another player's unit.
    Array get_unit_waypoints(const PackedInt32Array& entity_ids, int64_t player_id) const {
        constexpr int WAYPOINT_CAPACITY = 32;
        constexpr int64_t MAX_WAYPOINT_QUERIES = 512;
        Array result;
        const int64_t count = std::min<int64_t>(entity_ids.size(), MAX_WAYPOINT_QUERIES);
        for (int64_t entry = 0; entry < count; ++entry) {
            float points[WAYPOINT_CAPACITY * 2];
            const int point_count = simulation_get_unit_waypoints(
                entity_ids[entry], static_cast<int>(player_id), points, WAYPOINT_CAPACITY);
            PackedVector2Array waypoints;
            waypoints.resize(point_count);
            for (int index = 0; index < point_count; ++index) {
                waypoints.set(index, Vector2(points[index * 2], points[index * 2 + 1]));
            }
            result.append(waypoints);
        }
        return result;
    }

    PackedFloat32Array get_unit_off_road_state(int64_t entity_id) const {
        PackedFloat32Array state;
        state.resize(3);
        float* values = state.ptrw();
        if (!simulation_get_unit_off_road_state(
                static_cast<int>(entity_id), &values[0], &values[1], &values[2])) {
            state.clear();
        }
        return state;
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
        String resolved_path = path;
        if (path.begins_with("res://")) {
            String relative_path = path.substr(6);
            resolved_path = "res://" + relative_path;
            resolved_path = ProjectSettings::get_singleton()->globalize_path(resolved_path);
        }
        std::filesystem::path fs_path(resolved_path.utf8().get_data());
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
            
            PackedInt32Array landmass_ids;
            PackedFloat32Array landmass_center_x;
            PackedFloat32Array landmass_center_y;
            PackedFloat32Array landmass_size_x;
            PackedFloat32Array landmass_size_y;
            
            for (const auto& biome_entry : map_data.biomes) {
                String key(biome_entry.first.c_str());
                if (key.begins_with("landmass_")) {
                    String num_str = key.substr(9);
                    landmass_ids.append(num_str.to_int());
                }
            }
            
            for (const auto& center : map_data.landmass_centers) {
                landmass_center_x.append(center.first);
                landmass_center_y.append(center.second);
            }
            for (const auto& size : map_data.landmass_sizes) {
                landmass_size_x.append(size.first);
                landmass_size_y.append(size.second);
            }
            
            dict["landmass_ids"] = landmass_ids;
            dict["landmass_center_x"] = landmass_center_x;
            dict["landmass_center_y"] = landmass_center_y;
            dict["landmass_size_x"] = landmass_size_x;
            dict["landmass_size_y"] = landmass_size_y;
            
            Array player_units;
            Array ai_units;
            for (const auto& entity : map_data.initial_entities) {
                Dictionary entity_dict;
                entity_dict["entity_id"] = String(entity.id.c_str());
                entity_dict["type"] = String(entity.type.c_str());
                entity_dict["content_id"] = String(entity.content_id.c_str());
                entity_dict["unit_type"] = entity.unit_type;
                if (String(entity.id.c_str()).begins_with("player_")) {
                    player_units.append(entity_dict);
                } else if (String(entity.id.c_str()).begins_with("ai_")) {
                    ai_units.append(entity_dict);
                }
            }
            dict["player_units"] = player_units;
            dict["ai_units"] = ai_units;
        } else {
            dict["ok"] = false;
            std::string error_msg = "Map load failed";
            const auto& errors = loader.get_errors();
            if (!errors.empty()) {
                error_msg += ": ";
                for (size_t i = 0; i < errors.size(); ++i) {
                    if (i > 0) error_msg += "; ";
                    error_msg += errors[i].message;
                    if (!errors[i].field.empty()) {
                        error_msg += " (field: " + errors[i].field + ")";
                    }
                }
            }
            dict["error"] = String(error_msg.c_str());
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
        if (faction_id < 0 || faction_id > 2) return;
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
    
    Array economy_get_tactical_resources(int64_t storage_id) const {
        float materials, materials_capacity, fuel, fuel_capacity, generation, demand, reserve;
        if (!::economy_get_tactical_resources(static_cast<int>(storage_id), &materials, &materials_capacity,
                &fuel, &fuel_capacity, &generation, &demand, &reserve)) return Array();
        Array info;
        info.append(materials); info.append(materials_capacity);
        info.append(fuel); info.append(fuel_capacity);
        info.append(generation); info.append(demand); info.append(reserve);
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

    String get_unit_display_name(int64_t entity_id) const {
        if (entity_id <= 0 || entity_id > INT32_MAX) return String();
        const auto* data = rts::runtime_simulation()->component_manager()
            .get_component<rts::UnitData>(static_cast<rts::EntityId>(entity_id));
        if (!data) return String();
        const auto& prototypes = rts::get_unit_prototypes();
        const auto it = prototypes.find(static_cast<rts::UnitType>(data->unit_type));
        return it == prototypes.end() ? String() : String(it->second.name.c_str());
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
    int recorded_result_ = -1;
    std::unique_ptr<rts::StatsManager> stats_;
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
