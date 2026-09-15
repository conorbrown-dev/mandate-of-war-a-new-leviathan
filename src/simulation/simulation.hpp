#pragma once

#include <unordered_map>
#include <vector>

#include "spatial/spatial_grid.hpp"
#include "ecs/entity.hpp"
#include "ecs/component_manager.hpp"
#include "network/portable_snapshot.hpp"
#include "pathfinding/pathfinding.hpp"
#include "logistics/logistics.hpp"
#include "combat/combat_manager.hpp"
#include "production/production_manager.hpp"
#include "network/network_manager.hpp"
#include "replay/replay_writer.hpp"
#include "render/renderer.hpp"
#include "terrain.hpp"
#include "road_network.hpp"
#include "off_road.hpp"
#include "../ecs/components/off_road.hpp"
#include "../ecs/components/factions.hpp"
#include "../ecs/components/harvester.hpp"
#include "../ecs/components/territorial_control.hpp"
#include "command_manager.hpp"
#include "control_point_battle.hpp"
#include "reinforcement_delivery.hpp"
#include "ai/ai_manager.hpp"
#include "production/requisition_manager.hpp"

namespace rts {

struct SimulationState {
    uint32_t tick_number;
    float timestamp_ms;
    std::vector<EntityId> entity_ids;
    std::vector<float> positions_x;
    std::vector<float> positions_y;
    std::vector<float> velocities_x;
    std::vector<float> velocities_y;
    std::vector<float> health_current;
    std::vector<float> health_max;
    std::vector<uint8_t> is_dead;
};

struct MoveTarget {
    Position arrival;
    Position strategic_route;
};

class Simulation {
public:
    Simulation();

    void start();
    void stop();
    void reset();
    void clear_entities();
    void update(float delta_ms);

    SimulationState get_state() const;

    Entity create_unit(float x, float y);
    void move_unit(EntityId entity, float x, float y);
    void move_unit_with_route(EntityId entity, float x, float y, float route_x, float route_y);
    void move_units_formation(
        const std::vector<EntityId>& entities,
        float center_x,
        float center_y,
        float spacing
    );
    void stop_unit(EntityId entity);
    void attack_unit(EntityId entity, EntityId target_id);
    void destroy_unit(EntityId entity);
    void patrol_unit(EntityId entity, float x, float y);
    void return_unit(EntityId entity);
    void build_structure(EntityId entity, float x, float y, UnitType unit_type);
    void install_fob(EntityId entity, float x, float y, InstallationType installation_type);
    void requisition_unit(EntityId entity, float x, float y, UnitType unit_type);
    void harvest_resource(EntityId entity, float x, float y);
    bool destroy_resource_site(EntityId entity, float x, float y);
    void defend_area(EntityId entity, float x, float y);
    
