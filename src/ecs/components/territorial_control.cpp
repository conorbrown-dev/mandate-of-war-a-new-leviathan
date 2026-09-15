#include <algorithm>
#include <array>
#include <cmath>

#include "territorial_control.hpp"

namespace rts {

void TerritorialControlManager::assign_zone(float x, float y, float radius, ZoneType zone_type, FactionId faction) {
    ZoneData zone;
    zone.center_x = x;
    zone.center_y = y;
    zone.radius = radius;
    zone.type = zone_type;
    zone.state = TerritorialControlState::NEUTRAL;
    zone.controlling_faction = faction;
    zone.entry_tick = 0;
    zones_.push_back(zone);
}

void TerritorialControlManager::update_zone_state(float x, float y, float radius, TerritorialControlState state) {
    for (auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= radius * radius) {
            zone.state = state;
            if (state == TerritorialControlState::CLAIMED && zone.entry_tick == 0) {
                zone.entry_tick = 0;
            }
        }
    }
}

ZoneType TerritorialControlManager::get_zone_at(float x, float y) const {
    for (const auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            return zone.type;
        }
    }
    return ZoneType::RECONZONE;
}

TerritorialControlState TerritorialControlManager::get_control_state(float x, float y) const {
    for (const auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            return zone.state;
        }
    }
    return TerritorialControlState::NEUTRAL;
}

FactionId TerritorialControlManager::get_controlling_faction(float x, float y) const {
    for (const auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            return zone.controlling_faction;
        }
    }
    return FactionId::ELITE_PRECISION;
}

bool TerritorialControlManager::can_progress_to(ZoneType from, ZoneType to) const {
    constexpr std::array valid_progression = {
        ZoneType::RECONZONE,
        ZoneType::SEIZUREZONE,
        ZoneType::SECUREZONE,
        ZoneType::CONSOLIDATIONZONE,
        ZoneType::ESTABLISHMENTZONE,
        ZoneType::FOBZONE,
        ZoneType::LOGISTICSZONE,
        ZoneType::HELIPADZONE
    };
    
    int from_idx = static_cast<int>(from);
    int to_idx = static_cast<int>(to);
    
    if (from_idx < 0 || from_idx >= static_cast<int>(valid_progression.size()) ||
        to_idx < 0 || to_idx >= static_cast<int>(valid_progression.size())) {
        return false;
    }
    
    return to_idx == from_idx + 1;
}

float TerritorialControlManager::calculate_progress_rate(ZoneType zone, FactionId faction) const {
    // Base progress rate
    float rate = 1.0f;
    
    // Faction-specific modifiers
    // TODO: Implement faction-specific progress rates
    
    // Zone-specific modifiers
    switch (zone) {
        case ZoneType::RECONZONE:
            rate *= 1.5f;  // Recon is fast
            break;
        case ZoneType::SEIZUREZONE:
            rate *= 0.8f;  // Seizure is slow (combat required)
            break;
        case ZoneType::SECUREZONE:
            rate *= 1.2f;  // Securing is moderately fast
            break;
        case ZoneType::CONSOLIDATIONZONE:
            rate *= 0.6f;  // Consolidation requires infrastructure
            break;
        case ZoneType::ESTABLISHMENTZONE:
            rate *= 0.5f;  // Establishment is slowest
            break;
        default:
            rate *= 1.0f;
            break;
    }
    
    return rate;
}

void TerritorialControlManager::add_installation(float x, float y, InstallationType type, FactionId faction) {
    InstallationData installation;
    installation.x = x;
    installation.y = y;
    installation.type = type;
    installation.faction = faction;
    installation.built_tick = 0; // TODO: Replace with actual tick count
    installation.constructing = type == InstallationType::FORWARD_OPERATING_BASE;
    installation.construction_progress = installation.constructing ? 0.0f : 1.0f;
    installation.construction_cost = installation.constructing ? 500.0f : 0.0f;
    installations_.push_back(installation);
}

