#pragma once

#include <unordered_map>
#include <vector>

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
    
    float time_since_last_decision_ = 0.0f;
    const float decision_interval_ = 1000.0f;
    
    void update_visibility();
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
