#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "../entity.hpp"

namespace rts {

// Territorial control states
enum class TerritorialControlState : uint8_t {
    NEUTRAL = 0,              // Unclaimed land, no faction presence
    CLAIMED = 1,              // Claimed but not secured
    SECURED = 2,              // Secured with combat presence
    CONSOLIDATED = 3,         // Consolidated with infrastructure
    ESTABLISHED = 4,          // Fully established as territory
    CONTESTED = 5             // Under active enemy fire/assault
};

// Zone types - progressive territorial control
enum class ZoneType : uint8_t {
    RECONZONE = 0,            // Reconnaissance zone - initial entry point
    SEIZUREZONE = 1,          // Seizure zone - combat operations
    SECUREZONE = 2,           // Secured zone - friendly control
    CONSOLIDATIONZONE = 3,    // Consolidation zone - infrastructure buildout
    ESTABLISHMENTZONE = 4,    // Establishment zone - permanent presence
    FOBZONE = 5,              // Forward Operating Base zone
    LOGISTICSZONE = 6,        // Logistics support zone
    HELIPADZONE = 7           // Helipad/landing zone
};

} // namespace rts

#include "factions.hpp"

namespace rts {

// Site/Installation types
enum class InstallationType : uint8_t {
    COMMAND_POST = 0,
    FORWARD_OPERATING_BASE = 1,
    LOGISTICS_HUB = 2,
    RECON_STATION = 3,
    DEFENSIVE_BATTERY = 4,
    AIRFIELD = 5,
    NAVAL_BASE = 6
};

// Zone grid forward declaration  
class ZoneGrid;

// Zone data for territorial control
struct ZoneData {
    float center_x, center_y;
    float radius;
    ZoneType type = ZoneType::RECONZONE;
    TerritorialControlState state = TerritorialControlState::NEUTRAL;
    FactionId controlling_faction;
    uint32_t entry_tick = 0;
    uint32_t last_update_tick = 0;
    float security_score = 0.0f;
    float progress_value = 0.0f;
    uint32_t state_entry_tick = 0;
    std::vector<EntityId> assigned_units;
};

// Zone grid forward declaration
class ZoneGrid;

// Installation state for territorial control
struct InstallationState {
    InstallationType type;
    bool active = false;
    bool constructing = false;
    FactionId faction;
    float construction_progress = 0.0f;
    float construction_cost = 0.0f;
    float production_bonus = 0.0f;
    float defense_bonus = 0.0f;
    float view_range_bonus = 0.0f;
    bool provides_logsitics = false;
    bool supports_air_operations = false;
    bool supports_naval_operations = false;
};

// Territorial control manager interface
class TerritorialControlManager {
public:
    // Zone management
    void assign_zone(float x, float y, float radius, ZoneType zone_type, FactionId faction);
    void update_zone_state(float x, float y, float radius, TerritorialControlState state);
    
    // Get zone information
    ZoneType get_zone_at(float x, float y) const;
    TerritorialControlState get_control_state(float x, float y) const;
    FactionId get_controlling_faction(float x, float y) const;
    
    // Progression
    bool can_progress_to(ZoneType from, ZoneType to) const;
    float calculate_progress_rate(ZoneType zone, FactionId faction) const;
    
    // Installations
    void add_installation(float x, float y, InstallationType type, FactionId faction);
    void remove_installation(float x, float y, InstallationType type);
    InstallationState get_installation_at(float x, float y) const;
    
    // Unit capabilities
    bool has_capability(Entity entity, SeizureCapability capability) const;
    void assign_capability(Entity entity, SeizureCapability capability);
    void remove_capability(Entity entity, SeizureCapability capability);
    
    // Unit-to-zone assignment
    void assign_unit_to_zone(Entity entity, float x, float y);
    void remove_unit_from_zone(Entity entity, float x, float y);
    void update_unit_zone_assignment(Entity entity, float x, float y);
    
    // Zone progression
    void update_zone_progression(float x, float y, float radius);
    void process_zone_progression();
    
    // Combat presence and security
    void add_combat_presence(float x, float y, float radius, FactionId faction);
    void remove_combat_presence(float x, float y, float radius, FactionId faction);
    // Zone queries
    size_t zone_count() const { return zones_.size(); }
    const std::vector<ZoneData>& get_zones() const { return zones_; }
    

    float get_security_score(float x, float y, FactionId faction) const;
    
    // Lifecycle
    void reset();
    void update(float delta_ms);
    
private:
    struct InstallationData {
        float x, y;
        InstallationType type;
        FactionId faction;
        uint32_t built_tick = 0;
        bool constructing = false;
        float construction_progress = 0.0f;
        float construction_cost = 0.0f;
        uint32_t construction_started_tick = 0;
    };
    
    std::vector<InstallationData> installations_;
    std::unordered_map<EntityId, std::vector<SeizureCapability>> unit_capabilities_;
    std::vector<ZoneData> zones_;
};

} // namespace rts
