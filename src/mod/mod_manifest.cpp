#include "mod_manifest.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

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
    
    ModManifest full_manifest = manifest;
    full_manifest.id = manifest.id;
    
    loaded_manifests_.push_back(full_manifest);
    loaded_mod_ids_.insert(manifest.id);
    
    return true;
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
