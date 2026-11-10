#include <cmath>
#include "ecs/components/factions.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <optional>
#include <unordered_map>

#include "data/json_parser.hpp"
#include "util/environment.hpp"

namespace rts {

namespace {

std::filesystem::path content_data_path(const char* filename) {
    if (const std::string configured_root = read_environment("RTS_DATA_ROOT"); !configured_root.empty()) {
        return std::filesystem::path(configured_root) / filename;
    }
    return std::filesystem::path("data") / filename;
}

} // namespace

std::string faction_name(FactionId faction) {
    const auto& factions = get_faction_start_data();
    const auto found = factions.find(faction);
    return found == factions.end() ? "Unknown" : found->second.name;
}

static std::optional<UnitType> parse_unit_type(const std::string& name) {
    return unit_type_from_key(name);
}

static std::optional<FactionId> parse_faction_id(const std::string& name) {
    if (name == "ELITE_PRECISION") return FactionId::ELITE_PRECISION;
    if (name == "MASS_WARFARE") return FactionId::MASS_WARFARE;
    if (name == "INDUSTRIAL_EXPERIMENTAL") return FactionId::INDUSTRIAL_EXPERIMENTAL;
    return std::nullopt;
}

static std::optional<SeizureCapability> parse_seizure_capability(const std::string& name) {
    if (name == "RECON") return SeizureCapability::RECON;
    if (name == "SEIZURE") return SeizureCapability::SEIZURE;
    if (name == "SECURE") return SeizureCapability::SECURE;
    if (name == "CONSTRUCT_FOB") return SeizureCapability::CONSTRUCT_FOB;
    if (name == "CONSTRUCT_LOGISTICS") return SeizureCapability::CONSTRUCT_LOGISTICS;
    if (name == "ESTABLISH_BASE") return SeizureCapability::ESTABLISH_BASE;
    if (name == "DEFEND") return SeizureCapability::DEFEND;
    if (name == "HARVEST_SECURED") return SeizureCapability::HARVEST_SECURED;
    return std::nullopt;
}

static std::unordered_map<std::string, ResearchProject> load_research_projects_from_json();

static UnitPrototype parse_unit_prototype(const rts::data::JsonValue& obj) {
    auto name_opt = obj.get("name");
    auto type_opt = obj.get("type");
    auto faction_opt = obj.get("faction");
    auto mat_cost_opt = obj.get("material_cost");
    auto build_time_opt = obj.get("build_time_seconds");
    auto hp_opt = obj.get("hp");
    auto speed_opt = obj.get("speed");
    auto range_opt = obj.get("range");
    auto view_opt = obj.get("view_range");
    auto visual_id_opt = obj.get("visual_id");

    if (!name_opt || !type_opt || !faction_opt || !mat_cost_opt || !build_time_opt ||
        !hp_opt || !speed_opt || !range_opt || !view_opt) {
        throw std::runtime_error("Missing required field in unit prototype");
    }

    const auto id_opt = obj.get("id");
    if (!id_opt || id_opt->type() != rts::data::JsonValue::Type::Number ||
        id_opt->as_number() < 0.0 || id_opt->as_number() > 255.0 ||
        id_opt->as_number() != std::floor(id_opt->as_number())) {
        throw std::runtime_error("Unit prototype needs an integer id from 0 to 255");
    }
    const std::optional<UnitType> type = static_cast<UnitType>(static_cast<int>(id_opt->as_number()));
    auto faction = parse_faction_id(faction_opt.value().as_string());

    if (!faction) {
        throw std::runtime_error("Invalid unit type or faction in prototype");
    }

    UnitPrototype prototype{
        name_opt.value().as_string(),
        static_cast<float>(mat_cost_opt.value().as_number()),
        static_cast<float>(build_time_opt.value().as_number()),
        static_cast<float>(hp_opt.value().as_number()),
        static_cast<float>(speed_opt.value().as_number()),
        static_cast<float>(range_opt.value().as_number()),
        static_cast<float>(view_opt.value().as_number()),
        type.value(),
        faction.value(),
        {}
    };
    if (visual_id_opt && visual_id_opt->type() == rts::data::JsonValue::Type::String) {
        prototype.visual_id = visual_id_opt->as_string();
    }
    prototype.key = type_opt.value().as_string();
    if (auto nato = obj.get("nato_symbol"); nato && nato->type() == rts::data::JsonValue::Type::String) {
        prototype.nato_symbol = nato->as_string();
    }
    if (auto icon = obj.get("strategic_icon"); icon && icon->type() == rts::data::JsonValue::Type::String) {
        prototype.strategic_icon = icon->as_string();
    }
    if (auto construction = obj.get("construction_unit"); construction && construction->type() == rts::data::JsonValue::Type::Bool) {
        prototype.construction_unit = construction->as_bool();
    }
    if (auto cargo = obj.get("cargo_transport"); cargo) {
        if (cargo->type() != rts::data::JsonValue::Type::Bool) {
            throw std::runtime_error("Unit prototype " + prototype.key + " has a non-boolean cargo_transport");
        }
        prototype.cargo_transport = cargo->as_bool();
    }
    if (auto airdrop = obj.get("airdrop"); airdrop) {
        if (airdrop->type() != rts::data::JsonValue::Type::Bool) {
            throw std::runtime_error("Unit prototype " + prototype.key + " has a non-boolean airdrop");
        }
        prototype.airdrop = airdrop->as_bool();
    }
    if (auto burn = obj.get("fuel_per_km"); burn) {
        if (burn->type() != rts::data::JsonValue::Type::Number || !std::isfinite(burn->as_number()) || burn->as_number() < 0.0) {
            throw std::runtime_error("Unit prototype " + prototype.key + " has an invalid fuel_per_km");
        }
        prototype.fuel_per_km = static_cast<float>(burn->as_number());
    }
    if (const auto intel = obj.get("intel_range"); intel) {
        if (intel->type() != rts::data::JsonValue::Type::Number ||
            !std::isfinite(intel->as_number()) || intel->as_number() < 0.0 || intel->as_number() > 10000.0)
            throw std::runtime_error("Unit prototype " + prototype.key + " has an invalid intel_range");
        prototype.intel_range = static_cast<float>(intel->as_number());
    }
    if (const auto footprint = obj.get("footprint_radius"); footprint) {
        if (footprint->type() != rts::data::JsonValue::Type::Number ||
            !std::isfinite(footprint->as_number()) || footprint->as_number() < 0.5 || footprint->as_number() > 50.0)
            throw std::runtime_error("Unit prototype " + prototype.key + " has an invalid footprint_radius");
        prototype.footprint_radius = static_cast<float>(footprint->as_number());
    }
    const auto weapon = obj.get("weapon");
    if (!weapon || weapon->type() != rts::data::JsonValue::Type::Object) {
        throw std::runtime_error("Unit prototype " + prototype.key + " has no weapon");
    }
    const auto weapon_number = [&](const char* field) {
        const auto value = weapon->get(field);
        if (!value || value->type() != rts::data::JsonValue::Type::Number ||
            !std::isfinite(value->as_number()) || value->as_number() < 0.0) {
            throw std::runtime_error("Unit prototype " + prototype.key + " has an invalid weapon " + field);
        }
        return static_cast<float>(value->as_number());
    };
    prototype.weapon = {weapon_number("damage"), weapon_number("range"), weapon_number("fire_rate"),
                        weapon_number("projectile_speed"), weapon_number("aoe_radius"), weapon_number("lead_bias")};
    // "targets": which domains the weapon can engage, as real systems do. A
    // tank gun cannot engage a jet; a surface-to-air missile cannot engage a tank.
    if (const auto targets = weapon->get("targets"); targets) {
        if (targets->type() != rts::data::JsonValue::Type::Array) {
            throw std::runtime_error("Unit prototype " + prototype.key + " has non-list weapon targets");
        }
        prototype.weapon.hits_ground = false;
        prototype.weapon.hits_air = false;
        for (const auto& target : targets->as_array()) {
            const std::string domain = target.type() == rts::data::JsonValue::Type::String ? target.as_string() : std::string();
            if (domain == "ground") prototype.weapon.hits_ground = true;
            else if (domain == "air") prototype.weapon.hits_air = true;
            else throw std::runtime_error("Unit prototype " + prototype.key + " has an unknown weapon target");
        }
    }

    if (auto content_id_opt = obj.get("content_id"); content_id_opt && content_id_opt->type() == rts::data::JsonValue::Type::String) {
        prototype.content_id = content_id_opt->as_string();
    }

    if (auto capabilities = obj.get("capabilities");
        capabilities && capabilities->type() == rts::data::JsonValue::Type::Array) {
        for (const auto& capability : capabilities->as_array()) {
            auto parsed_capability = parse_seizure_capability(capability.as_string());
            if (!parsed_capability) {
                throw std::runtime_error("Invalid seizure capability in unit prototype");
            }
            prototype.capabilities.push_back(*parsed_capability);
        }
    }

    if (auto prerequisites = obj.get("research_prerequisites");
        prerequisites && prerequisites->type() == rts::data::JsonValue::Type::Array) {
        for (const auto& prerequisite : prerequisites->as_array()) {
            prototype.research_prerequisites.push_back(prerequisite.as_string());
        }
    }

    if (auto is_aircraft = obj.get("is_aircraft"); is_aircraft && is_aircraft->as_bool()) {
        auto requires_runway = obj.get("requires_runway");
        auto payload = obj.get("payload");
        auto operational_range = obj.get("operational_range");
        auto operational_energy = obj.get("operational_energy");
        auto operational_material = obj.get("operational_material");
        auto max_airborne_time = obj.get("max_airborne_time_seconds");
        auto energy_rate = obj.get("energy_consumption_rate");
        auto material_rate = obj.get("material_consumption_rate");
        if (!requires_runway || !payload || !operational_range || !operational_energy || !operational_material ||
            !max_airborne_time || !energy_rate || !material_rate) {
            throw std::runtime_error("Missing required aircraft field in unit prototype");
        }
        prototype.is_aircraft = true;
        prototype.requires_runway = requires_runway->as_bool();
        prototype.payload = payload->as_number();
        prototype.operational_range = operational_range->as_number();
        prototype.operational_energy = operational_energy->as_number();
        prototype.operational_material = operational_material->as_number();
        prototype.max_airborne_time_seconds = max_airborne_time->as_number();
        prototype.energy_consumption_rate = energy_rate->as_number();
        prototype.material_consumption_rate = material_rate->as_number();
        if (auto loiter = obj.get("recon_loiter_seconds"); loiter) {
            if (loiter->type() != rts::data::JsonValue::Type::Number || !std::isfinite(loiter->as_number()) || loiter->as_number() <= 0.0) {
                throw std::runtime_error("Invalid recon_loiter_seconds in unit prototype");
            }
            prototype.recon_loiter_seconds = static_cast<float>(loiter->as_number());
        }
        if (prototype.payload <= 0.0f || prototype.operational_range <= 0.0f || prototype.operational_energy <= 0.0f ||
            prototype.operational_material <= 0.0f || prototype.max_airborne_time_seconds <= 0.0f ||
            prototype.energy_consumption_rate <= 0.0f || prototype.material_consumption_rate < 0.0f) {
            throw std::runtime_error("Invalid aircraft field in unit prototype");
        }
    }
    if (auto naval = obj.get("is_naval"); naval && naval->as_bool()) {
        prototype.is_naval = true;
        prototype.operational_energy = obj.get("operational_energy").value().as_number();
        prototype.energy_consumption_rate = obj.get("energy_consumption_rate").value().as_number();
        if (!std::isfinite(prototype.operational_energy) || prototype.operational_energy<=0 ||
            !std::isfinite(prototype.energy_consumption_rate) || prototype.energy_consumption_rate<=0)
            throw std::runtime_error("Invalid naval endurance data");
    }
    if (!prototype.is_aircraft && !prototype.is_naval) {
        auto steering = obj.get("steering");
        if (!steering || steering->type() != rts::data::JsonValue::Type::Object) {
            throw std::runtime_error("Missing steering object in ground unit prototype");
        }
        const auto required_number = [&](const char* key) {
            auto value = steering->get(key);
            if (!value || value->type() != rts::data::JsonValue::Type::Number ||
                !std::isfinite(value->as_number()) || value->as_number() <= 0.0) {
                throw std::runtime_error(std::string("Invalid ground steering field: ") + key);
            }
            return static_cast<float>(value->as_number());
        };
        prototype.steering_acceleration = required_number("acceleration");
        prototype.steering_deceleration = required_number("deceleration");
        prototype.steering_turn_rate = required_number("turn_rate");
        prototype.steering_turn_rate_at_speed = required_number("turn_rate_at_speed");
        prototype.steering_minimum_turn_radius = required_number("minimum_turn_radius");
        prototype.steering_max_reverse_speed = required_number("max_reverse_speed");
        prototype.steering_reverse_preference_threshold = required_number("reverse_preference_threshold");
        prototype.steering_response = required_number("steering_response");
        if (auto maneuver_turn_rate = steering->get("maneuver_turn_rate"); maneuver_turn_rate) {
            if (maneuver_turn_rate->type() != rts::data::JsonValue::Type::Number ||
                !std::isfinite(maneuver_turn_rate->as_number()) || maneuver_turn_rate->as_number() < 0.0) {
                throw std::runtime_error("Invalid ground steering field: maneuver_turn_rate");
            }
            prototype.steering_maneuver_turn_rate = static_cast<float>(maneuver_turn_rate->as_number());
        }
        auto pivot = steering->get("can_pivot_turn");
        if (!pivot || pivot->type() != rts::data::JsonValue::Type::Bool) {
            throw std::runtime_error("Invalid ground steering field: can_pivot_turn");
        }
        prototype.steering_can_pivot_turn = pivot->as_bool();
    }
    return prototype;
}

static std::unordered_map<std::string, ResearchProject> load_research_projects_from_json() {
    const std::filesystem::path data_path = content_data_path("research_projects.json");
    
    if (!std::filesystem::exists(data_path)) {
        std::cerr << "Warning: " << data_path << " not found, using empty research data\n";
        static std::unordered_map<std::string, ResearchProject> empty_projects;
        return empty_projects;
    }

    std::ifstream file(data_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open " + data_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto json_str = buffer.str();
    file.close();

    auto parsed = rts::data::JsonParser::parse(json_str);
    if (!parsed) {
        throw std::runtime_error("Failed to parse JSON from " + data_path.string());
    }
    
    auto projects_opt = parsed.value().get("projects");
    if (!projects_opt || projects_opt.value().type() != rts::data::JsonValue::Type::Array) {
        throw std::runtime_error("research_projects.json: 'projects' field must be an array");
    }
    
    std::unordered_map<std::string, ResearchProject> projects;
    auto& projects_array = projects_opt.value().as_array();
    
    for (const auto& project_obj : projects_array) {
        if (project_obj.type() != rts::data::JsonValue::Type::Object) {
            throw std::runtime_error("research_projects.json: each project must be an object");
        }
        
        auto id_opt = project_obj.get("id");
        auto name_opt = project_obj.get("name");
        auto cost_per_tick_opt = project_obj.get("cost_per_tick");
        auto total_cost_opt = project_obj.get("total_cost");
        auto build_time_opt = project_obj.get("build_time_seconds");
        
        if (!id_opt || !name_opt || !cost_per_tick_opt || !total_cost_opt || !build_time_opt) {
            throw std::runtime_error("research_projects.json: missing required fields in project");
        }
        
        ResearchProject project;
        project.id = id_opt.value().as_string();
        project.name = name_opt.value().as_string();
        project.cost_per_tick = cost_per_tick_opt.value().as_number();
        project.total_cost = total_cost_opt.value().as_number();
        project.build_time_seconds = build_time_opt.value().as_number();
        
        auto prereq_opt = project_obj.get("prerequisites");
        if (prereq_opt && prereq_opt.value().type() == rts::data::JsonValue::Type::Array) {
            for (const auto& prereq : prereq_opt.value().as_array()) {
                project.prerequisites.push_back(prereq.as_string());
            }
        }
        
        auto unlocks_opt = project_obj.get("unlocks");
        if (unlocks_opt && unlocks_opt.value().type() == rts::data::JsonValue::Type::Array) {
            for (const auto& unlock : unlocks_opt.value().as_array()) {
                auto unit_type = parse_unit_type(unlock.as_string());
                if (unit_type) {
                    project.unlocks.push_back(unit_type.value());
                }
            }
        }
        
        project.completed = false;
        projects[project.id] = project;
    }
    
    return projects;
}

static std::string read_required_content(const char* filename) {
    const std::filesystem::path data_path = content_data_path(filename);
    std::ifstream file(data_path);
    if (!file.is_open()) {
        throw std::runtime_error("Required game data is missing: " + data_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::unordered_map<UnitType, UnitPrototype> parse_unit_catalog(const std::string& json) {
    auto parsed = rts::data::JsonParser::parse(json);
    if (!parsed) {
        throw std::runtime_error("Unit data is not valid JSON");
    }

    auto unit_types = parsed.value().get("unit_types");
    if (!unit_types || unit_types.value().type() != rts::data::JsonValue::Type::Array) {
        throw std::runtime_error("Missing or invalid unit_types array");
    }

    std::unordered_map<UnitType, UnitPrototype> prototypes;
    std::unordered_set<std::string> keys;
    for (size_t i = 0; i < unit_types.value().array_size(); i++) {
        const auto& proto_obj = unit_types.value().as_array()[i];
        if (proto_obj.type() != rts::data::JsonValue::Type::Object) {
            throw std::runtime_error("Invalid unit prototype at index " + std::to_string(i));
        }
        auto proto = parse_unit_prototype(proto_obj);
        if (prototypes.count(proto.type) > 0 || !keys.insert(proto.key).second) {
            throw std::runtime_error("Duplicate unit id or key: " + proto.key);
        }
        prototypes[proto.type] = proto;
    }

    return prototypes;
}

static std::unordered_map<FactionId, FactionStartData> load_faction_start_data() {
    const auto parsed = rts::data::JsonParser::parse(read_required_content("unit_faction_stats.json"));
    const auto factions = parsed ? parsed->get("faction_start_data") : std::nullopt;
    if (!factions || factions->type() != rts::data::JsonValue::Type::Array) {
        throw std::runtime_error("Missing or invalid faction_start_data array");
    }
    std::unordered_map<FactionId, FactionStartData> result;
    for (const auto& entry : factions->as_array()) {
        const auto require = [&](const char* field) {
            const auto value = entry.get(field);
            if (!value) throw std::runtime_error(std::string("Faction data is missing ") + field);
            return *value;
        };
        const auto id = parse_faction_id(require("faction_id").as_string());
        if (!id) throw std::runtime_error("Unknown faction id in faction_start_data");
        FactionStartData start{};
        start.faction_id = *id;
        start.name = require("name").as_string();
        start.start_material = static_cast<float>(require("start_material").as_number());
        start.start_energy = static_cast<float>(require("start_energy").as_number());
        const auto start_units = require("start_units");
        for (const auto& unit : start_units.as_array()) {
            const auto type = unit_type_from_key(unit.as_string());
            if (!type) throw std::runtime_error("Unknown start unit: " + unit.as_string());
            start.start_units.push_back(*type);
        }
        start.production_bonus_metal = static_cast<float>(require("production_bonus_metal").as_number());
        start.production_bonus_energy = static_cast<float>(require("production_bonus_energy").as_number());
        start.unit_cost_discount = static_cast<float>(require("unit_cost_discount").as_number());
        result[*id] = start;
    }
    return result;
}

const std::unordered_map<UnitType, UnitPrototype>& get_unit_prototypes() {
    static std::unordered_map<UnitType, UnitPrototype> prototypes =
        parse_unit_catalog(read_required_content("unit_faction_stats.json"));
    return prototypes;
}

std::optional<UnitType> unit_type_from_key(const std::string& key) {
    for (const auto& [type, prototype] : get_unit_prototypes()) {
        if (prototype.key == key) return type;
    }
    return std::nullopt;
}

const std::unordered_map<std::string, ResearchProject>& get_research_projects() {
    static std::unordered_map<std::string, ResearchProject> projects = load_research_projects_from_json();
    return projects;
}

std::string read_game_data(const char* filename) { return read_required_content(filename); }

const std::unordered_map<FactionId, FactionStartData>& get_faction_start_data() {
    static const std::unordered_map<FactionId, FactionStartData> faction_data = load_faction_start_data();
    return faction_data;
}

} // namespace rts
