#include "mod_manifest.hpp"
#include "data/json_parser.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/faction.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cmath>

namespace rts {

std::optional<std::string> ModManifestLoader::read_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::optional<Dependency> ModManifestLoader::parse_dependency(const std::string& line) {
    std::regex dep_regex(R"(^\s*-\s*id:\s*["']?([^"'\s]+)["']?\s*version:\s*["']?([^"'\s]+)["']?\s*$)");
    std::smatch match;
    
    if (std::regex_search(line, match, dep_regex)) {
        Dependency dep;
        dep.id = match[1].str();
        dep.version_constraint = match[2].str();
        return dep;
    }
    
    return std::nullopt;
}

std::optional<ContentEntry> ModManifestLoader::parse_content_entry(const std::string& line) {
    std::regex entry_regex(R"(^\s*-\s*path:\s*["']?([^"'\s]+)["']?\s*replace:\s*(true|false)\s*$)");
    std::smatch match;
    
    if (std::regex_search(line, match, entry_regex)) {
        ContentEntry entry;
        entry.path = match[1].str();
        entry.replace = (match[2].str() == "true");
        return entry;
    }
    
    return std::nullopt;
}

bool ModManifestLoader::parse_version_constraint(const std::string& constraint, int& major, int& minor, int& patch) {
    std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;
    
    if (std::regex_search(constraint, match, version_regex)) {
        major = std::stoi(match[1].str());
        minor = std::stoi(match[2].str());
        patch = std::stoi(match[3].str());
        return true;
    }
    
    return false;
}

bool ModManifestLoader::check_version_constraint(const std::string& loaded_version, const std::string& constraint) {
    int loaded_major, loaded_minor, loaded_patch;
    int constraint_major, constraint_minor, constraint_patch;
    
    if (!parse_version_constraint(loaded_version, loaded_major, loaded_minor, loaded_patch)) {
        return false;
    }
    
    std::regex simple_regex(R"(^>=\s*(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;
    
    if (std::regex_search(constraint, match, simple_regex)) {
        constraint_major = std::stoi(match[1].str());
        constraint_minor = std::stoi(match[2].str());
        constraint_patch = std::stoi(match[3].str());
        
        if (loaded_major > constraint_major) return true;
        if (loaded_major < constraint_major) return false;
        if (loaded_minor > constraint_minor) return true;
        if (loaded_minor < constraint_minor) return false;
        return loaded_patch >= constraint_patch;
    }
    
    return false;
}

std::optional<ModManifest> ModManifestLoader::load_manifest(const fs::path& mod_dir) {
    fs::path manifest_path = mod_dir / "manifest.yaml";
    
    auto content_opt = read_file(manifest_path);
    if (!content_opt.has_value()) {
        return std::nullopt;
    }
    
    const auto& content = content_opt.value();
    ModManifest manifest;
    
    std::istringstream stream(content);
    std::string line;
    
    bool in_dependencies = false;
    bool in_units = false;
    bool in_factions = false;
    bool in_weapons = false;
    bool in_maps = false;
    bool in_scripts = false;
    bool in_balance = false;
    std::string current_balance_type;
    
    std::string current_dep_id;
    std::string current_dep_version;
    
    while (std::getline(stream, line)) {
        if (line.find("manifest_version:") == 0) {
            std::regex version_regex(R"(manifest_version:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, version_regex)) {
                manifest.manifest_version = match[1].str();
            }
        } else if (line.find("id:") == 0) {
            std::regex id_regex(R"(id:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, id_regex)) {
                manifest.id = match[1].str();
            }
        } else if (line.find("name:") == 0) {
            std::regex name_regex(R"(name:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, name_regex)) {
                manifest.name = match[1].str();
            }
        } else if (line.find("version:") == 0) {
            std::regex ver_regex(R"(version:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, ver_regex)) {
                manifest.version = match[1].str();
            }
        } else if (line.find("description:") == 0) {
            std::regex desc_regex(R"(description:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, desc_regex)) {
                manifest.description = match[1].str();
            }
        } else if (line.find("author:") == 0) {
            std::regex author_regex(R"(author:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, author_regex)) {
                manifest.author = match[1].str();
            }
        } else if (line.find("dependencies:") == 0) {
            in_dependencies = true;
            in_units = in_factions = in_weapons = in_maps = in_scripts = in_balance = false;
        } else if (line.find("units:") == 0) {
            in_units = true;
            in_dependencies = in_factions = in_weapons = in_maps = in_scripts = in_balance = false;
        } else if (line.find("factions:") == 0) {
            in_factions = true;
            in_dependencies = in_units = in_weapons = in_maps = in_scripts = in_balance = false;
        } else if (line.find("weapons:") == 0) {
            in_weapons = true;
            in_dependencies = in_units = in_factions = in_maps = in_scripts = in_balance = false;
        } else if (line.find("maps:") == 0) {
            in_maps = true;
            in_dependencies = in_units = in_factions = in_weapons = in_scripts = in_balance = false;
        } else if (line.find("scripts:") == 0) {
            in_scripts = true;
            in_dependencies = in_units = in_factions = in_weapons = in_maps = in_balance = false;
        } else if (line.find("balance:") == 0) {
            in_balance = true;
            in_dependencies = in_units = in_factions = in_weapons = in_maps = in_scripts = false;
        } else if (in_dependencies && line.find("  - id:") == 0) {
            std::regex dep_id_regex(R"(^\s*-\s*id:\s*["']?([^"'\s]+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, dep_id_regex)) {
                current_dep_id = match[1].str();
                current_dep_version.clear();
            }
        } else if (in_dependencies && !current_dep_id.empty() && line.find("    version:") == 0) {
            std::regex dep_ver_regex(R"(^\s*version:\s*["']?([^"'\s]+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, dep_ver_regex)) {
                current_dep_version = match[1].str();
                Dependency dep;
                dep.id = current_dep_id;
                dep.version_constraint = current_dep_version;
                manifest.dependencies.push_back(dep);
                current_dep_id.clear();
            }
        } else if ((in_units || in_factions || in_weapons || in_maps || in_scripts) && line.find("  - path:") == 0) {
            std::regex entry_regex(R"(^\s*-\s*path:\s*["']?([^"'\s]+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, entry_regex)) {
                ContentEntry entry;
                entry.path = match[1].str();
                
                std::string next_line;
                if (std::getline(stream, next_line)) {
                    std::regex replace_regex(R"(^\s*replace:\s*(true|false)\s*$)");
                    std::smatch replace_match;
                    if (std::regex_search(next_line, replace_match, replace_regex)) {
                        entry.replace = (replace_match[1].str() == "true");
                    }
                }
                
                if (in_units) manifest.units.push_back(entry);
                else if (in_factions) manifest.factions.push_back(entry);
                else if (in_weapons) manifest.weapons.push_back(entry);
                else if (in_maps) manifest.maps.push_back(entry);
                else if (in_scripts) manifest.scripts.push_back(entry);
            }
        } else if (in_balance && line.find("  units:") == 0) {
            current_balance_type = "units";
        } else if (in_balance && current_balance_type == "units" && line.find("    unit|") == 0) {
            size_t colon_pos = line.find(":");
            if (colon_pos != std::string::npos) {
                std::string content_id = line.substr(0, colon_pos);
                content_id.erase(0, content_id.find_first_not_of(" \t"));
                content_id.erase(content_id.find_last_not_of(" \t") + 1);
                
                size_t value_start = line.find_first_of("0123456789");
                if (value_start != std::string::npos) {
                    std::string value = line.substr(value_start);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    manifest.balance_overrides["units"][content_id] = value;
                }
            }
        }
    }
    
    return manifest;
}

