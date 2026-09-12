#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rts {

enum class FactionId : uint8_t {
    ELITE_PRECISION = 0,
    MASS_WARFARE = 1,
    INDUSTRIAL_EXPERIMENTAL = 2
};

std::string faction_name(FactionId faction);

// Unit capability tags for Forward Seizure operations
enum class SeizureCapability : uint8_t {
    RECON = 0,
    SEIZURE = 1,
    SECURE = 2,
    CONSTRUCT_FOB = 3,
    CONSTRUCT_LOGISTICS = 4,
    ESTABLISH_BASE = 5,
    DEFEND = 6,
    HARVEST_SECURED = 7
};

// Unit type definitions for each faction
enum class UnitType : uint8_t {
    // Elite Precision (Faction A)
    ELITE_MAIN_BATTLE_TANK = 0,
    ELITE_LONG_RANGE_ARTILLERY = 1,
    ELITE_ANTI_AIR = 2,
    
    // Mass Warfare (Faction B)
    MASS_SWARM_TANK = 3,
    MASS_ASSAULT_VEHICLE = 4,
    MASS_ANTI_AIR = 5,
    
    // Industrial/Experimental (Faction C)
    INDUSTRIAL_MBT = 6,
    INDUSTRIAL_MISSILE_PLATFORM = 7,
    INDUSTRIAL_ENGINEERING = 8,

    // Goal 04 air prototypes
    ELITE_T1_FIGHTER = 9,
    ELITE_T1_VTOL = 10,
    ELITE_PATROL_BOAT = 11
};

struct UnitPrototype {
    std::string name;
    float material_cost;
    float energy_cost;
    float research_cost;
    float build_time_seconds;
    float hp;
    float speed;
    float range;
    float view_range;
    UnitType type;
    FactionId faction;
    std::vector<std::string> research_prerequisites;
    bool is_naval = false;
    bool is_aircraft = false;
    bool requires_runway = false;
    float payload = 0.0f;
    float operational_range = 0.0f;
    float operational_energy = 0.0f;
    float operational_material = 0.0f;
    float max_airborne_time_seconds = 0.0f;
    float energy_consumption_rate = 0.0f;
    float material_consumption_rate = 0.0f;
    // Presentation-only stable reference. Gameplay never derives behavior
    // from a donor model path, mesh name, or dimensions.
    std::string visual_id;
    std::string content_id = "";
    std::vector<SeizureCapability> capabilities = {};
    // Ground-motion tuning is content-owned so factions and chassis can diverge
    // without simulation type checks. These fields are ignored by air and naval
    // prototypes.
    float steering_acceleration = 5.0f;
    float steering_deceleration = 8.0f;
    float steering_turn_rate = 1.8f;
    float steering_turn_rate_at_speed = 1.0f;
    float steering_minimum_turn_radius = 8.0f;
    float steering_max_reverse_speed = 0.0f;
    float steering_reverse_preference_threshold = 2.2f;
    float steering_response = 1.0f;
    bool steering_can_pivot_turn = false;
};

// Unit production data keyed by UnitType
const std::unordered_map<UnitType, UnitPrototype>& get_unit_prototypes();

// Faction starting conditions
struct FactionStartData {
    FactionId faction_id;
    std::string name;
    float start_material;
    float start_energy;
    float start_research;
    std::vector<UnitType> start_units;
    float production_bonus_metal;  // % bonus to metal production
    float production_bonus_energy; // % bonus to energy production
    float unit_cost_discount;      // % discount on unit production
};

// Faction data keyed by FactionId
const std::unordered_map<FactionId, FactionStartData>& get_faction_start_data();

// Research project structure
struct ResearchProject {
    std::string id;
    std::string name;
    std::vector<std::string> prerequisites;
    float cost_per_tick;
    float total_cost;
    float build_time_seconds;
    bool completed;
    std::vector<UnitType> unlocks;
};

// Research projects keyed by project ID
const std::unordered_map<std::string, ResearchProject>& get_research_projects();

// Research manager for a single faction
struct FactionResearch {
    std::unordered_map<std::string, ResearchProject> available_projects;
    std::unordered_map<std::string, bool> completed_projects;
    std::vector<std::string> active_queue;
};

} // namespace rts
