#include "test_framework.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/factions.hpp"
#include "ecs/components/weapon.hpp"

using namespace rts;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string catalog_unit(int id, const std::string& key, const std::string& weapon) {
    return "{\"id\":" + std::to_string(id) + ",\"type\":\"" + key + "\",\"name\":\"Test\",\"faction\":\"MASS_WARFARE\","
        "\"visual_id\":\"visual.test\",\"material_cost\":10,\"build_time_seconds\":1,\"hp\":10,\"speed\":1,"
        "\"range\":10,\"view_range\":10,\"nato_symbol\":\"medium_armor\",\"strategic_icon\":\"mbt\","
        "\"steering\":{\"acceleration\":1,\"deceleration\":1,\"turn_rate\":1,\"turn_rate_at_speed\":1,"
        "\"minimum_turn_radius\":1,\"max_reverse_speed\":1,\"reverse_preference_threshold\":1,"
        "\"steering_response\":1,\"maneuver_turn_rate\":1,\"can_pivot_turn\":false}" + weapon + "}";
}

const std::string VALID_WEAPON =
    ",\"weapon\":{\"damage\":3,\"range\":40,\"fire_rate\":2,\"projectile_speed\":9,\"aoe_radius\":1,\"lead_bias\":0.5}";

bool catalog_rejected(const std::string& text) {
    try {
        parse_unit_catalog(text);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}
} // namespace

TEST(unit_data_new_unit_needs_only_a_data_entry) {
    const auto catalog = parse_unit_catalog("{\"unit_types\":[" + catalog_unit(200, "BRAND_NEW_UNIT", VALID_WEAPON) + "]}");
    const auto found = catalog.find(static_cast<UnitType>(200));
    require(found != catalog.end(), "an id with no C++ name is a valid unit");
    require(found->second.key == "BRAND_NEW_UNIT", "the unit keeps its data key");
    require(found->second.weapon.damage == 3.0f && found->second.weapon.range == 40.0f, "weapon stats come from data");
}

TEST(unit_data_rejects_invalid_entries) {
    require(catalog_rejected("{\"unit_types\":[" + catalog_unit(200, "NO_WEAPON", "") + "]}"), "a unit must define a weapon");
    require(catalog_rejected("{\"unit_types\":[" + catalog_unit(300, "HIGH_ID", VALID_WEAPON) + "]}"), "ids must fit the command format");
    require(catalog_rejected("{\"unit_types\":[" + catalog_unit(1, "A", VALID_WEAPON) + "," + catalog_unit(1, "B", VALID_WEAPON) + "]}"), "ids must be unique");
    require(catalog_rejected("{\"unit_types\":[" + catalog_unit(1, "A", VALID_WEAPON) + "," + catalog_unit(2, "A", VALID_WEAPON) + "]}"), "keys must be unique");
}

TEST(unit_data_drives_spawned_weapons_and_keys) {
    const auto type = unit_type_from_key("MASS_SWARM_TANK");
    require(type.has_value(), "keys resolve to data ids");
    const auto& prototype = get_unit_prototypes().at(*type);
    Simulation s; s.start();
    const int id = s.create_unit_with_type(0, 0, *type, FactionId::MASS_WARFARE);
    require(id >= 0, "the unit spawns");
    const auto* weapon = s.component_manager().get_component<Weapon>(static_cast<EntityId>(id));
    require(weapon && weapon->damage == prototype.weapon.damage && weapon->range == prototype.weapon.range &&
            weapon->aoe_radius == prototype.weapon.aoe_radius, "spawned weapons copy unit data");
}

TEST(unit_data_factions_load_from_data) {
    const auto& factions = get_faction_start_data();
    require(factions.size() == 3, "all factions are defined in data");
    for (const auto& [id, start] : factions) {
        require(!start.name.empty() && start.start_material > 0.0f && !start.start_units.empty(), "each faction has starting data");
        require(faction_name(id) == start.name, "faction names come from data");
    }
}

TEST(unit_data_intel_range_is_optional_validated_and_copied_to_sensors) {
    const auto legacy = parse_unit_catalog("{\"unit_types\":[" + catalog_unit(200, "LEGACY", VALID_WEAPON) + "]}");
    require(legacy.at(static_cast<UnitType>(200)).intel_range == 0, "legacy content has no invented radar");
    for (const std::string value : {"-1", "10001", "\"wide\""}) {
        auto unit = catalog_unit(200, "BAD_RADAR", VALID_WEAPON);
        unit.insert(1, "\"intel_range\":" + value + ",");
        require(catalog_rejected("{\"unit_types\":["+unit+"]}"), "invalid radar range is rejected");
    }
    Simulation sim; sim.start();
    for (const auto& [type, prototype] : get_unit_prototypes()) {
        const auto id = sim.create_unit_with_type(0,0,type,prototype.faction);
        const auto* sensor = sim.component_manager().get_component<UnitData>(static_cast<EntityId>(id));
        require(sensor && sensor->intel_range == prototype.intel_range && prototype.intel_range > prototype.view_range,
                "authored intel coverage is independent from sight and reaches farther");
    }
}
