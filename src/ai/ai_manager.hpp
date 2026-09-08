#pragma once

#include <unordered_map>
#include <vector>

#include "ecs/entity.hpp"
#include "ecs/components/factions.hpp"
#include "spatial/spatial_grid.hpp"
#include "simulation/simulation.hpp"

namespace rts {

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
    
    void gather_resources();
    void produce_units();
    void attack_enemy();
    void defend_base();

private:
    Simulation* simulation_ = nullptr;
    FactionId faction_id_ = FactionId::ELITE_PRECISION;
    
    std::vector<EntityId> visible_units_;
    std::vector<EntityId> enemy_units_;
    
    float time_since_last_decision_ = 0.0f;
    const float decision_interval_ = 1000.0f; // 1 second between major decisions
    
    void update_visibility();
    int count_visible_extractor_slots() const;
    void build_extractor_if_needed();
    void produce_defensive_units();
    void produce_attack_units();
};

} // namespace rts

#ifdef __cplusplus
extern "C" {
#endif

void ai_init();
void ai_update(float delta_ms);
void ai_reset();
void ai_set_faction_id(int faction_id);
int ai_get_visible_unit_count();
int ai_get_enemy_unit_count();

#ifdef __cplusplus
}
#endif
