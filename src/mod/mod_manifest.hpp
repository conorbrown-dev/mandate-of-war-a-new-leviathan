#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <filesystem>

namespace fs = std::filesystem;

namespace rts {

struct Dependency {
    std::string id;
    std::string version_constraint;
};

struct ContentEntry {
    std::filesystem::path path;
    bool replace;
};

struct ModManifest {
    std::string manifest_version;
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::vector<Dependency> dependencies;
    std::vector<ContentEntry> units;
    std::vector<ContentEntry> factions;
    std::vector<ContentEntry> weapons;
    std::vector<ContentEntry> maps;
    std::vector<ContentEntry> scripts;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> balance_overrides;
};

class ModManifestLoader {
public:
    ModManifestLoader() = default;
    
    std::optional<ModManifest> load_manifest(const fs::path& mod_dir);
    bool validate_manifest(const ModManifest& manifest);
    
    std::optional<std::string> read_file(const fs::path& path);
    std::optional<Dependency> parse_dependency(const std::string& line);
    std::optional<ContentEntry> parse_content_entry(const std::string& line);
    bool parse_version_constraint(const std::string& constraint, int& major, int& minor, int& patch);
    bool check_version_constraint(const std::string& loaded_version, const std::string& constraint);
};

class ModManager {
public:
    ModManager() = default;
    
    bool load_mod(const fs::path& mod_dir);
    bool load_base_content(const fs::path& base_dir);
    std::vector<std::string> get_load_errors() const;
    const std::vector<ModManifest>& get_loaded_manifests() const { return loaded_manifests_; }
    
private:
    std::vector<ModManifest> loaded_manifests_;
    std::vector<std::string> load_errors_;
    std::unordered_set<std::string> loaded_mod_ids_;
    
    bool resolve_dependencies();
    bool topological_sort();
};

} // namespace rts
