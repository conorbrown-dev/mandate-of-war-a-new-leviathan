#include <cmath>
#include "ecs/components/factions.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <optional>
#include <unordered_map>

#include "data/json_parser.hpp"

namespace rts {

namespace {

std::filesystem::path content_data_path(const char* filename) {
    if (const char* configured_root = std::getenv("RTS_DATA_ROOT");
        configured_root != nullptr && configured_root[0] != '\0') {
        return std::filesystem::path(configured_root) / filename;
    }
    return std::filesystem::path("data") / filename;
}

} // namespace

std::string faction_name(FactionId faction) {
    switch (faction) {
        case FactionId::ELITE_PRECISION: return "Elite Precision";
        case FactionId::MASS_WARFARE: return "Mass Warfare";
        case FactionId::INDUSTRIAL_EXPERIMENTAL: return "Industrial/Experimental";
        default: return "Unknown";
    }
}

static std::optional<UnitType> parse_unit_type(const std::string& name) {
    if (name == "ELITE_MAIN_BATTLE_TANK") return UnitType::ELITE_MAIN_BATTLE_TANK;
    if (name == "ELITE_LONG_RANGE_ARTILLERY") return UnitType::ELITE_LONG_RANGE_ARTILLERY;
    if (name == "ELITE_ANTI_AIR") return UnitType::ELITE_ANTI_AIR;
    if (name == "MASS_SWARM_TANK") return UnitType::MASS_SWARM_TANK;
    if (name == "MASS_ASSAULT_VEHICLE") return UnitType::MASS_ASSAULT_VEHICLE;
    if (name == "MASS_ANTI_AIR") return UnitType::MASS_ANTI_AIR;
    if (name == "INDUSTRIAL_MBT") return UnitType::INDUSTRIAL_MBT;
    if (name == "INDUSTRIAL_MISSILE_PLATFORM") return UnitType::INDUSTRIAL_MISSILE_PLATFORM;
    if (name == "INDUSTRIAL_ENGINEERING") return UnitType::INDUSTRIAL_ENGINEERING;
    if (name == "ELITE_PATROL_BOAT") return UnitType::ELITE_PATROL_BOAT;
    if (name == "ELITE_T1_FIGHTER") return UnitType::ELITE_T1_FIGHTER;
    if (name == "ELITE_T1_VTOL") return UnitType::ELITE_T1_VTOL;
    return std::nullopt;
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
    auto en_cost_opt = obj.get("energy_cost");
    auto res_cost_opt = obj.get("research_cost");
    auto build_time_opt = obj.get("build_time_seconds");
    auto hp_opt = obj.get("hp");
    auto speed_opt = obj.get("speed");
    auto range_opt = obj.get("range");
    auto view_opt = obj.get("view_range");
    auto visual_id_opt = obj.get("visual_id");

    if (!name_opt || !type_opt || !faction_opt || !mat_cost_opt || !en_cost_opt ||
        !res_cost_opt || !build_time_opt || !hp_opt || !speed_opt || !range_opt || !view_opt) {
        throw std::runtime_error("Missing required field in unit prototype");
    }

    auto type = parse_unit_type(type_opt.value().as_string());
    auto faction = parse_faction_id(faction_opt.value().as_string());

    if (!type || !faction) {
        throw std::runtime_error("Invalid unit type or faction in prototype");
    }

    UnitPrototype prototype{
        name_opt.value().as_string(),
        static_cast<float>(mat_cost_opt.value().as_number()),
        static_cast<float>(en_cost_opt.value().as_number()),
        static_cast<float>(res_cost_opt.value().as_number()),
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

static std::unordered_map<UnitType, UnitPrototype> load_unit_prototypes_from_json() {
    const std::filesystem::path data_path = content_data_path("unit_faction_stats.json");
    
     if (!std::filesystem::exists(data_path)) {
         std::cerr << "Warning: " << data_path << " not found, using default unit data\n";
          static std::unordered_map<UnitType, UnitPrototype> prototypes = {
              {UnitType::ELITE_MAIN_BATTLE_TANK, {"Elite MBT", 500.0f, 250.0f, 100.0f, 15.0f, 500.0f, 3.5f, 120.0f, 6.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION, {"advanced_targeting"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::ELITE_LONG_RANGE_ARTILLERY, {"Elite Artillery", 800.0f, 400.0f, 200.0f, 20.0f, 300.0f, 2.0f, 300.0f, 8.0f, UnitType::ELITE_LONG_RANGE_ARTILLERY, FactionId::ELITE_PRECISION, {"advance_ballistics"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::ELITE_ANTI_AIR, {"Elite AA Vehicle", 350.0f, 200.0f, 150.0f, 12.0f, 250.0f, 4.0f, 100.0f, 7.0f, UnitType::ELITE_ANTI_AIR, FactionId::ELITE_PRECISION, {"stealth_tech"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::ELITE_T1_FIGHTER, {"Elite T1 Fighter", 250.0f, 100.0f, 100.0f, 8.0f, 350.0f, 2.0f, 150.0f, 3.0f, UnitType::ELITE_T1_FIGHTER, FactionId::ELITE_PRECISION, {}, false, true, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::ELITE_T1_VTOL, {"Elite T1 VTOL", 300.0f, 120.0f, 80.0f, 10.0f, 400.0f, 2.5f, 120.0f, 3.5f, UnitType::ELITE_T1_VTOL, FactionId::ELITE_PRECISION, {"vtol_flight_systems"}, false, true, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::ELITE_PATROL_BOAT, {"Elite Patrol Boat", 200.0f, 80.0f, 50.0f, 10.0f, 200.0f, 2.5f, 80.0f, 5.0f, UnitType::ELITE_PATROL_BOAT, FactionId::ELITE_PRECISION, {}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::MASS_SWARM_TANK, {"Swarm Tank", 150.0f, 80.0f, 0.0f, 5.0f, 100.0f, 5.0f, 60.0f, 4.0f, UnitType::MASS_SWARM_TANK, FactionId::MASS_WARFARE, {"swarm_coordination"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::MASS_ASSAULT_VEHICLE, {"Assault Vehicle", 250.0f, 120.0f, 0.0f, 7.5f, 180.0f, 4.5f, 80.0f, 5.0f, UnitType::MASS_ASSAULT_VEHICLE, FactionId::MASS_WARFARE, {"assault_support"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::MASS_ANTI_AIR, {"Mass AA Vehicle", 180.0f, 100.0f, 0.0f, 6.0f, 150.0f, 5.5f, 80.0f, 5.0f, UnitType::MASS_ANTI_AIR, FactionId::MASS_WARFARE, {"rapid_response"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::INDUSTRIAL_MBT, {"Industrial MBT", 300.0f, 150.0f, 50.0f, 10.0f, 350.0f, 3.0f, 90.0f, 5.0f, UnitType::INDUSTRIAL_MBT, FactionId::INDUSTRIAL_EXPERIMENTAL, {"heavy_machinery"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::INDUSTRIAL_MISSILE_PLATFORM, {"Missile Platform", 500.0f, 300.0f, 150.0f, 15.0f, 250.0f, 2.5f, 200.0f, 6.0f, UnitType::INDUSTRIAL_MISSILE_PLATFORM, FactionId::INDUSTRIAL_EXPERIMENTAL, {"missile_systems"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}},
              {UnitType::INDUSTRIAL_ENGINEERING, {"Engineering Unit", 200.0f, 100.0f, 80.0f, 12.0f, 200.0f, 2.0f, 50.0f, 5.0f, UnitType::INDUSTRIAL_ENGINEERING, FactionId::INDUSTRIAL_EXPERIMENTAL, {"engineering_corps"}, false, false, false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, ""}}
          };
         return prototypes;
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

    auto unit_types = parsed.value().get("unit_types");
    if (!unit_types || unit_types.value().type() != rts::data::JsonValue::Type::Array) {
        throw std::runtime_error("Missing or invalid unit_types array");
    }

    std::unordered_map<UnitType, UnitPrototype> prototypes;
    for (size_t i = 0; i < unit_types.value().array_size(); i++) {
        auto proto_obj = unit_types.value().as_array()[i];
        if (proto_obj.type() != rts::data::JsonValue::Type::Object) {
            throw std::runtime_error("Invalid unit prototype at index " + std::to_string(i));
        }
        auto proto = parse_unit_prototype(proto_obj);
        prototypes[proto.type] = proto;
    }

    return prototypes;
}

const std::unordered_map<UnitType, UnitPrototype>& get_unit_prototypes() {
    static std::unordered_map<UnitType, UnitPrototype> prototypes = load_unit_prototypes_from_json();
    return prototypes;
}

const std::unordered_map<std::string, ResearchProject>& get_research_projects() {
    static std::unordered_map<std::string, ResearchProject> projects = load_research_projects_from_json();
    return projects;
}

const std::unordered_map<FactionId, FactionStartData>& get_faction_start_data() {
    static std::unordered_map<FactionId, FactionStartData> faction_data = {
        // Faction A: Elite Precision
        {FactionId::ELITE_PRECISION, {
            FactionId::ELITE_PRECISION,
            "Elite Precision",
            5000.0f,  // start material
            3000.0f,  // start energy
            1000.0f,  // start research
            {UnitType::ELITE_MAIN_BATTLE_TANK, UnitType::ELITE_LONG_RANGE_ARTILLERY, UnitType::ELITE_ANTI_AIR},
            0.10f,  // 10% metal production bonus
            0.15f,  // 15% energy production bonus
            0.10f   // 10% unit cost discount
        }},
        
        // Faction B: Mass Warfare
        {FactionId::MASS_WARFARE, {
            FactionId::MASS_WARFARE,
            "Mass Warfare",
            3000.0f,  // start material
            2000.0f,  // start energy
            500.0f,   // start research
            {UnitType::MASS_SWARM_TANK, UnitType::MASS_ASSAULT_VEHICLE, UnitType::MASS_ANTI_AIR},
            0.20f,  // 20% metal production bonus
            0.10f,  // 10% energy production bonus
            -0.20f  // -20% unit cost discount (cheaper production)
        }},
        
        // Faction C: Industrial/Experimental
        {FactionId::INDUSTRIAL_EXPERIMENTAL, {
            FactionId::INDUSTRIAL_EXPERIMENTAL,
            "Industrial/Experimental",
            4000.0f,  // start material
            2500.0f,  // start energy
            800.0f,   // start research
            {UnitType::INDUSTRIAL_MBT, UnitType::INDUSTRIAL_MISSILE_PLATFORM, UnitType::INDUSTRIAL_ENGINEERING},
            0.15f,  // 15% metal production bonus
            0.12f,  // 12% energy production bonus
            0.00f   // 0% unit cost discount
        }}
    };
    
    return faction_data;
}

} // namespace rts