bool ModManifestLoader::validate_manifest(const ModManifest& manifest) {
    if (manifest.id.empty()) {
        return false;
    }
    
    if (manifest.version.empty()) {
        return false;
    }
    
    if (manifest.manifest_version.empty()) {
        return false;
    }
    
    std::regex version_regex(R"(^\d+\.\d+(\.\d+)?$)");
    if (!std::regex_match(manifest.manifest_version, version_regex)) {
        return false;
    }
    
    if (!std::regex_match(manifest.version, version_regex)) {
        return false;
    }
    
    for (const auto& dep : manifest.dependencies) {
        if (dep.id.empty() || dep.version_constraint.empty()) {
            return false;
        }
    }
    
    return true;
}

bool ModManager::load_mod(const fs::path& mod_dir) {
    ModManifestLoader loader;
    auto manifest_opt = loader.load_manifest(mod_dir);
    
    if (!manifest_opt.has_value()) {
        load_errors_.push_back("Failed to load manifest from " + mod_dir.string());
        return false;
    }
    
    const auto& manifest = manifest_opt.value();
    
    if (!loader.validate_manifest(manifest)) {
        load_errors_.push_back("Manifest validation failed for " + mod_dir.string());
        return false;
    }
    
    if (loaded_mod_ids_.count(manifest.id) > 0) {
        load_errors_.push_back("Duplicate mod ID: " + manifest.id);
        return false;
    }

    for (const auto& dependency : manifest.dependencies) {
        const auto loaded = std::find_if(loaded_manifests_.begin(), loaded_manifests_.end(), [&](const auto& candidate) {
            return candidate.id == dependency.id;
        });
        if (loaded == loaded_manifests_.end()) {
            load_errors_.push_back("Missing dependency " + dependency.id + " required by " + manifest.id);
            return false;
        }
        if (!loader.check_version_constraint(loaded->version, dependency.version_constraint)) {
            load_errors_.push_back("Dependency version mismatch for " + dependency.id + " required by " + manifest.id);
            return false;
        }
    }
    
    std::vector<GeneratedUnitDefinition> loaded_units;
    for (const auto& entry : manifest.units) {
        auto generated_unit = load_generated_unit(manifest, mod_dir, entry);
        if (!generated_unit) {
            return false;
        }
        if (std::any_of(loaded_units.begin(), loaded_units.end(), [&](const auto& existing) {
                return existing.content.id == generated_unit->content.id;
            })) {
            load_errors_.push_back("Duplicate generated unit content ID: " + generated_unit->name);
            return false;
        }
        loaded_units.push_back(std::move(*generated_unit));
    }

    ModManifest full_manifest = manifest;
    
    loaded_manifests_.push_back(full_manifest);
    loaded_mod_ids_.insert(manifest.id);
    generated_units_.insert(generated_units_.end(), loaded_units.begin(), loaded_units.end());
    
    return true;
}

