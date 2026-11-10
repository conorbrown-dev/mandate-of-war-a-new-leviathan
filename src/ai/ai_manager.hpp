#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ai/ai_doctrine.hpp"
#include "ecs/entity.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/production.hpp"
#include "spatial/spatial_grid.hpp"

namespace rts {

class Simulation;

class AIManager {
public:
    AIManager();
    ~AIManager() = default;

    void set_simulation(Simulation* sim);
    void set_faction_id(FactionId faction_id);
    
    void update(float delta_ms);
    void reset();

    std::vector<EntityId> get_visible_units() const;
    std::vector<EntityId> get_enemy_units() const;
    const std::vector<std::vector<EntityId>>& army_groups() const { return army_groups_; }
    EntityId focus_target() const { return focus_target_; }
    EntityId expansion_target() const { return expansion_target_; }
    float front_x() const { return front_x_; }
    float front_y() const { return front_y_; }
    float staging_x() const { return staging_x_; }
    float staging_y() const { return staging_y_; }
    
    void gather_resources();
    void produce_units();
    void attack_enemy();
    void defend_base();
    void set_objective(float x, float y);
    // Holds units at home until a wave is ready, then commits them together.
    // Each wave grows by wave_growth up to max_wave.
    // The rectangle is the sector the briefing names, which is all the player is
    // told too. The AI learns where the enemy is the same way: by seeing it.
    void configure_assault(float min_x, float min_y, float max_x, float max_y, int first_wave, int wave_growth, int max_wave);
    int waves_launched() const { return waves_launched_; }
    bool is_committed(EntityId unit) const { return committed_.contains(unit); }
    int scout_count() const;
    int recon_flights_ordered() const { return recon_flights_ordered_; }
    // Forces arrive by transport, so the AI needs its own airfield to receive
    // them, built by its Field Engineer at the ordinary price, as the player does.
    int airfield_orders() const { return airfield_orders_; }
    // Fuel runs the army once ground vehicles burn it. Below this the AI asks
    // for a shipment and looks to depots before Materials sites.
    static constexpr float kLowFuel = 1500.0f;
    int fuel_shipments_ordered() const { return fuel_shipments_ordered_; }
    int paradrops_ordered() const { return paradrops_ordered_; }

    // Style and difficulty. Both stay in force across reset() until changed.
    void set_doctrine(const AIDoctrine& doctrine);
    void set_difficulty(const AIDifficulty& difficulty);
    void restore_default_doctrine();
    const AIDoctrine& doctrine() const { return doctrine_; }
    const AIDifficulty& difficulty() const { return difficulty_; }
    int next_wave_size() const { return next_wave_size_; }
    bool is_home_guard(EntityId unit) const { return home_guard_.contains(unit); }

private:
    Simulation* simulation_ = nullptr;
    FactionId faction_id_ = FactionId::MASS_WARFARE;
    
    std::vector<EntityId> visible_units_;
    std::vector<EntityId> enemy_units_;
    std::vector<std::vector<EntityId>> army_groups_;
    EntityId focus_target_{INVALID_ENTITY};
    EntityId expansion_target_{INVALID_ENTITY};
    float front_x_{0.0f}, front_y_{0.0f};
    float staging_x_{0.0f}, staging_y_{0.0f};
    
    // A patch of the search area and when it was last watched or sent to.
    struct Sector {
        float x, y;
        uint32_t last_seen_tick = 0;
        uint32_t last_tasked_tick = 0;
    };
    struct ScoutTask {
        size_t sector = 0;
        uint32_t tasked_tick = 0;
    };

    bool assault_active_ = false;
    float area_min_x_ = 0.0f, area_min_y_ = 0.0f, area_max_x_ = 0.0f, area_max_y_ = 0.0f;
    std::vector<Sector> sectors_;
    std::unordered_map<EntityId, ScoutTask> scout_tasks_;
    float sweep_x_ = 0.0f, sweep_y_ = 0.0f;
    uint32_t next_sweep_tick_ = 0;
    // Scouts ordered but not yet on the map, so a slow queue does not over-order.
    int scouts_on_order_ = 0;
    std::unordered_set<EntityId> arrived_scouts_;
    uint32_t next_flight_tick_ = 0;
    int recon_flights_ordered_ = 0;
    int airfield_orders_ = 0;
    int fuel_shipments_ordered_ = 0;
    uint32_t next_fuel_order_tick_ = 0;
    uint32_t next_airfield_tick_ = 0;
    uint32_t next_paradrop_tick_ = 0;
    int paradrops_ordered_ = 0;
    // Sites the AI cannot know about yet, and sites it has seen in enemy hands.
    bool sites_surveyed_ = false;
    std::unordered_set<EntityId> unknown_sites_;
    std::unordered_set<EntityId> enemy_sites_;
    int next_wave_size_ = 0;
    int wave_growth_ = 0;
    int max_wave_ = 0;
    // The briefing's wave numbers, used when the style leaves them to the scenario.
    int scenario_first_wave_ = 0;
    int scenario_wave_growth_ = 0;
    int scenario_max_wave_ = 0;
    AIDoctrine doctrine_;
    AIDifficulty difficulty_;
    std::unordered_set<EntityId> home_guard_;
    // When each enemy now in sight was first seen, for the reaction delay.
    std::unordered_map<EntityId, uint32_t> first_seen_;
    size_t order_cursor_ = 0;
    size_t mixed_cursor_ = 0;
    int waves_launched_ = 0;
    EntityId expansion_unit_{INVALID_ENTITY};
    std::unordered_set<EntityId> committed_;

    float time_since_last_decision_ = 0.0f;
    float decision_interval_ = 1000.0f;
    void apply_wave_plan();
    
    void update_visibility();
    void apply_reaction_delay();
    void update_sectors();
    void update_scouts();
    void update_sweep();
    void produce_for_assault(EntityId base);
    void build_airfield(EntityId base);
    void order_fuel(EntityId base);
    void order_paradrop(EntityId base);
    float own_fuel() const;
    bool holding_for_fuel(float price) const;
    bool find_airfield_site(EntityId base, float& site_x, float& site_y) const;
    // False when the transport rule would refuse this unit now.
    bool can_receive(UnitType type) const;
    size_t stalest_sector(float x, float y, EntityId scout = INVALID_ENTITY) const;
    size_t plot_recon_flight(float view_range, float base_x, float base_y);
    bool is_recon_asset(EntityId unit) const;
    bool nearest_contact(float x, float y, float& contact_x, float& contact_y) const;
    void update_waves();
    bool home_threatened(EntityId base) const;
    void update_operational_plan();
    void plan_expansion();
    EntityId select_focus_target(EntityId base) const;
    bool objective_set_ = false;
    float objective_x_ = 0, objective_y_ = 0;
};

} // namespace rts

extern "C" {

void ai_init();
void ai_update(float delta_ms);
void ai_reset();
void ai_set_faction_id(int faction_id);
int ai_get_visible_unit_count();
int ai_get_enemy_unit_count();

}