    size_t issue_move_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float center_x,
        float center_y,
        float spacing
    );
    size_t issue_stop_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id
    );
    size_t issue_attack_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        EntityId target_id
    );
    size_t issue_patrol_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float patrol_x,
        float patrol_y
    );
    size_t issue_return_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id
    );
    size_t issue_build_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float build_x,
        float build_y,
        int64_t unit_type
    );
    size_t issue_harvest_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float harvest_x,
        float harvest_y
    );
    size_t issue_defend_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float defend_x,
        float defend_y
    );
    size_t issue_install_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float install_x,
        float install_y,
        InstallationType installation_type
    );
    size_t issue_requisition_commands(
        const std::vector<EntityId>& entities,
        FactionId player_id,
        float requisition_x,
        float requisition_y,
        UnitType unit_type
    );

    // Rendering
    void render_add_unit(float x, float y, uint32_t unit_type);
    void render_update();
    int render_get_instance_count() const { return renderer_.get_instance_count(); }

    size_t entity_count() const { return entity_manager_.entity_count(); }
    std::vector<EntityId> get_entity_list() const { return entity_manager_.get_entities(); }
    float last_tick_ms() const { return last_tick_ms_; }

    bool get_unit_position(EntityId entity, float& x, float& y);
    float get_unit_x(EntityId entity) const;
    float get_unit_y(EntityId entity) const;
    float get_unit_heading(EntityId entity) const;
    bool get_unit_off_road_state(EntityId entity, float& wear, float& distance, float& speed_multiplier) const;
    bool get_unit_steering_state(EntityId entity, float& heading, float& desired_heading, float& speed) const;
    
    bool get_unit_health(EntityId entity, float& current, float& max);
    bool get_unit_is_dead(EntityId entity);
    bool apply_damage(EntityId entity, float damage);
    bool get_unit_faction_id(EntityId entity, FactionId& faction_id);

    Pathfinding& pathfinding() { return pathfinding_; }
    Pathfinding& naval_pathfinding() { return naval_pathfinding_; }
    Pathfinding& navigation_for(EntityId entity);
    const Pathfinding& navigation_for(EntityId entity) const;
     LogisticsManager& logistics_manager() { return logistics_manager_; }
     CombatManager& combat_manager() { return combat_manager_; }
     SpatialGrid& spatial_grid() { return spatial_grid_; }
     const SpatialGrid& spatial_grid() const { return spatial_grid_; }
     ProductionManager& production_manager() { return production_manager_; }
     RequisitionManager& requisition_manager() { return requisition_manager_; }
     NetworkManager& network_manager() { return network_manager_; }
     CommandManager& command_manager() { return command_manager_; }
     ComponentManager& component_manager() { return component_manager_; }
     Renderer& renderer() { return renderer_; }
     AIManager& ai_manager() { return *ai_manager_; }
     Terrain& terrain() { return terrain_; }
     const Terrain& terrain() const { return terrain_; }
     TerritorialControlManager& territorial_control() { return territorial_control_; }
     const TerritorialControlManager& territorial_control() const { return territorial_control_; }

    void update_logistics(float delta_ms) { logistics_manager_.update_all(delta_ms); }
    void update_economy(float delta_ms) { production_manager_.update_all(delta_ms); }

    void process_commands();
    void process_network_commands();
    void process_command_internal(const InputCommand& cmd);
     bool is_position_visible_to(FactionId faction, float x, float y) const;
     bool is_visible_to(FactionId faction, EntityId target) const;
     EntityId find_nearest_visible_enemy(FactionId faction, float x, float y, float max_range = 100.0f) const;
     bool validate_command(const InputCommand& cmd, uint32_t execution_tick) const;
     size_t submit_commands(const std::vector<InputCommand>& commands);
     size_t issue_commands(const std::vector<EntityId>& entities, FactionId player,
                           CommandType type, float x = 0, float y = 0, uint32_t extra = 0);
      static std::string research_id(uint32_t index);
    TerritorialControlManager& territorial_control_manager() { return territorial_control_; }


    // Replay control
    bool start_replay(const std::string& path) { return replay_writer_.open(path); }
    bool write_replay_header(const ReplayMetadata& metadata) { return replay_writer_.write_header(metadata); }
    bool write_replay_command(const uint8_t* data, size_t size) { return replay_writer_.write_command(data, size); }
    bool write_replay_snapshot(const uint8_t* data, size_t size) { return replay_writer_.write_snapshot(data, size); }
    void close_replay() { replay_writer_.close(); }
    
    // Write portable snapshot to replay (call after each tick)
    void write_replay_portable_snapshot();

    // Scenario setup may select a larger theater before a match starts. Keep
    // the default 320x320 world for existing simulations and tests.
    void configure_world_size(float width, float height);
    void configure_theater_landmasses(
        float first_center_x, float first_center_y, float first_width, float first_height,
        float second_center_x, float second_center_y, float second_width, float second_height);
    bool is_land_position(float x, float y) const;
    void block_civilian_area(float x, float y, float radius);
    bool validate_structure_placement(uint8_t structure_type, float x, float y) const;
    bool validate_engineer_placement(float x, float y) const;
    bool validate_road_placement(float start_x, float start_y, float end_x, float end_y) const;
    bool queue_road(EntityId engineer, FactionId owner, float start_x, float start_y, float end_x, float end_y);
    const RoadNetwork& road_network() const { return road_network_; }
    void enable_theater_water_rules(bool enabled = true) { theater_water_rules_enabled_ = enabled; }

    // Faction initialization
    EntityId create_faction_base(FactionId faction, float x, float y);
    void initialize_faction(FactionId faction_id, float x, float y);
    int create_unit_with_type(float x, float y, UnitType unit_type, FactionId faction_id);
    void set_unit_faction(EntityId entity, FactionId faction_id);

    // Faction research
    const FactionResearch& get_faction_research(FactionId faction_id) const;
    void set_faction_research(FactionId faction_id, const FactionResearch& research);
    
    uint32_t simulation_tick() const { return tick_; }
    void enable_ai(bool enabled) { ai_enabled_ = enabled; }
    const std::vector<InputCommand>& command_log() const { return command_log_; }

    bool begin_control_point_battle(const std::vector<ControlPointDefinition>& points,
                                    FactionId player, FactionId enemy, float hold_duration_ms);
    void reset_control_point_battle() { control_point_battle_.reset(); }
    const ControlPointBattle& control_point_battle() const { return control_point_battle_; }
    bool configure_reinforcement_delivery(float x, float y, float radius, FactionId owner);
    bool select_reinforcement_delivery_zone(FactionId player, float x, float y);
    bool request_reinforcement_delivery(FactionId player);
    void set_reinforcement_resources(FactionId faction, float material, float energy);
    const ReinforcementDelivery& reinforcement_delivery() const { return reinforcement_delivery_; }

