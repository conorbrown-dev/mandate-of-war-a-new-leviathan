#pragma once
#include <optional>

#include <array>
#include <deque>
#include <map>
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
#include "structure_sites.hpp"
#include "airfield_approach.hpp"
#include "road_network.hpp"
#include "off_road.hpp"
#include "../ecs/components/off_road.hpp"
#include "../ecs/components/factions.hpp"
#include "../ecs/components/harvester.hpp"
#include "../ecs/components/territorial_control.hpp"
#include "command_manager.hpp"
#include "control_point_battle.hpp"
#include "reinforcement_delivery.hpp"
#include "fuel_supply.hpp"
#include "vision_map.hpp"
#include "sighting_log.hpp"
#include "ai/ai_manager.hpp"
#include "simulation/match_record.hpp"
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
    bool set_game_speed(float scale);
    float game_speed() const { return network_manager_.is_multiplayer_session() ? 1.0f : game_speed_; }


    SimulationState get_state() const;
    static constexpr size_t MAX_COMBAT_EVENTS = 2048;
    uint64_t combat_event_cursor() const { return combat_event_sequence_; }
    uint64_t combat_event_cursor(FactionId viewer) const {
        const auto index = static_cast<unsigned>(viewer);
        return index < 3 ? combat_event_viewer_sequences_[index] : 0;
    }
    std::vector<CombatPresentationEvent> combat_events_after(uint64_t cursor, FactionId viewer) const;


    Entity create_unit(float x, float y);
    void move_unit(EntityId entity, float x, float y);
    // Replaces the unit's move order and clears any queued waypoints.
    void move_unit_with_route(EntityId entity, float x, float y, float route_x, float route_y);
    // Appends a waypoint after the unit's current move order and any waypoints
    // already queued. With no current move order the unit starts on it now.
    // Returns false when the target is not walkable or the queue is full.
    bool queue_unit_move_with_route(EntityId entity, float x, float y, float route_x, float route_y);
    void move_units_formation(
        const std::vector<EntityId>& entities,
        float center_x,
        float center_y,
        float spacing,
        bool queued = false
    );
    // Most waypoints one unit may hold, counting its current destination.
    static constexpr std::size_t MAX_MOVE_WAYPOINTS = 32;
    // MOVE commands carry this bit in `extra` (beside the spacing) when the
    // order appends a waypoint instead of replacing the move order.
    static constexpr uint32_t MOVE_QUEUE_FLAG = 0x80000000u;
    // BUILD commands carry this bit in `extra` when they order a structure
    // (its type in the low byte) instead of a unit. The ordering entity must be
    // one of the faction's construction units, so a structure order is an
    // ordinary recorded command that plays back like any other.
    static constexpr uint32_t STRUCTURE_ORDER_FLAG = 0x40000000u;
    size_t issue_structure_command(EntityId engineer, FactionId faction, uint8_t structure_type, float x, float y);
    bool is_construction_unit(EntityId entity) const;
    // True while the faction's line holds a job for this structure type,
    // counting the earthworks that level its site.
    bool structure_queued(FactionId faction, uint8_t structure_type) const;
    // The unit's current destination followed by its queued waypoints, in
    // the order it will drive them. Empty when the unit has no move order.
    std::vector<std::pair<float, float>> get_unit_waypoints(EntityId entity) const;
    std::size_t queued_waypoint_count(EntityId entity) const;
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
        float spacing,
        bool queued = false
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
    bool has_move_target(EntityId entity) const;
    // True while a ground unit is driving aside (or back) to give a neighbour
    // room. That is not an order: it ends by itself and any real order replaces it.
    bool is_making_room(EntityId entity) const;
    // Presentation-only read of the route the movement system will follow
    // from the unit's current position: the unit position, the turns it will
    // take, then its arrival point, then each queued waypoint's leg (its turns
    // and its arrival point) in order. It mirrors update_movement (straight while
    // the arrival point is in line of sight, otherwise the flow field toward
    // the strategic route) and keeps the cells where that walk changes
    // direction. A very long staircase stops at MAX_ROUTE_POINTS without its
    // arrival point. Returns false when the unit has no move order. Never
    // changes simulation state.
    static constexpr std::size_t MAX_ROUTE_POINTS = 128;
    bool get_unit_route(EntityId entity, std::vector<std::pair<float, float>>& route) const;
    float get_unit_x(EntityId entity) const;
    float get_unit_y(EntityId entity) const;
    float get_unit_heading(EntityId entity) const;
    bool get_unit_off_road_state(EntityId entity, float& wear, float& distance, float& speed_multiplier) const;
    bool get_unit_steering_state(EntityId entity, float& heading, float& desired_heading, float& speed) const;
    bool get_aircraft_approach_pose(EntityId entity, float& altitude, float& pitch, float& bank) const;
    
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
    StructureSite assess_structure_site(uint8_t structure_type, float x, float y) const;
    // Queues the structure, with an earthworks job first when the ground needs it.
    bool queue_structure(EntityId line, FactionId faction, uint8_t structure_type, float x, float y);
    bool validate_engineer_placement(float x, float y) const;
    bool validate_road_placement(float start_x, float start_y, float end_x, float end_y) const;
    bool queue_road(EntityId engineer, FactionId owner, float start_x, float start_y, float end_x, float end_y);
    const RoadNetwork& road_network() const { return road_network_; }
    void enable_theater_water_rules(bool enabled = true) { theater_water_rules_enabled_ = enabled; }

    // Faction initialization
    EntityId create_faction_base(FactionId faction, float x, float y);
    void initialize_faction(FactionId faction_id, float x, float y);
    // Returns -1 without creating anything when the faction is at its unit cap.
    int create_unit_with_type(float x, float y, UnitType unit_type, FactionId faction_id);
    int unit_count(FactionId faction_id) const;
    int unit_cap_per_player() const { return unit_cap_per_player_; }
    // Benchmarks raise this to probe beyond the gameplay limit.
    void set_unit_cap_per_player(int cap) { unit_cap_per_player_ = cap; }
    bool at_unit_cap(FactionId faction_id) const { return unit_count(faction_id) >= unit_cap_per_player_; }
    void set_unit_faction(EntityId entity, FactionId faction_id);

    
    uint32_t simulation_tick() const { return tick_; }
    // How far the clock has run into the next 50 ms tick, from 0 to 1. The view
    // blends the last two ticks by this so motion is smooth at any frame rate.
    float tick_fraction() const { return elapsed_ms_ / 50.0f; }
    void enable_ai(bool enabled) { ai_enabled_ = enabled; }
    bool ai_enabled() const { return ai_enabled_; }

    // Match recording. begin_match starts a record; it closes when the match ends.
    MatchRecord& match_record() { return match_record_; }
    const MatchRecord& match_record() const { return match_record_; }
    // A presentation-layer call that changed the match outside the command queue.
    void record_external_action(std::string name, std::vector<double> args);
    // A hash of everything that decides the match. Two runs that agree on every
    // hash played out the same way.
    uint64_t state_hash() const;
    // Feeds recorded orders back in at the tick each one originally ran.
    // Turn the AI off first: its orders are already in the recording.
    void play_back_commands(std::vector<InputCommand> commands);
    bool playing_back() const { return playback_cursor_ < playback_.size(); }
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

    // Forces arrive by transport. With this on, ground units are not built in
    // the theatre: an order pays for the unit, waits out its build and flight
    // time, then lands at the faction's airfield (or, for a side that has none
    // yet, its landing zone). Aircraft and boats keep their own arrival.
    struct InboundForce {
        FactionId faction;
        UnitType type;
        uint32_t arrive_tick;
        // A paratroop drop lands at its drop point instead of the landing site.
        bool airdrop = false;
        float drop_x = 0.0f;
        float drop_y = 0.0f;
        // Orders the drop count from, so each paratrooper lands on its own spot.
        uint32_t drop_sequence = 0;
        // A drop flies in on a cargo aircraft once its unit is ready. It is
        // released over the zone; if the aircraft is shot down first it is lost.
        uint32_t launch_tick = 0;
        EntityId transport = INVALID_ENTITY;
        bool released = false;
        // AIR is the original airfield delivery. ROAD and RAIL use authored
        // ground corridors and unload at their respective terminal.
        uint8_t route_mode = 0;
        uint32_t order_id = 0;
    };
    enum class GroundTransport : uint8_t { AIR = 0, ROAD = 1, RAIL = 2 };
    struct TransportRoute {
        bool enabled = false;
        float entry_x = 0.0f, entry_y = 0.0f;
        float terminal_x = 0.0f, terminal_y = 0.0f;
    };
    static constexpr float kTransitSeconds = 20.0f;
    static constexpr int kMaxInboundForces = 24;
    void set_forces_by_transport(bool enabled) { forces_by_transport_ = enabled; }
    // Ground vehicles draw fuel from their side's store as they drive, at the
    // unit data's fuel_per_km. A vehicle whose side cannot pay for the next
    // step does not move; it can still turn its gun and fire.
    void set_ground_fuel_burn(bool enabled) { ground_fuel_burn_ = enabled; }
    bool ground_fuel_burn() const { return ground_fuel_burn_; }
    // Ground vehicles of this side held still last tick for want of fuel.
    int fuel_starved_units(FactionId faction) const { return fuel_starved_[static_cast<size_t>(faction)]; }
    bool forces_by_transport() const { return forces_by_transport_; }
    bool set_landing_zone(FactionId faction, float x, float y);
    // Where a faction's ground forces land, if anywhere.
    bool landing_site(FactionId faction, float& x, float& y) const;
    bool needs_delivery(UnitType type) const;
    // Empty when the faction can order this unit now, else why not.
    std::string delivery_refusal(FactionId faction, UnitType type) const;
    // Orders a unit the way a faction normally gets one: delivered when the
    // transport rule is on for ground units, otherwise built by its line.
    bool order_unit(FactionId faction, UnitType type, float x, float y);
    bool order_delivery(FactionId faction, UnitType type);
    bool configure_transport_route(FactionId faction, GroundTransport mode,
                                   float entry_x, float entry_y, float terminal_x, float terminal_y);
    TransportRoute transport_route(FactionId faction, GroundTransport mode) const;
    std::string delivery_refusal_via(FactionId faction, UnitType type, GroundTransport mode) const;
    bool order_delivery_via(FactionId faction, UnitType type, GroundTransport mode);
    size_t issue_ground_transport_command(FactionId faction, UnitType type, GroundTransport mode);

    // Paratroop drops. A transport flies in from the west map edge and drops
    // the unit by parachute onto ground the faction can see right now. It
    // needs no airfield, but pays the catalog price at once and counts against
    // the unit cap and the in-air limit like any delivery. Only units marked
    // `airdrop` in the unit data can be dropped. The order is a BUILD command
    // from the command post with PARADROP_ORDER_FLAG, so it is recorded.
    static constexpr uint32_t PARADROP_ORDER_FLAG = 0x20000000u;
    static constexpr uint32_t ROAD_DELIVERY_ORDER_FLAG = 0x08000000u;
    static constexpr uint32_t RAIL_DELIVERY_ORDER_FLAG = 0x04000000u;
    // A BUILD from the command post with this bit asks for a fuel shipment,
    // so a shipment order is recorded and plays back like any other order.
    static constexpr uint32_t FUEL_SHIPMENT_ORDER_FLAG = 0x10000000u;
    size_t issue_fuel_shipment_command(FactionId faction);
    static constexpr float kParachuteSeconds = 12.0f;
    // The cargo aircraft releases the drop when it is this close to the zone.
    static constexpr float kParadropReleaseRadius = 100.0f;
    // Drops lost because their aircraft was shot down, since the match began.
    int paradrops_lost(FactionId faction) const { return paradrops_lost_[static_cast<size_t>(faction)]; }
    // The side's unarmed cargo aircraft, which flies shipments and drops in.
    std::optional<UnitType> cargo_aircraft_type(FactionId faction) const;
    static constexpr float kParadropScatterMeters = 150.0f;
    // Empty when the faction can drop this unit at this point now, else why not.
    std::string paradrop_refusal(FactionId faction, UnitType type, float x, float y) const;
    bool order_paradrop(FactionId faction, UnitType type, float x, float y);
    size_t issue_paradrop_command(FactionId faction, UnitType type, float x, float y);
    // Seconds from the order until the unit is on the ground at this point.
    float paradrop_seconds(UnitType type, float x) const;
    int inbound_count(FactionId faction) const;
    const std::vector<InboundForce>& inbound_forces() const { return inbound_forces_; }

    // Arms the win condition: a side loses when its command post is destroyed.
    void begin_match(EntityId player_post, EntityId enemy_post);
    bool configure_key_site_victory(const std::vector<EntityId>& sites, int required, uint32_t hold_ticks);
    int match_result() const { return match_result_; } // -1 playing, 0 player won, 1 enemy won, 2 draw
    int match_victory_reason() const { return match_victory_reason_; } // 0 command post, 1 key sites
    int key_sites_required() const { return key_sites_required_; }
    uint32_t key_site_hold_ticks_required() const { return key_site_hold_ticks_required_; }
    int key_sites_held(FactionId faction) const {
        const auto index = static_cast<size_t>(faction);
        return index < key_sites_held_.size() ? key_sites_held_[index] : 0;
    }
    uint32_t key_site_hold_ticks(FactionId faction) const {
        const auto index = static_cast<size_t>(faction);
        return index < key_site_hold_ticks_.size() ? key_site_hold_ticks_[index] : 0;
    }
    float match_seconds() const { return (tick_ - match_start_tick_) * 0.05f; }

    const VisionMap& vision_map() const { return vision_map_; }
    const std::vector<std::array<float, 2>>& radar_contacts(FactionId faction) const {
        static const std::vector<std::array<float, 2>> empty;
        const auto index = static_cast<size_t>(faction);
        return index < radar_contacts_.size() ? radar_contacts_[index] : empty;
    }
    uint32_t vision_revision() const { return vision_revision_; }
    bool can_see(FactionId faction, float x, float y) const { return vision_map_.visible(faction, x, y); }
    bool can_see_sky(FactionId faction, float x, float y) const { return vision_map_.visible_in_sky(faction, x, y); }
    bool vision_active() const { return vision_active_; }
    const SightingLog& last_known(FactionId faction) const { return sightings_[static_cast<size_t>(faction)]; }
    int recon_flights_in_air(FactionId faction) const;
    bool on_recon_flight(EntityId entity) const { return recon_flights_.contains(entity); }
    std::vector<EntityId> seen_enemies(FactionId faction) const;

    bool request_fuel_shipment(FactionId player);
    const FuelShipment* fuel_shipment(FactionId faction) const;
    const std::string& fuel_supply_reason(FactionId faction) const;
    bool assign_fuel_depot(EntityId depot, FactionId holder);
    // Scenario setup: a Materials site that starts the match in a side's hands.
    bool assign_material_site(EntityId site, FactionId holder);
    std::vector<FuelDepotStatus> fuel_depots() const;
    // Every Materials site, its holder and whether a unit is on its way to take it.
    std::vector<MaterialSiteStatus> material_sites() const;