void TerritorialControlManager::remove_installation(float x, float y, InstallationType type) {
    installations_.erase(
        std::remove_if(installations_.begin(), installations_.end(),
            [x, y, type](const InstallationData& inst) {
                float dx = inst.x - x;
                float dy = inst.y - y;
                return dx * dx + dy * dy < 1.0f && inst.type == type;
            }),
        installations_.end()
    );
}

InstallationState TerritorialControlManager::get_installation_at(float x, float y) const {
    constexpr float INSTALLATION_RADIUS_SQ = 2.25f; // 1.5^2
    
    for (const auto& inst : installations_) {
        float dx = inst.x - x;
        float dy = inst.y - y;
        if (dx * dx + dy * dy <= INSTALLATION_RADIUS_SQ) {
            InstallationState state;
            state.type = inst.type;
            state.active = !inst.constructing;
            state.constructing = inst.constructing;
            state.faction = inst.faction;
            state.construction_progress = inst.construction_progress;
            state.construction_cost = inst.construction_cost;
            
            // Setup bonuses based on installation type
            switch (inst.type) {
                case InstallationType::COMMAND_POST:
                    state.production_bonus = 0.2f;
                    state.defense_bonus = 0.3f;
                    state.view_range_bonus = 50.0f;
                    break;
                case InstallationType::FORWARD_OPERATING_BASE:
                    state.production_bonus = 0.1f;
                    state.defense_bonus = 0.2f;
                    state.provides_logsitics = true;
                    break;
                case InstallationType::LOGISTICS_HUB:
                    state.production_bonus = 0.15f;
                    state.provides_logsitics = true;
                    break;
                case InstallationType::DEFENSIVE_BATTERY:
                    state.defense_bonus = 0.4f;
                    break;
                case InstallationType::AIRFIELD:
                    state.supports_air_operations = true;
                    break;
                case InstallationType::NAVAL_BASE:
                    state.supports_naval_operations = true;
                    break;
                default:
                    break;
            }
            
            return state;
        }
    }
    
    return InstallationState{};
}

bool TerritorialControlManager::has_active_installation(FactionId faction, InstallationType type) const {
    for (const auto& inst : installations_) {
        if (inst.faction == faction && inst.type == type && !inst.constructing) return true;
    }
    return false;
}

bool TerritorialControlManager::get_active_installation_position(FactionId faction, InstallationType type, float& x, float& y) const {
    for (const auto& inst : installations_) {
        if (inst.faction == faction && inst.type == type && !inst.constructing) {
            x = inst.x;
            y = inst.y;
            return true;
        }
    }
    return false;
}

size_t TerritorialControlManager::active_installation_count(FactionId faction, InstallationType type) const {
    size_t count = 0;
    for (const auto& inst : installations_) {
        if (inst.faction == faction && inst.type == type && !inst.constructing) ++count;
    }
    return count;
}

bool TerritorialControlManager::get_active_installation_position_near(FactionId faction, InstallationType type,
                                                                        float query_x, float query_y, float max_distance,
                                                                        float& x, float& y) const {
    if (!std::isfinite(query_x) || !std::isfinite(query_y) || !std::isfinite(max_distance) || max_distance < 0.0f) return false;
    const float max_distance_sq = max_distance * max_distance;
    float best_distance_sq = max_distance_sq;
    bool found = false;
    for (const auto& inst : installations_) {
        if (inst.faction != faction || inst.type != type || inst.constructing) continue;
        const float dx = inst.x - query_x;
        const float dy = inst.y - query_y;
        const float distance_sq = dx * dx + dy * dy;
        if (distance_sq <= best_distance_sq) {
            best_distance_sq = distance_sq;
            x = inst.x;
            y = inst.y;
            found = true;
        }
    }
    return found;
}

bool TerritorialControlManager::has_capability(Entity entity, SeizureCapability capability) const {
    auto it = unit_capabilities_.find(entity.id);
    if (it == unit_capabilities_.end()) {
        return false;
    }
    
    for (auto cap : it->second) {
        if (cap == capability) {
            return true;
        }
    }
    return false;
}