private:
    bool running_{false};
    bool ai_enabled_ = true;
    std::vector<InputCommand> command_log_;
    uint32_t tick_{0};
    float elapsed_ms_{0.0f};
    float last_tick_ms_{0.0f};
    float command_position_scale_{INPUT_COMMAND_POSITION_SCALE};
    bool theater_water_rules_enabled_{false};

    EntityManager entity_manager_{false}; // Match references never alias a later spawn.
    ComponentManager component_manager_;
    SpatialGrid spatial_grid_{100.0f};
    Pathfinding pathfinding_{320, 320, 1.0f, -160.0f, -160.0f};
    Pathfinding naval_pathfinding_{320, 320, 1.0f, -160.0f, -160.0f};
    Pathfinding air_pathfinding_{320, 320, 1.0f, -160.0f, -160.0f};
    LogisticsManager logistics_manager_;
    CombatManager combat_manager_;
    ProductionManager production_manager_;
    NetworkManager network_manager_;
    CommandManager command_manager_;
    Renderer renderer_;
    ReplayWriter& replay_writer() { return replay_writer_; }
    ReplayWriter replay_writer_;
    std::unordered_map<EntityId, MoveTarget> move_targets_;
    struct PatrolOrder { Position origin; Position destination; bool returning = false; };
    std::unordered_map<EntityId, PatrolOrder> patrol_orders_;
    std::unordered_map<FactionId, FactionResearch> faction_research_;
    Terrain terrain_;
    RoadNetwork road_network_;
    TerritorialControlManager territorial_control_;
     ControlPointBattle control_point_battle_;
     ReinforcementDelivery reinforcement_delivery_;
     RequisitionManager requisition_manager_;
    
    std::unique_ptr<AIManager> ai_manager_;

    void prediction_phase(float delta_ms);
    void combat_phase(float delta_ms);
    void environment_phase(float delta_ms);
    void logistics_phase(float delta_ms);
    void economy_phase(float delta_ms);
    void update_harvesters(float delta_ms);
    void apply_completed_road(const RoadSegment& segment);
};

Simulation* runtime_simulation();

} // namespace rts