private:
    void record_combat_event(CombatEventKind kind, EntityId entity, float vx = 0, float vy = 0);
    uint64_t combat_event_sequence_ = 0; // Monotonic even across match resets.
    std::array<uint64_t, 3> combat_event_viewer_sequences_{};
    std::deque<CombatPresentationEvent> combat_events_;
    void refresh_unit_counts();
    static constexpr size_t FACTION_COUNT = 3;
    // Recounted every economy phase so deaths free slots; creation increments.
    std::array<int, FACTION_COUNT> unit_counts_{};
    int unit_cap_per_player_ = MAX_UNITS_PER_PLAYER;

    bool running_{false};
    bool ai_enabled_ = true;
    std::vector<InputCommand> command_log_;
    uint32_t tick_{0};
    float game_speed_ = 1.0f;
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
    // Waypoints after the current move target, in driving order.
    std::unordered_map<EntityId, std::deque<MoveTarget>> queued_moves_;
    bool assign_move_target(EntityId entity, float x, float y, float route_x, float route_y);
    bool trace_route_leg(EntityId entity, float from_x, float from_y, const MoveTarget& target,
                         std::vector<std::pair<float, float>>& route) const;
    struct PatrolOrder { Position origin; Position destination; bool returning = false; };
    std::unordered_map<EntityId, PatrolOrder> patrol_orders_;
    Terrain terrain_;
    struct SiteClaim {
        float x, y, half_x, half_y, pad_height;
    };
    bool site_claimed(float x, float y, float half_x, float half_y) const;
    std::vector<SiteClaim> site_claims_;
    void refresh_contact_grid();
    bool near_enemy(float x, float y, FactionId faction) const;
    std::array<uint8_t, 128 * 128> contact_grid_{};
    void update_match_result();
    void refresh_vision();
    void update_sightings(FactionId faction);
    bool is_airborne(EntityId entity) const;
    bool is_watched(FactionId faction, EntityId entity, float x, float y) const;
    bool has_unit_near(FactionId faction, float x, float y, float radius) const;
    void update_recon_flights(float delta_ms);
    void update_ferry_flights(float delta_ms);
    void update_air_discipline();
    bool orbit_eligible(EntityId entity) const;
    bool nearest_friendly_airfield(FactionId faction, float x, float y, float& ax, float& ay) const;
    bool begin_airfield_return(EntityId entity);
    struct VisionSource { FactionId faction; float x, y, radius, eye_height; };
    struct ReconFlight {
        float x = 0.0f;
        float y = 0.0f;
        float loiter_ms = 0.0f;
        bool on_station = false;
        bool leaving = false;
        uint32_t next_leg_tick = 0;
        int leg = 0;
    };
    std::map<EntityId, ReconFlight> recon_flights_;
    // Manufactured runway aircraft ferrying in to their side's airfield.
    // They roll out on the main runway, then taxi to distinct apron slots.
    // `sortie` marks a plane heading home on low fuel; with no airfield left it
    // goes back to orbiting instead of leaving the map.
    struct FerryFlight {
        bool leaving = false;
        bool sortie = false;
        bool landed = false;
        bool taxiing = false;
        int parking_slot = -1;
        size_t taxi_leg = 0;
        float airfield_x = 0, airfield_y = 0;
        std::optional<AirfieldApproach> approach;
    };
    std::map<EntityId, FerryFlight> ferry_flights_;
    // A conventional aircraft cannot hold still: with no order it flies a wide
    // circle that passes through the point it was sent to.
    struct AirOrbit { float cx = 0.0f; float cy = 0.0f; float angle = 0.0f; float radius = 0.0f; };
    std::unordered_map<EntityId, AirOrbit> air_orbits_;
    FactionSightings sightings_{};
    uint32_t vision_terrain_revision_ = 0;
    std::vector<VisionSource> vision_sources_;
    // No-fog combat uses per-faction observer lists built once for the combat
    // phase. Sight still reads live positions and health for each candidate.
    std::array<std::vector<EntityId>, FACTION_COUNT> combat_observers_{};
    bool combat_observers_active_ = false;
    VisionMap vision_map_;
    VisionMap intel_map_;
    std::array<std::vector<std::array<float, 2>>, 3> radar_contacts_;
    bool vision_active_ = false;
    uint32_t vision_revision_ = 0;
    float world_width_ = 0.0f;
    float world_height_ = 0.0f;
    MatchRecord match_record_;
    std::vector<InputCommand> playback_;
    size_t playback_cursor_ = 0;
    void feed_playback();
    void record_match_tick();
    MatchSample sample_factions() const;
    EntityId match_player_post_ = INVALID_ENTITY;
    EntityId match_enemy_post_ = INVALID_ENTITY;
    int match_result_ = -1;
    int match_victory_reason_ = 0;
    FactionId match_player_faction_ = FactionId::ELITE_PRECISION;
    FactionId match_enemy_faction_ = FactionId::MASS_WARFARE;
    std::vector<EntityId> key_site_victory_sites_;
    int key_sites_required_ = 0;
    uint32_t key_site_hold_ticks_required_ = 0;
    std::array<int, FACTION_COUNT> key_sites_held_{};
    std::array<uint32_t, FACTION_COUNT> key_site_hold_ticks_{};
    uint32_t match_start_tick_ = 0;
    void update_fuel_supply(float delta_ms);
    std::vector<FuelCapture> fuel_captures_;
    std::vector<SiteCapture> site_captures_;
    void update_site_captures();
    std::vector<FuelShipment> fuel_shipments_;
    std::array<std::string, FACTION_COUNT> fuel_supply_reasons_{};
    RoadNetwork road_network_;
    TerritorialControlManager territorial_control_;
     ControlPointBattle control_point_battle_;
     ReinforcementDelivery reinforcement_delivery_;
     bool forces_by_transport_ = false;
     bool ground_fuel_burn_ = false;
     std::array<int, FACTION_COUNT> fuel_starved_{};
     std::array<bool, FACTION_COUNT> has_landing_zone_{};
     std::array<std::array<float, 2>, FACTION_COUNT> landing_zones_{};
     std::vector<InboundForce> inbound_forces_;
     std::array<std::array<TransportRoute, 2>, FACTION_COUNT> transport_routes_{};
     uint32_t next_transport_order_id_ = 1;
     uint32_t paradrop_sequence_ = 0;
     std::array<int, FACTION_COUNT> paradrops_lost_{};
     // Puts a side's cargo aircraft in the air at the west map edge, level with
     // its destination, flying straight there. INVALID_ENTITY if it cannot.
     EntityId launch_cargo_aircraft(FactionId faction, float x, float y);
     void deliver_inbound_forces();
     bool land_paradrop(const InboundForce& force);
     RequisitionManager requisition_manager_;
    
    std::unique_ptr<AIManager> ai_manager_;

    void prediction_phase(float delta_ms);
    // Stops a unit at the end of its current leg and starts its next queued leg.
    void complete_move(EntityId entity);
    // Pushes overlapping ground units apart and ends moves whose destination
    // is already occupied. Runs at the end of prediction_phase.
    void separate_ground_units();
    void combat_phase(float delta_ms);
    void environment_phase(float delta_ms);
    void logistics_phase(float delta_ms);
    void economy_phase(float delta_ms);
    void update_harvesters(float delta_ms);
    void apply_completed_road(const RoadSegment& segment);
    // Ground units caught inside a finished structure step out to the nearest
    // walkable ground beyond its footprint, so a builder is never sealed in.
    void evict_ground_units(float x, float y, float half_x, float half_y);
};

Simulation* runtime_simulation();

} // namespace rts
