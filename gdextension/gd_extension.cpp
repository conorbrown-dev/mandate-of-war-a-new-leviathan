#include <cstdint>
#include <filesystem>
#include <optional>

#include <godot_cpp/classes/project_settings.hpp>
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
#include "simulation/skirmish.hpp"

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
int simulation_issue_install_commands(
    const int32_t* entity_ids,
    int entity_count,
    int player_id,
    float x,
    float y,
    int installation_type
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
        ClassDB::bind_method(D_METHOD("skirmish_position_visible", "x", "y"), &RtsExtension::skirmish_position_visible);
        ClassDB::bind_method(D_METHOD("skirmish_state"), &RtsExtension::skirmish_state);
        ClassDB::bind_method(D_METHOD("skirmish_command", "type", "ids", "x", "y", "extra"), &RtsExtension::skirmish_command);
        ClassDB::bind_method(D_METHOD("start_simulation"), &RtsExtension::start_simulation);
        ClassDB::bind_method(D_METHOD("stop_simulation"), &RtsExtension::stop_simulation);
        ClassDB::bind_method(D_METHOD("reset_simulation"), &RtsExtension::reset_simulation);
        ClassDB::bind_method(D_METHOD("configure_world_size", "width", "height"), &RtsExtension::configure_world_size);
        ClassDB::bind_method(D_METHOD("update_simulation", "delta_ms"), &RtsExtension::update_simulation);
        ClassDB::bind_method(D_METHOD("create_unit", "x", "y"), &RtsExtension::create_unit);
        ClassDB::bind_method(D_METHOD("create_faction_base", "faction_id", "x", "y"), &RtsExtension::create_faction_base);
        ClassDB::bind_method(D_METHOD("destroy_resource_site", "entity_id", "x", "y"), &RtsExtension::destroy_resource_site);
        ClassDB::bind_method(D_METHOD("get_entity_ids"), &RtsExtension::get_entity_ids);
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
        ClassDB::bind_method(
            D_METHOD("issue_install_commands", "entity_ids", "player_id", "x", "y", "installation_type"),
            &RtsExtension::issue_install_commands
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
        ClassDB::bind_method(D_METHOD("territory_get_zone_count"), &RtsExtension::territory_get_zone_count);
        ClassDB::bind_method(D_METHOD("territory_get_zone_info", "zone_id"), &RtsExtension::territory_get_zone_info);
        ClassDB::bind_method(D_METHOD("territory_get_installation_info", "x", "y"), &RtsExtension::territory_get_installation_info);
        ClassDB::bind_method(D_METHOD("get_build_catalog", "faction_id"), &RtsExtension::get_build_catalog);
        ClassDB::bind_method(D_METHOD("get_production_queue", "line_id"), &RtsExtension::get_production_queue);
        ClassDB::bind_method(D_METHOD("queue_structure", "line_id", "faction_id", "structure_type", "x", "y"), &RtsExtension::queue_structure);
        ClassDB::bind_method(D_METHOD("get_faction_production_line", "faction_id"), &RtsExtension::get_faction_production_line);
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

public:
    std::unique_ptr<rts::Skirmish> skirmish_;
    String skirmish_save_replay(const String& path) {
        String resolved_path = path;
        if (path.begins_with("res://")) {
            String relative_path = path.substr(6);
            resolved_path = "res://" + relative_path;
            resolved_path = ProjectSettings::get_singleton()->globalize_path(resolved_path);
        }
        if(!skirmish_) return "No match";
        return skirmish_->save_replay(resolved_path.utf8().get_data()) ? String() : String(skirmish_->error().c_str());
    }
    String skirmish_verify_replay(const String& path) {
        String resolved_path = path;
        if (path.begins_with("res://")) {
            String relative_path = path.substr(6);
            resolved_path = "res://" + relative_path;
            resolved_path = ProjectSettings::get_singleton()->globalize_path(resolved_path);
        }
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
        started_ = true;
        return "";
    }
    void skirmish_update(double delta_ms) { if (skirmish_) skirmish_->update(delta_ms); }
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
            unit["kind"]="ground";
            if(auto* aircraft=simulation.component_manager().get_component<rts::Aircraft>(id)) {
                unit["kind"]="air"; unit["fuel"]=aircraft->fuel; unit["max_fuel"]=aircraft->max_fuel;
                unit["safe_return"]=aircraft->safe_return; unit["status"]=static_cast<int>(aircraft->status);
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
            state["material"]=funds->second.metal_storage; state["energy"]=funds->second.energy_storage;
            state["research"]=funds->second.research_storage;
            state["material_capacity"]=funds->second.metal_capacity;
            state["energy_capacity"]=funds->second.energy_capacity;
            state["research_capacity"]=funds->second.research_capacity;
            float rates[3]{};
            for (const auto& [id, extractor] : production.extractors()) {
                const auto node=production.resource_nodes().find(extractor.resource_node_id);
                if(extractor.storage_id==base && extractor.active && node!=production.resource_nodes().end() && !node->second.depleted)
                    rates[static_cast<int>(node->second.type)] += extractor.extraction_rate * 20;
            }
            state["material_income"]=rates[0]; state["energy_income"]=rates[1]; state["research_income"]=rates[2];
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
        Array projects;
        for(uint32_t i=0;i<rts::get_research_projects().size();++i) {
            const auto id=rts::Simulation::research_id(i);
            const auto& project=rts::get_research_projects().at(id);
            bool owned=false;
            for(auto type:project.unlocks) if(rts::get_unit_prototypes().at(type).faction==rts::FactionId::ELITE_PRECISION) owned=true;
            if(!owned) continue;
            Dictionary row; row["index"]=i; row["name"]=String(project.name.c_str());
            row["available"]=production.can_research(rts::FactionId::ELITE_PRECISION,id);
            const auto& research=production.research(rts::FactionId::ELITE_PRECISION);
            const auto completed=research.completed_projects.find(id);
            row["complete"]=completed!=research.completed_projects.end() && completed->second;
            row["active"]=!research.active_queue.empty() && research.active_queue.front()==id;
            row["progress"]=production.research_progress(rts::FactionId::ELITE_PRECISION);
            projects.append(row);
        }
        state["projects"]=projects;
        return state;
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
            if (prototype.faction != faction) continue;
            Dictionary entry;
            entry["type"] = static_cast<int>(type);
            entry["name"] = String(prototype.name.c_str());
            entry["material"] = production.get_unit_metal_cost(faction, type);
            entry["energy"] = production.get_unit_energy_cost(faction, type);
            entry["research"] = production.get_unit_research_cost(faction, type);
            entry["build_seconds"] = prototype.build_time_seconds;
            entry["available"] = line != rts::INVALID_ENTITY && production.can_queue_unit(line, faction, type);
            entry["is_structure"] = false;
            catalog.append(entry);
        }
        for (uint8_t structure = 0; structure < 2; ++structure) {
            Dictionary entry;
            entry["type"] = 100 + static_cast<int>(structure);
            entry["structure_type"] = static_cast<int>(structure);
            entry["is_structure"] = true;
            entry["name"] = structure == 0 ? String("FORWARD OUTPOST") : String("RADAR MAST");
            entry["material"] = structure == 0 ? 300.0 : 450.0;
            entry["energy"] = structure == 0 ? 150.0 : 250.0;
            entry["research"] = structure == 0 ? 0.0 : 100.0;
            entry["build_seconds"] = structure == 0 ? 20.0 : 28.0;
            entry["available"] = line != rts::INVALID_ENTITY && production.can_queue_structure(line, faction, structure);
            catalog.append(entry);
        }
        return catalog;
    }

    bool queue_structure(int64_t line_id, int64_t faction_id, int64_t structure_type, double x, double y) const {
        if (line_id <= 0 || faction_id < 0 || faction_id > 2 || structure_type < 0 || structure_type > 1) return false;
        return rts::runtime_simulation()->production_manager().queue_structure(
            static_cast<rts::EntityId>(line_id), static_cast<rts::FactionId>(faction_id), static_cast<uint8_t>(structure_type), static_cast<float>(x), static_cast<float>(y));
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
            entry["reserved_energy"] = item.total_cost_energy;
            entry["reserved_research"] = item.total_cost_research;
            entry["target_x"] = item.target_x;
            entry["target_y"] = item.target_y;
            queue.append(entry);
            pending.pop();
        }
        return queue;
    }

    bool destroy_resource_site(int64_t entity_id, double x, double y) const {
        if (entity_id <= 0) return false;
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
        if (player_id < 0 || player_id > 2) return 0;
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