void TerritorialControlManager::assign_capability(Entity entity, SeizureCapability capability) {
    unit_capabilities_[entity.id].push_back(capability);
}

void TerritorialControlManager::remove_capability(Entity entity, SeizureCapability capability) {
    auto it = unit_capabilities_.find(entity.id);
    if (it == unit_capabilities_.end()) {
        return;
    }
    
    auto& caps = it->second;
    caps.erase(
        std::remove(caps.begin(), caps.end(), capability),
        caps.end()
    );
    
    if (caps.empty()) {
        unit_capabilities_.erase(it);
    }
}

void TerritorialControlManager::add_combat_presence(float x, float y, float radius, FactionId faction) {
    // Zone management already handles this via update_zone_state
    // This method is for explicit combat presence tracking
    update_zone_state(x, y, radius, TerritorialControlState::SECURED);
}

void TerritorialControlManager::remove_combat_presence(float x, float y, float radius, FactionId faction) {
    // When combat presence is removed, zone may become contested or neutral
    update_zone_state(x, y, radius, TerritorialControlState::CONTESTED);
}

float TerritorialControlManager::get_security_score(float x, float y, FactionId faction) const {
    float score = 0.0f;
    
    // Check zone state bonuses
    for (const auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            switch (zone.state) {
                case TerritorialControlState::NEUTRAL:
                    score += 0.1f;
                    break;
                case TerritorialControlState::CLAIMED:
                    score += 0.3f;
                    break;
                case TerritorialControlState::SECURED:
                    score += 0.5f;
                    break;
                case TerritorialControlState::CONSOLIDATED:
                    score += 0.7f;
                    break;
                case TerritorialControlState::ESTABLISHED:
                    score += 1.0f;
                    break;
                case TerritorialControlState::CONTESTED:
                    score -= 0.5f;
                    break;
            }
            
            // Friendly faction bonus
            if (zone.controlling_faction == faction) {
                score *= 1.5f;
            }
        }
    }
    
    // Check completed installations only; construction is not yet an active bonus.
    for (const auto& inst : installations_) {
        if (inst.constructing) {
            continue;
        }
        float dx = inst.x - x;
        float dy = inst.y - y;
        if (dx * dx + dy * dy <= 100.0f) { // 10x10 cell radius
            if (inst.faction == faction) {
                switch (inst.type) {
                    case InstallationType::FORWARD_OPERATING_BASE:
                    case InstallationType::LOGISTICS_HUB:
                    case InstallationType::DEFENSIVE_BATTERY:
                        score += 0.3f;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
     return std::max(0.0f, score);
}

void TerritorialControlManager::assign_unit_to_zone(Entity entity, float x, float y) {
    // Find the zone that contains this position
    for (auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            // Check if already assigned
            bool already_assigned = false;
            for (auto assigned : zone.assigned_units) {
                if (assigned == entity.id) {
                    already_assigned = true;
                    break;
                }
            }
            if (!already_assigned) {
                zone.assigned_units.push_back(entity.id);
            }
            return;
        }
    }
}

void TerritorialControlManager::remove_unit_from_zone(Entity entity, float x, float y) {
    // Find all zones that contain this position and remove the unit
    for (auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            auto it = std::find(zone.assigned_units.begin(), zone.assigned_units.end(), entity.id);
            if (it != zone.assigned_units.end()) {
                zone.assigned_units.erase(it);
            }
        }
    }
}

void TerritorialControlManager::update_unit_zone_assignment(Entity entity, float x, float y) {
    // Remove from all zones first
    remove_unit_from_zone(entity, x, y);
    
    // Add to current zone
    assign_unit_to_zone(entity, x, y);
}

void TerritorialControlManager::update_zone_progression(float x, float y, float radius) {
    // Update zones that contain this position
    for (auto& zone : zones_) {
        float dx = zone.center_x - x;
        float dy = zone.center_y - y;
        if (dx * dx + dy * dy <= zone.radius * zone.radius) {
            zone.last_update_tick++;
            
            // Recalculate security score based on assigned units
            float score = 0.0f;
            for (auto unit_id : zone.assigned_units) {
                // TODO: Query unit component for combat strength
                score += 1.0f; // Placeholder: each unit adds 1.0
            }
            zone.security_score = score;
        }
    }
}

void TerritorialControlManager::process_zone_progression() {
    // Process progression for all zones
    for (auto& zone : zones_) {
        // Check if zone can progress to next type
        ZoneType current_type = zone.type;
        
        // Determine target zone based on state and progress
        ZoneType target_type = current_type;
        
        // Simple progression based on state
        switch (zone.state) {
            case TerritorialControlState::NEUTRAL:
                if (zone.security_score > 10.0f) {
                    zone.state = TerritorialControlState::CLAIMED;
                    zone.state_entry_tick = zone.last_update_tick;
                }
                break;
            case TerritorialControlState::CLAIMED:
                if (zone.security_score > 20.0f && 
                    zone.last_update_tick - zone.state_entry_tick > 100) {
                    zone.state = TerritorialControlState::SECURED;
                    zone.state_entry_tick = zone.last_update_tick;
                }
                break;
            case TerritorialControlState::SECURED:
                if (zone.security_score > 30.0f && 
                    zone.last_update_tick - zone.state_entry_tick > 200) {
                    zone.state = TerritorialControlState::CONSOLIDATED;
                    zone.state_entry_tick = zone.last_update_tick;
                }
                break;
            case TerritorialControlState::CONSOLIDATED: {
                // Check for FOB installation
                bool has_fob = false;
                for (const auto& inst : installations_) {
                    float dx = inst.x - zone.center_x;
                    float dy = inst.y - zone.center_y;
                    if (dx * dx + dy * dy <= 100.0f && 
                        inst.type == InstallationType::FORWARD_OPERATING_BASE &&
                        !inst.constructing) {
                        has_fob = true;
                        break;
                    }
                }
                if (has_fob && zone.security_score > 40.0f) {
                    zone.state = TerritorialControlState::ESTABLISHED;
                    zone.state_entry_tick = zone.last_update_tick;
                }
                break;
            }
            case TerritorialControlState::ESTABLISHED: {
                // Check for additional infrastructure
                int installed_count = 0;
                for (const auto& inst : installations_) {
                    if (inst.constructing) {
                        continue;
                    }
                    float dx = inst.x - zone.center_x;
                    float dy = inst.y - zone.center_y;
                    if (dx * dx + dy * dy <= 100.0f && inst.faction == zone.controlling_faction) {
                        installed_count++;
                    }
                }
                if (installed_count >= 3) {
                    zone.state = TerritorialControlState::ESTABLISHED;
                }
                break;
            }
            case TerritorialControlState::CONTESTED:
                if (zone.security_score > 15.0f) {
                    zone.state = TerritorialControlState::SECURED;
                    zone.state_entry_tick = zone.last_update_tick;
                }
                break;
        }
        
        // Update zone type based on progression
        if (can_progress_to(current_type, target_type)) {
            zone.type = target_type;
        }
    }
}

void TerritorialControlManager::reset() {
    zones_.clear();
    installations_.clear();
    unit_capabilities_.clear();
}

void TerritorialControlManager::update(float delta_ms) {
    if (!std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }

    // FOB assembly is deliberately manager-owned and deterministic: ten
    // seconds of simulation time completes a paid construction. The
    // installation remains queryable while constructing, but contributes no
    // active installation bonuses until completion.
    constexpr float FOB_BUILD_TIME_MS = 10000.0f;
    for (auto& installation : installations_) {
        if (!installation.constructing) {
            continue;
        }
        installation.construction_progress = std::min(
            1.0f,
            installation.construction_progress + delta_ms / FOB_BUILD_TIME_MS);
        if (installation.construction_progress >= 1.0f) {
            installation.constructing = false;
            installation.built_tick = 1;
        }
    }
}

} // namespace rts