std::optional<GeneratedUnitDefinition> ModManager::load_generated_unit(
    const ModManifest& manifest, const fs::path& mod_dir, const ContentEntry& entry) {
    if (entry.path.empty() || entry.path.is_absolute() || entry.path.lexically_normal().string().starts_with("..")) {
        load_errors_.push_back("Invalid mod unit path: " + entry.path.string());
        return std::nullopt;
    }

    const fs::path source_path = mod_dir / entry.path;
    const auto raw = ModManifestLoader{}.read_file(source_path);
    if (!raw) {
        load_errors_.push_back("Missing mod unit definition: " + source_path.string());
        return std::nullopt;
    }
    const auto parsed = data::JsonParser::parse(*raw);
    if (!parsed || parsed->type() != data::JsonValue::Type::Object) {
        load_errors_.push_back("Invalid JSON mod unit definition: " + source_path.string());
        return std::nullopt;
    }

    const auto id = parsed->get("id");
    const auto health = parsed->get("health");
    const auto speed = parsed->get("speed");
    const auto view_range = parsed->get("view_range");
    const auto attack_range = parsed->get("attack_range");
    const auto attack_damage = parsed->get("attack_damage");
    const auto attack_cooldown = parsed->get("attack_cooldown");
    const auto movement_type = parsed->get("movement_type");
    const auto mesh = parsed->get("placeholder_mesh");
    if (!id || !health || !speed || !view_range || !attack_range || !attack_damage || !attack_cooldown || !movement_type || !mesh ||
        id->type() != data::JsonValue::Type::String || health->type() != data::JsonValue::Type::Number ||
        speed->type() != data::JsonValue::Type::Number || view_range->type() != data::JsonValue::Type::Number ||
        attack_range->type() != data::JsonValue::Type::Number || attack_damage->type() != data::JsonValue::Type::Number ||
        attack_cooldown->type() != data::JsonValue::Type::Number || movement_type->type() != data::JsonValue::Type::String ||
        mesh->type() != data::JsonValue::Type::String) {
        load_errors_.push_back("Generated unit schema is incomplete: " + source_path.string());
        return std::nullopt;
    }

    const fs::path mesh_relative = fs::path(mesh->as_string()).lexically_normal();
    if (mesh_relative.empty() || mesh_relative.is_absolute() || mesh_relative.string().starts_with("..")) {
        load_errors_.push_back("Invalid generated unit placeholder mesh path: " + mesh->as_string());
        return std::nullopt;
    }
    const fs::path mesh_path = source_path.parent_path() / mesh_relative;
    if (!fs::is_regular_file(mesh_path)) {
        load_errors_.push_back("Generated unit placeholder mesh is missing: " + mesh_path.string());
        return std::nullopt;
    }
    const auto mesh_raw = ModManifestLoader{}.read_file(mesh_path);
    const auto mesh_json = mesh_raw ? data::JsonParser::parse(*mesh_raw) : std::nullopt;
    const auto mesh_unit_id = mesh_json ? mesh_json->get("unit_id") : std::nullopt;
    if (!mesh_unit_id || mesh_unit_id->type() != data::JsonValue::Type::String || mesh_unit_id->as_string() != id->as_string()) {
        load_errors_.push_back("Generated unit placeholder mesh does not match definition: " + mesh_path.string());
        return std::nullopt;
    }

    const auto finite_positive = [](double value) { return std::isfinite(value) && value > 0.0; };
    if (!finite_positive(health->as_number()) || !finite_positive(speed->as_number()) || !finite_positive(view_range->as_number()) ||
        !finite_positive(attack_range->as_number()) || !finite_positive(attack_damage->as_number()) || !finite_positive(attack_cooldown->as_number())) {
        load_errors_.push_back("Generated unit numeric values must be finite and positive: " + source_path.string());
        return std::nullopt;
    }
    if (movement_type->as_string() != "ground" && movement_type->as_string() != "air" &&
        movement_type->as_string() != "naval") {
        load_errors_.push_back("Generated unit movement_type is unsupported: " + movement_type->as_string());
        return std::nullopt;
    }

    GeneratedUnitDefinition definition;
    definition.content = content_registry_.register_content("unit", manifest.id, id->as_string());
    if (content_registry_.has_collision(definition.content.id)) {
        load_errors_.push_back("Generated unit content ID collision: " + id->as_string());
        return std::nullopt;
    }
    definition.name = id->as_string();
    definition.source_path = source_path;
    definition.placeholder_mesh_path = mesh_path;
    definition.health = static_cast<float>(health->as_number());
    definition.speed = static_cast<float>(speed->as_number());
    definition.view_range = static_cast<float>(view_range->as_number());
    definition.attack_range = static_cast<float>(attack_range->as_number());
    definition.attack_damage = static_cast<float>(attack_damage->as_number());
    definition.attack_cooldown = static_cast<float>(attack_cooldown->as_number());
    definition.movement_type = movement_type->as_string();
    return definition;
}

