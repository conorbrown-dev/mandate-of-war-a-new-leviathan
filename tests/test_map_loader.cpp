#include "test_framework.hpp"
#include "map/map_loader.hpp"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <unistd.h>

namespace {
const std::filesystem::path test_root = std::filesystem::temp_directory_path() / ("rts-map-tests-" + std::to_string(getpid()));
}

namespace fs = std::filesystem;

TEST(map_loader_load_empty_header) {
    fs::path test_dir = test_root;
    fs::create_directories(test_dir);
    
    fs::path map_path = test_dir / "test_map.map";
    std::ofstream map_file(map_path);
    map_file << "map_version: \"1.0\"\n";
    map_file << "id: \"test_map\"\n";
    map_file << "name: \"Test Map\"\n";
    map_file << "dimensions:\n";
    map_file << "  width: 1024\n";
    map_file << "  height: 1024\n";
    map_file << "  tile_size: 16\n";
    map_file << "  max_elevation: 512\n";
    map_file.close();
    
    rts::MapLoader loader;
    
    std::ofstream terrain_file(test_dir / "terrain.bin", std::ios::binary);
    uint32_t tiles_x = 1024 / 16;
    uint32_t tiles_y = 1024 / 16;
    std::vector<float> heights(tiles_x * tiles_y, 0.0f);
    terrain_file.write(reinterpret_cast<const char*>(heights.data()), heights.size() * sizeof(float));
    terrain_file.close();
    
    auto map_opt = loader.load_map(map_path);
    
    if (!map_opt.has_value()) {
        throw std::runtime_error("Failed to load map");
    }
    
    const auto& map = map_opt.value();
    
    if (map.id != "test_map") {
        throw std::runtime_error("Wrong map ID");
    }
    
    if (map.width != 1024 || map.height != 1024) {
        throw std::runtime_error("Wrong map dimensions");
    }
    
    if (map.tile_size != 16.0f) {
        throw std::runtime_error("Wrong tile size");
    }
}

TEST(map_loader_validate) {
    rts::MapLoader loader;
    
    rts::MapData valid_map;
    valid_map.id = "test_map";
    valid_map.width = 1024;
    valid_map.height = 1024;
    valid_map.tile_size = 16.0f;
    valid_map.tiles_x = 64;
    valid_map.tiles_y = 64;
    valid_map.terrain_heights.resize(64 * 64, 0.0f);
    
    if (!loader.validate_map(valid_map)) {
        throw std::runtime_error("Valid map should pass validation");
    }
    
    rts::MapData invalid_map;
    invalid_map.id = "";
    invalid_map.width = 1024;
    invalid_map.height = 1024;
    invalid_map.tile_size = 16.0f;
    invalid_map.tiles_x = 64;
    invalid_map.tiles_y = 64;
    invalid_map.terrain_heights.resize(64 * 64, 0.0f);
    
    if (loader.validate_map(invalid_map)) {
        throw std::runtime_error("Invalid map should fail validation");
    }
}

TEST(map_loader_compute_hash) {
    rts::MapLoader loader;
    
    rts::MapData map;
    map.id = "test_map";
    map.width = 1024;
    map.height = 1024;
    map.tiles_x = 64;
    map.tiles_y = 64;
    
    std::string hash = loader.compute_hash(map);
    
    if (hash.empty()) {
        throw std::runtime_error("Hash should not be empty");
    }
    
    if (hash.length() != 64) {
        throw std::runtime_error("Hash should be 64 characters (SHA-256 hex)");
    }
    
    for (char c : hash) {
        if (!std::isxdigit(c)) {
            throw std::runtime_error("Hash should contain only hex digits");
        }
    }
}

TEST(map_loader_save_load_round_trip) {
    const fs::path test_dir = test_root / "round_trip";
    fs::remove_all(test_dir);
    rts::MapData source{};
    source.map_version = "1.0";
    source.id = "round_trip";
    source.name = "Round Trip";
    source.description = "serializer coverage";
    source.author = "test";
    source.width = 32;
    source.height = 32;
    source.tile_size = 16.0f;
    source.max_elevation = 100.0f;
    source.tiles_x = 2;
    source.tiles_y = 2;
    source.terrain_heights = {1.25f, 2.5f, 3.75f, 5.0f};
    source.resource_depots.push_back({"metal_1", "material", 4.0f, 5.0f, 100.0f, 3.0f, 0.8f});
    source.spawn_points.push_back({"spawn_1", "faction_a", 6.0f, 7.0f, 1.5f, "land"});
    source.initial_entities.push_back({"unit_1", "unit", "unit|test_mod|heavy", 8.0f, 9.0f, 0.25f, 1.0f, -1.0f, 0.75f, 0});

    rts::MapLoader writer;
    const fs::path map_path = test_dir / "round_trip.map";
    if (!writer.save_map(map_path, source)) {
        throw std::runtime_error("Map save failed");
    }
    rts::MapLoader reader;
    const auto loaded = reader.load_map(map_path);
    if (!loaded || loaded->terrain_heights != source.terrain_heights || loaded->resource_depots.size() != 1 ||
        loaded->spawn_points.size() != 1 || loaded->initial_entities.size() != 1 ||
        loaded->resource_depots[0].id != "metal_1" || loaded->spawn_points[0].heading != 1.5f ||
        loaded->initial_entities[0].content_id != "unit|test_mod|heavy" || loaded->initial_entities[0].velocity_y != -1.0f) {
        throw std::runtime_error("Map save/load did not preserve editable map data");
    }
}

