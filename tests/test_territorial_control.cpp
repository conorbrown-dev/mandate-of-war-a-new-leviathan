#include "test_framework.hpp"
#include "ecs/components/territorial_control.hpp"
#include "ecs/components/factions.hpp"
#include "simulation/simulation.hpp"

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

TEST(unit_capabilities_are_assigned_from_prototypes) {
    using namespace rts;

    const auto& prototypes = get_unit_prototypes();
    const auto engineer_prototype = prototypes.find(UnitType::INDUSTRIAL_ENGINEERING);
    if (engineer_prototype == prototypes.end() || engineer_prototype->second.capabilities.size() != 8) {
        throw std::runtime_error("Industrial engineering capabilities should be authored in content");
    }

    Simulation simulation;
    simulation.start();
    const Entity combat_unit{
        static_cast<EntityId>(simulation.create_unit_with_type(
            0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION))};
    const Entity engineer{
        static_cast<EntityId>(simulation.create_unit_with_type(
            10.0f, 0.0f, UnitType::INDUSTRIAL_ENGINEERING,
            FactionId::INDUSTRIAL_EXPERIMENTAL))};

    if (!simulation.territorial_control().has_capability(combat_unit, SeizureCapability::SEIZURE) ||
        !simulation.territorial_control().has_capability(combat_unit, SeizureCapability::DEFEND)) {
        throw std::runtime_error("combat prototype should receive combat territorial capabilities");
    }
    if (simulation.territorial_control().has_capability(combat_unit, SeizureCapability::CONSTRUCT_FOB)) {
        throw std::runtime_error("combat prototype should not construct FOBs");
    }
    if (!simulation.territorial_control().has_capability(engineer, SeizureCapability::CONSTRUCT_FOB) ||
        !simulation.territorial_control().has_capability(engineer, SeizureCapability::CONSTRUCT_LOGISTICS) ||
        !simulation.territorial_control().has_capability(engineer, SeizureCapability::HARVEST_SECURED)) {
        throw std::runtime_error("Industrial engineering prototype should receive construction capabilities");
    }
}

TEST(fob_construction_progress_completes_deterministically) {
    using namespace rts;

    TerritorialControlManager manager;
    manager.add_installation(20.0f, 30.0f, InstallationType::FORWARD_OPERATING_BASE,
                              FactionId::INDUSTRIAL_EXPERIMENTAL);

    const auto initial = manager.get_installation_at(20.0f, 30.0f);
    if (initial.active || !initial.constructing || initial.construction_progress != 0.0f ||
        initial.construction_cost != 500.0f) {
        throw std::runtime_error("FOB should start as a paid, inactive construction");
    }

    manager.update(5000.0f);
    const auto halfway = manager.get_installation_at(20.0f, 30.0f);
    if (halfway.active || !halfway.constructing || halfway.construction_progress < 0.49f ||
        halfway.construction_progress > 0.51f) {
        throw std::runtime_error("FOB should report deterministic halfway progress");
    }

    manager.update(5000.0f);
    const auto complete = manager.get_installation_at(20.0f, 30.0f);
    if (!complete.active || complete.constructing || complete.construction_progress != 1.0f) {
        throw std::runtime_error("FOB should become active exactly at completion");
    }
}

TEST(fob_install_command_requires_construction_capability) {
    using namespace rts;

    Simulation simulation;
    simulation.start();
    const auto combat = static_cast<EntityId>(simulation.create_unit_with_type(
        0.0f, 0.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION));
    const auto engineer = static_cast<EntityId>(simulation.create_unit_with_type(
        5.0f, 0.0f, UnitType::INDUSTRIAL_ENGINEERING, FactionId::INDUSTRIAL_EXPERIMENTAL));

    if (simulation.issue_install_commands({combat}, FactionId::ELITE_PRECISION, 20.0f, 30.0f,
                                           InstallationType::FORWARD_OPERATING_BASE) != 0) {
        throw std::runtime_error("combat unit without CONSTRUCT_FOB must be rejected");
    }
    if (simulation.issue_install_commands({engineer}, FactionId::INDUSTRIAL_EXPERIMENTAL, 20.0f, 30.0f,
                                           InstallationType::FORWARD_OPERATING_BASE) != 1) {
        throw std::runtime_error("engineering unit with CONSTRUCT_FOB should be accepted");
    }
}
