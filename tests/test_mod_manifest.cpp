#include "test_framework.hpp"
#include "mod/mod_manifest.hpp"
#include "content_id/content_id.hpp"
#include "simulation/simulation.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST(mod_manifest_load_valid) {
    fs::path test_dir = fs::current_path() / "test_mods" / "test_mod_1";
    rts::ModManifestLoader loader;
    auto manifest_opt = loader.load_manifest(test_dir);
    
    if (!manifest_opt.has_value()) {
        throw std::runtime_error("Failed to load manifest");
    }
    
    const auto& manifest = manifest_opt.value();
    
    if (manifest.manifest_version != "1.0") {
        throw std::runtime_error("Wrong manifest version");
    }
    
    if (manifest.id != "test_mod_1") {
        throw std::runtime_error("Wrong mod ID");
    }
    
    if (manifest.name != "Test Mod 1") {
        throw std::runtime_error("Wrong mod name");
    }
    
    if (manifest.version != "1.2.3") {
        throw std::runtime_error("Wrong mod version");
    }
    
    if (manifest.dependencies.size() != 1) {
        throw std::runtime_error("Wrong dependency count");
    }
    
    if (manifest.dependencies[0].id != "base_content") {
        throw std::runtime_error("Wrong dependency ID");
    }
    
    if (manifest.dependencies[0].version_constraint != ">=1.0.0") {
        throw std::runtime_error("Wrong dependency version constraint");
    }
    
    if (manifest.units.size() != 1) {
        throw std::runtime_error("Wrong unit count");
    }
}

TEST(mod_manifest_validate) {
    rts::ModManifestLoader loader;
    
    rts::ModManifest valid_manifest;
    valid_manifest.manifest_version = "1.0";
    valid_manifest.id = "test_mod";
    valid_manifest.version = "1.0.0";
    
    if (!loader.validate_manifest(valid_manifest)) {
        throw std::runtime_error("Valid manifest should pass validation");
    }
    
    rts::ModManifest invalid_manifest;
    invalid_manifest.manifest_version = "1.0";
    invalid_manifest.version = "1.0.0";
    
    if (loader.validate_manifest(invalid_manifest)) {
        throw std::runtime_error("Invalid manifest should fail validation");
    }
}

TEST(mod_manifest_parse_dependency) {
    rts::ModManifestLoader loader;
    auto dep = loader.parse_dependency("- id: \"base_content\" version: \">=1.0.0\"");
    
    if (!dep.has_value()) {
        throw std::runtime_error("Failed to parse dependency");
    }
    
    if (dep->id != "base_content") {
        throw std::runtime_error("Wrong dependency ID");
    }
    
    if (dep->version_constraint != ">=1.0.0") {
        throw std::runtime_error("Wrong version constraint");
    }
}

TEST(mod_manifest_parse_content_entry) {
    rts::ModManifestLoader loader;
    auto entry = loader.parse_content_entry("- path: \"units/test_unit.json\" replace: false");
    
    if (!entry.has_value()) {
        throw std::runtime_error("Failed to parse content entry");
    }
    
    if (entry->path.string() != "units/test_unit.json") {
        throw std::runtime_error("Wrong path");
    }
    
    if (entry->replace != false) {
        throw std::runtime_error("Wrong replace flag");
    }
}

TEST(mod_manifest_version_check) {
    rts::ModManifestLoader loader;
    
    if (!loader.check_version_constraint("1.2.3", ">=1.0.0")) {
        throw std::runtime_error("Version 1.2.3 should satisfy >=1.0.0");
    }
    
    if (loader.check_version_constraint("0.9.0", ">=1.0.0")) {
        throw std::runtime_error("Version 0.9.0 should not satisfy >=1.0.0");
    }
}

TEST(mod_manifest_version_resolution) {
    rts::ModManifestLoader loader;
    
    if (!loader.check_version_constraint("1.2.3", ">=1.0.0")) {
        throw std::runtime_error("1.2.3 should satisfy >=1.0.0");
    }
    
    if (loader.check_version_constraint("0.9.0", ">=1.0.0")) {
        throw std::runtime_error("0.9.0 should not satisfy >=1.0.0");
    }
    
    if (!loader.check_version_constraint("1.0.0", ">=1.0.0")) {
        throw std::runtime_error("1.0.0 should satisfy >=1.0.0");
    }
    
    if (!loader.check_version_constraint("1.0.5", ">=1.0.0")) {
        throw std::runtime_error("1.0.5 should satisfy >=1.0.0");
    }
}

TEST(mod_manifest_dependency_resolution) {
    rts::ModManifestLoader loader;
    
    rts::ModManifest manifest;
    manifest.id = "dependent_mod";
    manifest.version = "1.0.0";
    manifest.manifest_version = "1.0";
    
    rts::Dependency dep;
    dep.id = "base_mod";
    dep.version_constraint = ">=1.0.0";
    manifest.dependencies.push_back(dep);
    
    if (!loader.validate_manifest(manifest)) {
        throw std::runtime_error("Manifest with dependencies should pass validation");
    }
    
    rts::Dependency invalid_dep;
    invalid_dep.id = "";
    invalid_dep.version_constraint = ">=1.0.0";
    rts::ModManifest invalid_manifest;
    invalid_manifest.id = "bad_mod";
    invalid_manifest.version = "1.0.0";
    invalid_manifest.manifest_version = "1.0";
    invalid_manifest.dependencies.push_back(invalid_dep);
    
    if (loader.validate_manifest(invalid_manifest)) {
        throw std::runtime_error("Manifest with empty dependency ID should fail validation");
    }
}

TEST(mod_manifest_load_mod) {
    fs::path test_dir = fs::current_path() / "test_mods" / "test_mod_1";
    rts::ModManager manager;
    
    if (!manager.load_mod(test_dir)) {
        throw std::runtime_error("Failed to load mod");
    }
    
    const auto& manifests = manager.get_loaded_manifests();
    if (manifests.size() != 1) {
        throw std::runtime_error("Expected 1 loaded manifest");
    }
    
    if (manifests[0].id != "test_mod_1") {
        throw std::runtime_error("Wrong mod ID in loaded manifests");
    }
    
    if (manifests[0].units.size() != 1) {
        throw std::runtime_error("Wrong unit count in manifest");
    }
    
    if (manager.generated_units().size() != 1) {
        throw std::runtime_error("Expected one validated generated unit");
    }
    const auto& generated = manager.generated_units().front();
    if (generated.name != "test_mod_1_t1_heavy" || !fs::is_regular_file(generated.placeholder_mesh_path)) {
        throw std::runtime_error("Generated unit metadata was not loaded");
    }

    rts::Simulation simulation;
    const auto spawned = manager.spawn_generated_unit(simulation, generated.content.id, 0.0f, 0.0f,
                                                      rts::FactionId::MASS_WARFARE);
    if (!spawned.valid()) {
        throw std::runtime_error("Validated generated unit must be spawnable");
    }
    float health = 0.0f;
    float max_health = 0.0f;
    rts::FactionId faction = rts::FactionId::ELITE_PRECISION;
    if (!simulation.get_unit_health(spawned.id, health, max_health) || health != 150.0f || max_health != 150.0f ||
        !simulation.get_unit_faction_id(spawned.id, faction) || faction != rts::FactionId::MASS_WARFARE) {
        throw std::runtime_error("Spawned generated unit did not receive authored runtime data");
    }
}
