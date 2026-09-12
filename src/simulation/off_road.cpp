#include "simulation/off_road.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>

#include "data/json_parser.hpp"

namespace rts {
namespace {

std::optional<UnitType> unit_type_from_name(const std::string& name) {
    if (name == "ELITE_MAIN_BATTLE_TANK") return UnitType::ELITE_MAIN_BATTLE_TANK;
    if (name == "ELITE_LONG_RANGE_ARTILLERY") return UnitType::ELITE_LONG_RANGE_ARTILLERY;
    if (name == "ELITE_ANTI_AIR") return UnitType::ELITE_ANTI_AIR;
    if (name == "MASS_SWARM_TANK") return UnitType::MASS_SWARM_TANK;
    if (name == "MASS_ASSAULT_VEHICLE") return UnitType::MASS_ASSAULT_VEHICLE;
    if (name == "MASS_ANTI_AIR") return UnitType::MASS_ANTI_AIR;
    if (name == "INDUSTRIAL_MBT") return UnitType::INDUSTRIAL_MBT;
    if (name == "INDUSTRIAL_MISSILE_PLATFORM") return UnitType::INDUSTRIAL_MISSILE_PLATFORM;
    if (name == "INDUSTRIAL_ENGINEERING") return UnitType::INDUSTRIAL_ENGINEERING;
    return std::nullopt;
}

} // namespace

const OffRoadSettings& get_off_road_settings() {
    static const OffRoadSettings settings = [] {
        OffRoadSettings result;
        result.unit_factors = {
            {UnitType::ELITE_MAIN_BATTLE_TANK, 0.80f},
            {UnitType::ELITE_LONG_RANGE_ARTILLERY, 1.10f},
            {UnitType::ELITE_ANTI_AIR, 1.00f},
            {UnitType::MASS_SWARM_TANK, 0.90f},
            {UnitType::MASS_ASSAULT_VEHICLE, 1.20f},
            {UnitType::MASS_ANTI_AIR, 1.10f},
            {UnitType::INDUSTRIAL_MBT, 0.85f},
            {UnitType::INDUSTRIAL_MISSILE_PLATFORM, 1.25f},
            {UnitType::INDUSTRIAL_ENGINEERING, 1.35f},
        };
        const char* configured_root = std::getenv("RTS_DATA_ROOT");
        const std::string path = std::string(configured_root != nullptr && configured_root[0] != '\0' ? configured_root : "data") + "/off_road.json";
        std::ifstream input(path);
        if (!input) return result;
        std::stringstream contents;
        contents << input.rdbuf();
        const auto parsed = data::JsonParser::parse(contents.str());
        if (!parsed || parsed->type() != data::JsonValue::Type::Object) return result;
        const auto number = [&parsed](const char* key, float fallback) {
            const auto value = parsed->get(key);
            return value && value->type() == data::JsonValue::Type::Number && std::isfinite(value->as_number())
                ? static_cast<float>(value->as_number()) : fallback;
        };
        result.wear_per_meter = std::max(0.0f, number("wear_per_meter", result.wear_per_meter));
        result.road_wear_multiplier = std::clamp(number("road_wear_multiplier", result.road_wear_multiplier), 0.0f, 1.0f);
        result.energy_per_meter = std::max(0.0f, number("energy_per_meter", result.energy_per_meter));
        result.material_per_meter = std::max(0.0f, number("material_per_meter", result.material_per_meter));
        result.speed_penalty_per_wear = std::max(0.0f, number("speed_penalty_per_wear", result.speed_penalty_per_wear));
        result.maximum_speed_penalty = std::clamp(number("maximum_speed_penalty", result.maximum_speed_penalty), 0.0f, 0.9f);
        if (const auto factors = parsed->get("unit_factors"); factors && factors->type() == data::JsonValue::Type::Object) {
            for (const auto& [name, value] : factors->as_object()) {
                const auto type = unit_type_from_name(name);
                if (type && value.type() == data::JsonValue::Type::Number && std::isfinite(value.as_number())) {
                    result.unit_factors[*type] = std::clamp(static_cast<float>(value.as_number()), 0.1f, 4.0f);
                }
            }
        }
        return result;
    }();
    return settings;
}

float off_road_unit_factor(UnitType unit_type) {
    const auto& factors = get_off_road_settings().unit_factors;
    const auto it = factors.find(unit_type);
    return it == factors.end() ? 1.0f : it->second;
}

} // namespace rts