TEST(map_loader_load_spawnpoints) {
    fs::path test_dir = test_root / "spawn_test";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);
    
    fs::path map_path = test_dir / "test.map";
    std::ofstream map_file(map_path);
    map_file << "map_version: \"1.0\"\n";
    map_file << "id: \"spawn_test\"\n";
    map_file << "dimensions:\n";
    map_file << "  width: 512\n";
    map_file << "  height: 512\n";
    map_file << "  tile_size: 16\n";
    map_file << "factions:\n";
    map_file << "  faction_a:\n";
    map_file << "    primary_spawn: spawn_1\n";
    map_file.close();
    
    std::ofstream spawnpoints_file(test_dir / "spawnpoints.yaml");
    spawnpoints_file << "spawnpoints:\n";
    spawnpoints_file << "  - id: \"spawn_1\"\n";
    spawnpoints_file << "    faction: \"faction_a\"\n";
    spawnpoints_file << "    position: [256.0, 256.0]\n";
    spawnpoints_file << "    heading: 0.0\n";
    spawnpoints_file << "    type: \"land\"\n";
    spawnpoints_file << "  - id: \"spawn_2\"\n";
    spawnpoints_file << "    faction: \"faction_a\"\n";
    spawnpoints_file << "    position: [128.0, 128.0]\n";
    spawnpoints_file << "    heading: 1.57\n";
    spawnpoints_file << "    type: \"land\"\n";
    spawnpoints_file.close();
    
    std::ofstream terrain_file(test_dir / "terrain.bin", std::ios::binary);
    uint32_t tiles_x = 512 / 16;
    uint32_t tiles_y = 512 / 16;
    std::vector<float> heights(tiles_x * tiles_y, 100.0f);
    terrain_file.write(reinterpret_cast<const char*>(heights.data()), heights.size() * sizeof(float));
    terrain_file.close();
    
    rts::MapLoader loader;
    auto map_opt = loader.load_map(map_path);
    
    if (!map_opt.has_value()) {
        throw std::runtime_error("Failed to load map with spawnpoints");
    }
    
    const auto& map = map_opt.value();
    
    if (map.spawn_points.size() != 2) {
        throw std::runtime_error("Wrong spawnpoint count");
    }
    
    if (map.spawn_points[0].id != "spawn_1") {
        throw std::runtime_error("Wrong spawnpoint ID");
    }
    
    if (std::abs(map.spawn_points[0].x - 256.0f) > 0.1f) {
        throw std::runtime_error("Wrong spawnpoint X position");
    }
    
    if (std::abs(map.spawn_points[0].y - 256.0f) > 0.1f) {
        throw std::runtime_error("Wrong spawnpoint Y position");
    }
}

TEST(map_loader_load_resources) {
    fs::path test_dir = test_root / "resource_test";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);
    
    fs::path map_path = test_dir / "test.map";
    std::ofstream map_file(map_path);
    map_file << "map_version: \"1.0\"\n";
    map_file << "id: \"resource_test\"\n";
    map_file << "dimensions:\n";
    map_file << "  width: 512\n";
    map_file << "  height: 512\n";
    map_file << "  tile_size: 16\n";
    map_file.close();
    
    std::ofstream resources_file(test_dir / "resources.yaml");
    resources_file << "resources:\n";
    resources_file << "  - id: \"resource_1\"\n";
    resources_file << "    type: \"material\"\n";
    resources_file << "    position: [256.0, 256.0]\n";
    resources_file << "    amount: 10000.0\n";
    resources_file << "    radius: 32.0\n";
    resources_file << "    quality: 1.0\n";
    resources_file.close();
    
    std::ofstream terrain_file(test_dir / "terrain.bin", std::ios::binary);
    uint32_t tiles_x = 512 / 16;
    uint32_t tiles_y = 512 / 16;
    std::vector<float> heights(tiles_x * tiles_y, 100.0f);
    terrain_file.write(reinterpret_cast<const char*>(heights.data()), heights.size() * sizeof(float));
    terrain_file.close();
    
    rts::MapLoader loader;
    auto map_opt = loader.load_map(map_path);
    
    if (!map_opt.has_value()) {
        throw std::runtime_error("Failed to load map with resources");
    }
    
    const auto& map = map_opt.value();
    
    if (map.resource_depots.size() != 1) {
        throw std::runtime_error("Wrong resource count");
    }
    
    if (map.resource_depots[0].id != "resource_1") {
        throw std::runtime_error("Wrong resource ID");
    }
    
    if (std::abs(map.resource_depots[0].amount - 10000.0f) > 0.1f) {
        throw std::runtime_error("Wrong resource amount");
    }
}

