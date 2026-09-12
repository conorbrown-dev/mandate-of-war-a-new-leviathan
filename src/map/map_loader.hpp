#ifndef MAP_LOADER_HPP
#define MAP_LOADER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <map>

namespace fs = std::filesystem;

namespace rts {

enum class Biome {
    OCEAN,
    COAST,
    PLAINS,
    HILLS,
    MOUNTAINS
};

struct ResourceDepot {
    std::string id;
    std::string type;
    float x, y;
    float amount;
    float radius;
    float quality;
};

struct SpawnPoint {
    std::string id;
    std::string faction;
    float x, y;
    float heading;
    std::string type;
};

struct MapEntity {
    std::string id;
    std::string type;
    std::string content_id;
    float x, y;
    float heading;
    float velocity_x, velocity_y;
    float hp;
    uint8_t unit_type;
};

struct FactionSpawnData {
    std::string primary_spawn;
    std::string secondary_spawn;
};

struct MapData {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string map_version;
    
    uint32_t width;
    uint32_t height;
    float tile_size;
    float max_elevation;
    
    std::vector<float> terrain_heights;
    uint32_t tiles_x;
    uint32_t tiles_y;
    
    std::vector<ResourceDepot> resource_depots;
    std::vector<SpawnPoint> spawn_points;
    std::vector<MapEntity> initial_entities;
    
    std::unordered_map<std::string, FactionSpawnData> faction_data;
    std::unordered_map<std::string, std::string> biomes;
    
    std::optional<std::string> generation_seed;
    std::vector<std::pair<float, float>> landmass_centers;
    std::vector<std::pair<float, float>> landmass_sizes;
};

struct MapLoadError {
    std::string message;
    std::string field;
    int line_number;
};

class MapLoader {
public:
    MapLoader();
    ~MapLoader();
    
    std::optional<MapData> load_map(const fs::path& map_path);
    bool save_map(const fs::path& map_path, const MapData& map);
    bool validate_map(const MapData& map);
    std::string compute_hash(const MapData& map);
    
    const std::vector<MapLoadError>& get_errors() const { return errors_; }
    
private:
    std::vector<MapLoadError> errors_;
    
    bool load_yaml_header(const fs::path& map_path, MapData& map_data);
    bool load_terrain_binary(const fs::path& bin_path, MapData& map_data);
    bool load_resources_yaml(const fs::path& yaml_path, MapData& map_data);
    bool load_spawnpoints_yaml(const fs::path& yaml_path, MapData& map_data);
    bool load_entities_yaml(const fs::path& yaml_path, MapData& map_data);
    
    bool load_json_scenario(const fs::path& json_path, MapData& map_data);
    
    void add_error(const std::string& message, const std::string& field = "", int line_number = 0);
    
    static std::string sha256_hex(const std::string& input);
};

} // namespace rts

#endif
