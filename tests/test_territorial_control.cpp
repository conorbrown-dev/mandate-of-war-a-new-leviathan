#include "test_framework.hpp"
#include "ecs/components/territorial_control.hpp"
#include "ecs/components/factions.hpp"

TEST(territorial_control_state_transitions) {
    using namespace rts;
    
    TerritorialControlState state = TerritorialControlState::NEUTRAL;
    
    if (state != TerritorialControlState::NEUTRAL) {
        throw std::runtime_error("Initial state should be NEUTRAL");
    }
    
    state = TerritorialControlState::CLAIMED;
    
    if (state != TerritorialControlState::CLAIMED) {
        throw std::runtime_error("State should transition to CLAIMED");
    }
    
    state = TerritorialControlState::SECURED;
    
    if (state != TerritorialControlState::SECURED) {
        throw std::runtime_error("State should transition to SECURED");
    }
    
    state = TerritorialControlState::CONSOLIDATED;
    
    if (state != TerritorialControlState::CONSOLIDATED) {
        throw std::runtime_error("State should transition to CONSOLIDATED");
    }
    
    state = TerritorialControlState::ESTABLISHED;
    
    if (state != TerritorialControlState::ESTABLISHED) {
        throw std::runtime_error("State should transition to ESTABLISHED");
    }
}

TEST(zone_type_progression) {
    using namespace rts;
    
    ZoneType zone = ZoneType::RECONZONE;
    
    if (zone != ZoneType::RECONZONE) {
        throw std::runtime_error("Initial zone should be RECONZONE");
    }
    
    zone = ZoneType::HELIPADZONE;
    
    if (zone != ZoneType::HELIPADZONE) {
        throw std::runtime_error("Zone should transition to HELIPADZONE");
    }
    
    zone = ZoneType::SECUREZONE;
    
    if (zone != ZoneType::SECUREZONE) {
        throw std::runtime_error("Zone should transition to SECUREZONE");
    }
    
    zone = ZoneType::CONSOLIDATIONZONE;
    
    if (zone != ZoneType::CONSOLIDATIONZONE) {
        throw std::runtime_error("Zone should transition to CONSOLIDATIONZONE");
    }
    
    zone = ZoneType::ESTABLISHMENTZONE;
    
    if (zone != ZoneType::ESTABLISHMENTZONE) {
        throw std::runtime_error("Zone should transition to ESTABLISHMENTZONE");
    }
    
    zone = ZoneType::FOBZONE;
    
    if (zone != ZoneType::FOBZONE) {
        throw std::runtime_error("Zone should transition to FOBZONE");
    }
    
    zone = ZoneType::LOGISTICSZONE;
    
    if (zone != ZoneType::LOGISTICSZONE) {
        throw std::runtime_error("Zone should transition to LOGISTICSZONE");
    }
    
    zone = ZoneType::HELIPADZONE;
    
    if (zone != ZoneType::HELIPADZONE) {
        throw std::runtime_error("Zone should transition to HELIPADZONE");
    }
}

TEST(seizure_capability_flags) {
    using namespace rts;
    
    if (static_cast<int>(SeizureCapability::RECON) < 0) {
        throw std::runtime_error("RECON capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::SEIZURE) < 0) {
        throw std::runtime_error("SEIZURE capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::SECURE) < 0) {
        throw std::runtime_error("SECURE capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::CONSTRUCT_FOB) < 0) {
        throw std::runtime_error("CONSTRUCT_FOB capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::CONSTRUCT_LOGISTICS) < 0) {
        throw std::runtime_error("CONSTRUCT_LOGISTICS capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::ESTABLISH_BASE) < 0) {
        throw std::runtime_error("ESTABLISH_BASE capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::DEFEND) < 0) {
        throw std::runtime_error("DEFEND capability flag should be non-negative");
    }
    
    if (static_cast<int>(SeizureCapability::HARVEST_SECURED) < 0) {
        throw std::runtime_error("HARVEST_SECURED capability flag should be non-negative");
    }
}

TEST(installation_type_definitions) {
    using namespace rts;
    
    if (static_cast<int>(InstallationType::COMMAND_POST) < 0) {
        throw std::runtime_error("COMMAND_POST should be non-negative");
    }
    
    if (static_cast<int>(InstallationType::FORWARD_OPERATING_BASE) < 0) {
        throw std::runtime_error("FOB should be non-negative");
    }
    
    if (static_cast<int>(InstallationType::LOGISTICS_HUB) < 0) {
        throw std::runtime_error("LOGISTICS_HUB should be non-negative");
    }
    
    if (static_cast<int>(InstallationType::RECON_STATION) < 0) {
        throw std::runtime_error("RECON_STATION should be non-negative");
    }
    
    if (static_cast<int>(InstallationType::DEFENSIVE_BATTERY) < 0) {
        throw std::runtime_error("DEFENSIVE_BATTERY should be non-negative");
    }
    
    if (static_cast<int>(InstallationType::AIRFIELD) < 0) {
        throw std::runtime_error("AIRFIELD should be non-negative");
    }
    
    if (static_cast<int>(InstallationType::NAVAL_BASE) < 0) {
        throw std::runtime_error("NAVAL_BASE should be non-negative");
    }
}

TEST(zone_progression_methods) {
    using namespace rts;
    
    TerritorialControlManager manager;
    
    // Add test zones
    manager.assign_zone(0.0f, 0.0f, 50.0f, ZoneType::RECONZONE, FactionId::ELITE_PRECISION);
    
    bool can_progress = manager.can_progress_to(ZoneType::RECONZONE, ZoneType::SEIZUREZONE);
    if (!can_progress) {
        throw std::runtime_error("Should be able to progress from RECONZONE to SEIZUREZONE");
    }
    
    can_progress = manager.can_progress_to(ZoneType::RECONZONE, ZoneType::SECUREZONE);
    if (can_progress) {
        throw std::runtime_error("Should not be able to skip SEIZUREZONE");
    }
    
    can_progress = manager.can_progress_to(ZoneType::SEIZUREZONE, ZoneType::RECONZONE);
    if (can_progress) {
        throw std::runtime_error("Should not be able to regress to earlier zone");
    }
    
    float rate = manager.calculate_progress_rate(ZoneType::RECONZONE, FactionId::ELITE_PRECISION);
    if (rate <= 0.0f) {
        throw std::runtime_error("Progress rate should be positive");
    }
    
    rate = manager.calculate_progress_rate(ZoneType::SEIZUREZONE, FactionId::MASS_WARFARE);
    if (rate <= 0.0f) {
        throw std::runtime_error("Progress rate should be positive");
    }
    
    rate = manager.calculate_progress_rate(ZoneType::ESTABLISHMENTZONE, FactionId::INDUSTRIAL_EXPERIMENTAL);
    if (rate <= 0.0f) {
        throw std::runtime_error("Progress rate should be positive");
    }
}