TEST(map_loader_load_entities) {
    fs::path test_dir = test_root / "entity_test";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);
    
    fs::path map_path = test_dir / "test.map";
    std::ofstream map_file(map_path);
    map_file << "map_version: \"1.0\"\n";
    map_file << "id: \"entity_test\"\n";
    map_file << "dimensions:\n";
    map_file << "  width: 512\n";
    map_file << "  height: 512\n";
    map_file << "  tile_size: 16\n";
    map_file.close();
    
    std::ofstream entities_file(test_dir / "entities.yaml");
    entities_file << "entities:\n";
    entities_file << "  - id: \"unit_1\"\n";
    entities_file << "    type: \"unit\"\n";
    entities_file << "    content_id: \"faction_a|t1_miner\"\n";
    entities_file << "    position: [256.0, 256.0]\n";
    entities_file << "    heading: 0.0\n";
    entities_file << "    velocity: [0.0, 0.0]\n";
    entities_file << "    hp: 1.0\n";
    entities_file.close();
    
    std::ofstream terrain_file(test_dir / "terrain.bin", std::ios::binary);
    uint32_t tiles_x = 512 / 16;
    uint32_t tiles_y = 512 / 16;
    std::vector<float> heights(tiles_x * tiles_y, 100.0f);
    terrain_file.write(reinterpret_cast<const char*>(heights.data()), heights.size() * sizeof(float));
    terrain_file.close();
    
    rts::MapLoader loader;
    auto map_opt = loader.load_map(map_path);
    
    if (!map_opt.has_value()) {
        throw std::runtime_error("Failed to load map with entities");
    }
    
    const auto& map = map_opt.value();
    
    if (map.initial_entities.size() != 1) {
        throw std::runtime_error("Wrong entity count");
    }
    
    if (map.initial_entities[0].id != "unit_1") {
        throw std::runtime_error("Wrong entity ID");
    }
    
    if (map.initial_entities[0].content_id != "faction_a|t1_miner") {
        throw std::runtime_error("Wrong content ID");
    }
}

TEST(map_loader_load_json_scenario) {
    fs::path scenario_path = fs::absolute(fs::current_path() / "godot" / "project" / "scenarios" / "two_landmass_skirmish.json");
    
    if (!fs::exists(scenario_path)) {
        throw std::runtime_error("Testscenario not found at: " + scenario_path.string());
    }
    
    rts::MapLoader loader;
    auto map_opt = loader.load_map(scenario_path);
    
    if (!map_opt.has_value()) {
        std::string error_list;
        for (const auto& err : loader.get_errors()) {
            error_list += err.message + "\n";
        }
        throw std::runtime_error("Failed to load JSON scenario: " + error_list);
    }
    
    const auto& map = map_opt.value();
    
    if (map.width != 320 || map.height != 320) {
        throw std::runtime_error("Wrong theater dimensions (expected 320x320)");
    }
    
    if (map.tiles_x != 20 || map.tiles_y != 20) {
        throw std::runtime_error("Wrong tile counts (expected 20x20)");
    }
    
    if (map.biomes.size() != 2) {
        throw std::runtime_error("Wrong landmass count (expected 2)");
    }
    
    if (map.spawn_points.size() != 2) {
        throw std::runtime_error("Wrong spawnpoint count (expected 2)");
    }
    
    if (std::abs(map.spawn_points[0].x - (-105.0f)) > 0.1f || 
        std::abs(map.spawn_points[0].y - 0.0f) > 0.1f) {
        throw std::runtime_error("Wrong player spawn position");
    }
    
    if (map.spawn_points[0].faction != "faction_0") {
        throw std::runtime_error("Wrong player faction (expected faction_0)");
    }
    
    if (std::abs(map.spawn_points[1].x - 105.0f) > 0.1f || 
        std::abs(map.spawn_points[1].y - 0.0f) > 0.1f) {
        throw std::runtime_error("Wrong AI spawn position");
    }
    
    if (map.spawn_points[1].faction != "faction_1") {
        throw std::runtime_error("Wrong AI faction (expected faction_1)");
    }
}
