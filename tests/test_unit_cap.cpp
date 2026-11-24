#include "test_framework.hpp"
#include "simulation/simulation.hpp"

#include <memory>

using namespace rts;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

int count_spawned(Simulation& s, FactionId faction, UnitType type, int attempts) {
    int created = 0;
    for (int i = 0; i < attempts; ++i)
        if (s.create_unit_with_type(float(i % 50) * 4.0f, float(i / 50) * 4.0f, type, faction) >= 0) ++created;
    return created;
}
} // namespace

TEST(unit_cap_default_refuses_the_unit_after_two_thousand) {
    Simulation s; s.start();
    require(s.unit_cap_per_player() == 2000, "gameplay cap is 2000 units per player");
    require(count_spawned(s, FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 2001) == 2000,
            "exactly 2000 units are created");
    require(s.unit_count(FactionId::ELITE_PRECISION) == 2000, "count tracks creation");
    require(s.entity_count() == 2000, "the refused unit creates no entity");
    require(count_spawned(s, FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 1) == 1,
            "one player's cap does not limit another player");
}

TEST(unit_cap_slot_returns_after_a_unit_dies) {
    Simulation s; s.start();
    s.set_unit_cap_per_player(2);
    const int first = s.create_unit_with_type(0, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    require(first >= 0 && s.create_unit_with_type(8, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION) >= 0,
            "units below the cap are created");
    require(s.create_unit_with_type(16, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION) == -1,
            "the cap refuses a third unit");
    s.destroy_unit(static_cast<EntityId>(first));
    s.update(50);
    require(s.unit_count(FactionId::ELITE_PRECISION) == 1, "a destroyed unit leaves the count");
    require(s.create_unit_with_type(16, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION) >= 0,
            "the freed slot can be used");
}

TEST(unit_cap_pauses_production_without_spending_then_resumes) {
    Simulation s; s.start();
    const auto base = s.create_faction_base(FactionId::ELITE_PRECISION, -100, 0);
    auto& p = s.production_manager();
    s.update(50);
    s.set_unit_cap_per_player(s.unit_count(FactionId::ELITE_PRECISION) + 1);
    const int blocker = s.create_unit_with_type(-120, 0, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    require(blocker >= 0, "the blocker fills the last slot");
    require(s.issue_build_commands({base}, FactionId::ELITE_PRECISION, -90, 12, 0) == 1, "a queued build is still accepted at the cap");
    s.update(50);
    // Unit builds are paid for when the command is accepted, so the queued
    // order keeps its materials through the pause.
    const auto materials = p.storages().at(base).tactical.materials.current;
    const auto completed = p.get_construction_count();
    require(p.production_lines().at(base).queue.size() == 1, "the build is queued");
    for (int i = 0; i < 400; ++i) s.update(50);
    require(p.storages().at(base).tactical.materials.current == materials, "a paused build spends nothing further");
    require(p.get_construction_count() == completed, "a paused build never completes");
    s.destroy_unit(static_cast<EntityId>(blocker));
    for (int i = 0; i < 400; ++i) s.update(50);
    require(p.get_construction_count() == completed + 1, "production resumes when a slot opens");
    require(p.storages().at(base).tactical.materials.current == materials, "resuming does not charge a second time");
    require(s.at_unit_cap(FactionId::ELITE_PRECISION), "the finished unit takes the free slot");
}

TEST(unit_cap_rejects_reinforcement_without_charging) {
    Simulation s; s.start();
    s.create_faction_base(FactionId::ELITE_PRECISION, -100, 0);
    s.territorial_control_manager().add_installation(-1000.0f, 0.0f, InstallationType::AIRFIELD, FactionId::ELITE_PRECISION);
    s.set_reinforcement_resources(FactionId::ELITE_PRECISION, 600.0f, 400.0f);
    require(s.configure_reinforcement_delivery(-999.0f, 0.0f, 20.0f, FactionId::ELITE_PRECISION), "delivery configures");
    s.update(50);
    s.set_unit_cap_per_player(s.unit_count(FactionId::ELITE_PRECISION));
    const EntityId line = s.production_manager().faction_line(FactionId::ELITE_PRECISION);
    const auto before = s.production_manager().storages().at(line).tactical.materials.current;
    require(!s.request_reinforcement_delivery(FactionId::ELITE_PRECISION), "delivery is refused at the cap");
    require(s.reinforcement_delivery().reason() == "UNIT CAP REACHED", "the refusal states the cap");
    require(s.reinforcement_delivery().state() == ReinforcementDeliveryState::SELECTED, "the selected airfield survives the refusal");
    require(s.production_manager().storages().at(line).tactical.materials.current == before, "a refused delivery charges nothing");
    s.set_unit_cap_per_player(s.unit_count(FactionId::ELITE_PRECISION) + 1);
    require(s.request_reinforcement_delivery(FactionId::ELITE_PRECISION), "delivery proceeds once a slot exists");
    const int inbound_faction_units = s.unit_count(FactionId::ELITE_PRECISION);
    for (int i = 0; i < 100; ++i) s.update(50);
    require(s.reinforcement_delivery().delivery_completed(), "delivery lands");
    require(s.unit_count(FactionId::ELITE_PRECISION) == inbound_faction_units + 1, "the delivered unit spawns in its reserved slot");
}

TEST(unit_cap_snapshot_holds_every_faction_at_the_cap) {
    Simulation s; s.start();
    require(count_spawned(s, FactionId::ELITE_PRECISION, UnitType::ELITE_MAIN_BATTLE_TANK, 2000) == 2000, "elite fills its cap");
    require(count_spawned(s, FactionId::MASS_WARFARE, UnitType::MASS_SWARM_TANK, 2000) == 2000, "mass fills its cap");
    require(count_spawned(s, FactionId::INDUSTRIAL_EXPERIMENTAL, UnitType::INDUSTRIAL_MBT, 2000) == 2000, "industrial fills its cap");
    s.network_manager().send_snapshot();
    const auto snapshot = std::make_unique<Snapshot>();
    require(s.network_manager().receive_snapshot(*snapshot), "snapshot is stored");
    require(snapshot->entity_count == 6000, "every capped unit is in the snapshot");
    require(s.network_manager().snapshot_entities_dropped() == 0, "nothing was dropped");
}
