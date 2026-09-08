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
#include "../ecs/components/factions.hpp"
#include "command_manager.hpp"
#include "ai/ai_manager.hpp"

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
    void harvest_resource(EntityId entity, float x, float y);
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
    
    bool get_unit_health(EntityId entity, float& current, float& max);
    bool get_unit_is_dead(EntityId entity);
    bool apply_damage(EntityId entity, float damage);
    bool get_unit_faction_id(EntityId entity, FactionId& faction_id);

    Pathfinding& pathfinding() { return pathfinding_; }
    LogisticsManager& logistics_manager() { return logistics_manager_; }
    CombatManager& combat_manager() { return combat_manager_; }
    SpatialGrid& spatial_grid() { return spatial_grid_; }
    ProductionManager& production_manager() { return production_manager_; }
    NetworkManager& network_manager() { return network_manager_; }
    CommandManager& command_manager() { return command_manager_; }
    ComponentManager& component_manager() { return component_manager_; }
    Renderer& renderer() { return renderer_; }
    AIManager& ai_manager() { return *ai_manager_; }

    void update_logistics(float delta_ms) { logistics_manager_.update_all(delta_ms); }
    void update_economy(float delta_ms) { production_manager_.update_all(delta_ms); }

    void process_commands();
    void process_command_internal(const InputCommand& cmd);

    // Replay control
    bool start_replay(const std::string& path) { return replay_writer_.open(path); }
    bool write_replay_header(const ReplayMetadata& metadata) { return replay_writer_.write_header(metadata); }
    bool write_replay_command(const uint8_t* data, size_t size) { return replay_writer_.write_command(data, size); }
    bool write_replay_snapshot(const uint8_t* data, size_t size) { return replay_writer_.write_snapshot(data, size); }
    void close_replay() { replay_writer_.close(); }
    
    // Write portable snapshot to replay (call after each tick)
    void write_replay_portable_snapshot();

    // Faction initialization
    void initialize_faction(FactionId faction_id, float x, float y);
    int create_unit_with_type(float x, float y, UnitType unit_type, FactionId faction_id);
    void set_unit_faction(EntityId entity, FactionId faction_id);

    // Faction research
    const FactionResearch& get_faction_research(FactionId faction_id) const;
    void set_faction_research(FactionId faction_id, const FactionResearch& research);
    
    uint32_t simulation_tick() const { return tick_; }

private:
    bool running_{false};
    uint32_t tick_{0};
    float elapsed_ms_{0.0f};
    float last_tick_ms_{0.0f};

    EntityManager entity_manager_;
    ComponentManager component_manager_;
    SpatialGrid spatial_grid_{100.0f};
    Pathfinding pathfinding_{320, 320, 1.0f, -160.0f, -160.0f};
    LogisticsManager logistics_manager_;
    CombatManager combat_manager_;
    ProductionManager production_manager_;
    NetworkManager network_manager_;
    CommandManager command_manager_;
    Renderer renderer_;
    ReplayWriter& replay_writer() { return replay_writer_; }
    ReplayWriter replay_writer_;
    std::unordered_map<EntityId, MoveTarget> move_targets_;
    std::unordered_map<FactionId, FactionResearch> faction_research_;
    
    std::unique_ptr<AIManager> ai_manager_;

    void prediction_phase(float delta_ms);
    void combat_phase(float delta_ms);
    void environment_phase(float delta_ms);
    void logistics_phase(float delta_ms);
    void economy_phase(float delta_ms);
};

} // namespace rts