Entity ModManager::spawn_generated_unit(Simulation& simulation, std::string_view content_id,
                                        float x, float y, FactionId faction_id) const {
    const auto found = std::find_if(generated_units_.begin(), generated_units_.end(), [&](const auto& unit) {
        return unit.content.id == content_id;
    });
    if (found == generated_units_.end()) return {};

    Entity entity = simulation.create_unit(x, y);
    if (entity.id == INVALID_ENTITY) return {};
    auto& components = simulation.component_manager();
    auto* health = components.get_component<Health>(entity.id);
    auto* weapon = components.get_component<Weapon>(entity.id);
    auto* faction = components.get_component<Faction>(entity.id);
    if (!health || !weapon || !faction) return {};
    *health = {found->health, found->health};
    *weapon = {found->attack_range, found->attack_damage, found->attack_cooldown, 0.0f,
               found->attack_range, 0.5f, 0.0f, 0.0f};
    *faction = {faction_id};
    components.add_component(entity.id, UnitData{found->speed, found->view_range});
    return entity;
}

bool ModManager::load_base_content(const fs::path& base_dir) {
    fs::path manifest_path = base_dir / "manifest.yaml";
    
    ModManifestLoader loader;
    auto manifest_opt = loader.load_manifest(manifest_path.parent_path());
    
    if (!manifest_opt.has_value()) {
        load_errors_.push_back("Failed to load base content manifest");
        return false;
    }
    
    const auto& manifest = manifest_opt.value();
    
    if (!loader.validate_manifest(manifest)) {
        load_errors_.push_back("Base content manifest validation failed");
        return false;
    }
    
    if (loaded_mod_ids_.count(manifest.id) > 0) {
        load_errors_.push_back("Duplicate mod ID for base content: " + manifest.id);
        return false;
    }
    
    loaded_manifests_.insert(loaded_manifests_.begin(), manifest);
    loaded_mod_ids_.insert(manifest.id);
    
    return true;
}

std::vector<std::string> ModManager::get_load_errors() const {
    return load_errors_;
}

bool ModManager::resolve_dependencies() {
    std::unordered_map<std::string, std::vector<std::string>> graph;
    std::unordered_map<std::string, int> in_degree;
    
    for (const auto& manifest : loaded_manifests_) {
        if (in_degree.find(manifest.id) == in_degree.end()) {
            in_degree[manifest.id] = 0;
        }
        
        for (const auto& dep : manifest.dependencies) {
            graph[dep.id].push_back(manifest.id);
            in_degree[manifest.id]++;
        }
    }
    
    std::vector<std::string> queue;
    for (const auto& [id, degree] : in_degree) {
        if (degree == 0) {
            queue.push_back(id);
        }
    }
    
    while (!queue.empty()) {
        std::string current = queue.back();
        queue.pop_back();
        
        for (const auto& neighbor : graph[current]) {
            if (--in_degree[neighbor] == 0) {
                queue.push_back(neighbor);
            }
        }
    }
    
    for (const auto& [id, degree] : in_degree) {
        if (degree > 0) {
            load_errors_.push_back("Circular dependency detected involving mod: " + id);
            return false;
        }
    }
    
    return true;
}

bool ModManager::topological_sort() {
    return resolve_dependencies();
}

} // namespace rts
