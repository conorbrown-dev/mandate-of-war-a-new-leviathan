#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <filesystem>
#include <string_view>

#include "content_id/content_id.hpp"
#include "ecs/entity.hpp"
#include "ecs/components/factions.hpp"

namespace fs = std::filesystem;

namespace rts {

class Simulation;

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

// A deliberately small development-time bridge from a generated unit file to
// the simulation. It keeps authored gameplay values data-driven without
// widening the fixed production UnitType enum before the mod schema is mature.
struct GeneratedUnitDefinition {
    ContentHandle content;
    std::string name;
    std::filesystem::path source_path;
    std::filesystem::path placeholder_mesh_path;
    float health = 0.0f;
    float speed = 0.0f;
    float view_range = 0.0f;
    float attack_range = 0.0f;
    float attack_damage = 0.0f;
    float attack_cooldown = 0.0f;
    std::string movement_type;
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
    bool load_mod_batch(const std::vector<fs::path>& mod_dirs);
    bool load_base_content(const fs::path& base_dir);
    std::vector<std::string> get_load_errors() const;
    const std::vector<ModManifest>& get_loaded_manifests() const { return loaded_manifests_; }
    const std::vector<GeneratedUnitDefinition>& generated_units() const { return generated_units_; }
    Entity spawn_generated_unit(Simulation& simulation, std::string_view content_id,
                                float x, float y, FactionId faction_id) const;
    
private:
    std::vector<ModManifest> loaded_manifests_;
    std::vector<std::string> load_errors_;
    std::unordered_set<std::string> loaded_mod_ids_;
    ContentRegistry content_registry_;
    std::vector<GeneratedUnitDefinition> generated_units_;
    
    bool resolve_dependencies();
    bool topological_sort();
    bool load_mod_batch_in_place(const std::vector<fs::path>& mod_dirs);
    std::optional<GeneratedUnitDefinition> load_generated_unit(
        const ModManifest& manifest, const fs::path& mod_dir, const ContentEntry& entry);
};

} // namespace rts
