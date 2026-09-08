#include "map/map_loader.hpp"
#include "data/json_parser.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
#include <openssl/sha.h>
#include <regex>

namespace rts {

MapLoader::MapLoader() {}

MapLoader::~MapLoader() {}

std::optional<MapData> MapLoader::load_map(const fs::path& map_path) {
    errors_.clear();
    
    if (!fs::exists(map_path)) {
        add_error("Map file not found: " + map_path.string());
        return std::nullopt;
    }
    
    MapData map_data;
    
    auto extension = map_path.extension().string();
    if (extension == ".json") {
        if (!load_json_scenario(map_path, map_data)) {
            return std::nullopt;
        }
    } else {
        if (!load_yaml_header(map_path, map_data)) {
            return std::nullopt;
        }
        
        auto terrain_path = map_path.parent_path() / "terrain.bin";
        if (!load_terrain_binary(terrain_path, map_data)) {
            return std::nullopt;
        }
        
        auto resources_path = map_path.parent_path() / "resources.yaml";
        if (fs::exists(resources_path)) {
            if (!load_resources_yaml(resources_path, map_data)) {
                return std::nullopt;
            }
        }
        
        auto spawnpoints_path = map_path.parent_path() / "spawnpoints.yaml";
        if (fs::exists(spawnpoints_path)) {
            if (!load_spawnpoints_yaml(spawnpoints_path, map_data)) {
                return std::nullopt;
            }
        }
        
        auto entities_path = map_path.parent_path() / "entities.yaml";
        if (fs::exists(entities_path)) {
            if (!load_entities_yaml(entities_path, map_data)) {
                return std::nullopt;
            }
        }
    }
    
    return map_data;
}

bool MapLoader::validate_map(const MapData& map) {
    if (map.id.empty()) {
        add_error("Map ID is empty");
        return false;
    }
    
    if (map.width == 0 || map.height == 0) {
        add_error("Map dimensions are invalid");
        return false;
    }
    
    if (map.tile_size <= 0) {
        add_error("Tile size must be positive");
        return false;
    }
    
    if (map.terrain_heights.size() != map.tiles_x * map.tiles_y) {
        add_error("Terrain height count doesn't match dimensions");
        return false;
    }
    
    return true;
}

std::string MapLoader::compute_hash(const MapData& map) {
    std::ostringstream oss;
    oss << map.id << "|"
        << map.width << "|"
        << map.height << "|"
        << map.tiles_x << "|"
        << map.tiles_y;
    
    std::string input = oss.str();
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    
    std::ostringstream hex_oss;
    hex_oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hex_oss << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }
    
    return hex_oss.str();
}

bool MapLoader::load_yaml_header(const fs::path& map_path, MapData& map_data) {
    std::ifstream stream(map_path);
    if (!stream.is_open()) {
        add_error("Failed to open map file: " + map_path.string());
        return false;
    }
    
    std::string line;
    bool in_dimensions = false;
    bool in_layers = false;
    bool in_factions = false;
    bool in_biomes = false;
    bool in_metadata = false;
    std::string current_biome;
    
    int line_number = 0;
    
    while (std::getline(stream, line)) {
        line_number++;
        
        if (line.empty() || line[0] == '#') continue;
        
        if (line == "dimensions:") {
            in_dimensions = true;
            in_layers = in_factions = in_biomes = in_metadata = false;
            continue;
        }
        
        if (line == "layers:") {
            in_layers = true;
            in_dimensions = in_factions = in_biomes = in_metadata = false;
            continue;
        }
        
        if (line == "factions:") {
            in_factions = true;
            in_dimensions = in_layers = in_biomes = in_metadata = false;
            continue;
        }
        
        if (line == "biomes:") {
            in_biomes = true;
            in_dimensions = in_layers = in_factions = in_metadata = false;
            continue;
        }
        
        if (line == "metadata:") {
            in_metadata = true;
            in_dimensions = in_layers = in_factions = in_biomes = false;
            continue;
        }
        
        if (in_dimensions) {
            std::regex width_regex(R"(width:\s*(\d+))");
            std::regex height_regex(R"(height:\s*(\d+))");
            std::regex tile_regex(R"(tile_size:\s*([\d.]+))");
            std::regex max_elev_regex(R"(max_elevation:\s*(\d+))");
            
            std::smatch match;
            if (std::regex_search(line, match, width_regex)) {
                map_data.width = std::stoul(match[1].str());
            } else if (std::regex_search(line, match, height_regex)) {
                map_data.height = std::stoul(match[1].str());
            } else if (std::regex_search(line, match, tile_regex)) {
                map_data.tile_size = std::stof(match[1].str());
            } else if (std::regex_search(line, match, max_elev_regex)) {
                map_data.max_elevation = std::stoul(match[1].str());
            }
        }
        
        if (line.find("map_version:") == 0) {
            std::regex version_regex(R"(map_version:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, version_regex)) {
                map_data.map_version = match[1].str();
            }
        } else if (line.find("id:") == 0) {
            std::regex id_regex(R"(id:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, id_regex)) {
                map_data.id = match[1].str();
            }
        } else if (line.find("name:") == 0) {
            std::regex name_regex(R"(name:\s*["']?(.+?)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, name_regex)) {
                map_data.name = match[1].str();
            }
        } else if (line.find("description:") == 0) {
            std::regex desc_regex(R"(description:\s*["']?(.+?)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, desc_regex)) {
                map_data.description = match[1].str();
            }
        } else if (line.find("author:") == 0) {
            std::regex author_regex(R"(author:\s*["']?(.+?)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, author_regex)) {
                map_data.author = match[1].str();
            }
        } else if (in_factions && line.find("  ") == 0 && line.find(":") != std::string::npos) {
            size_t colon_pos = line.find(":");
            std::string faction_name = line.substr(2, colon_pos - 2);
            
            if (line.find("primary_spawn:") != std::string::npos) {
                std::regex spawn_regex(R"(primary_spawn:\s*["']?([^"']+)["']?\s*$)");
                std::smatch match;
                if (std::regex_search(line, match, spawn_regex)) {
                    map_data.faction_data[faction_name].primary_spawn = match[1].str();
                }
            } else if (line.find("secondary_spawn:") != std::string::npos) {
                std::regex spawn_regex(R"(secondary_spawn:\s*["']?([^"']+)["']?\s*$)");
                std::smatch match;
                if (std::regex_search(line, match, spawn_regex)) {
                    map_data.faction_data[faction_name].secondary_spawn = match[1].str();
                }
            }
        }
    }
    
    map_data.tiles_x = map_data.width / static_cast<uint32_t>(map_data.tile_size);
    map_data.tiles_y = map_data.height / static_cast<uint32_t>(map_data.tile_size);
    
    return true;
}

bool MapLoader::load_terrain_binary(const fs::path& bin_path, MapData& map_data) {
    std::ifstream stream(bin_path, std::ios::binary);
    if (!stream.is_open()) {
        add_error("Failed to open terrain binary file: " + bin_path.string());
        return false;
    }
    
    size_t expected_size = map_data.tiles_x * map_data.tiles_y * sizeof(float);
    map_data.terrain_heights.resize(map_data.tiles_x * map_data.tiles_y);
    
    stream.read(reinterpret_cast<char*>(map_data.terrain_heights.data()), expected_size);
    
    if (stream.gcount() != static_cast<std::streamsize>(expected_size)) {
        add_error("Terrain binary file size mismatch");
        return false;
    }
    
    return true;
}

bool MapLoader::load_resources_yaml(const fs::path& yaml_path, MapData& map_data) {
    std::ifstream stream(yaml_path);
    if (!stream.is_open()) {
        add_error("Failed to open resources file: " + yaml_path.string());
        return false;
    }
    
    std::string line;
    ResourceDepot current_resource;
    bool in_resources = false;
    
    while (std::getline(stream, line)) {
        if (line.find("resources:") == 0) {
            in_resources = true;
            continue;
        }
        
        if (in_resources && line.find("  - id:") == 0) {
            if (!current_resource.id.empty()) {
                map_data.resource_depots.push_back(current_resource);
            }
            
            std::regex id_regex(R"(^\s*-\s*id:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, id_regex)) {
                current_resource.id = match[1].str();
            }
        } else if (in_resources && line.find("    type:") == 0) {
            std::regex type_regex(R"(^\s*type:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, type_regex)) {
                current_resource.type = match[1].str();
            }
        } else if (in_resources && line.find("    position:") == 0) {
            std::regex pos_regex(R"(^\s*position:\s*\[\s*([\d.]+)\s*,\s*([\d.]+)\s*\])");
            std::smatch match;
            if (std::regex_search(line, match, pos_regex)) {
                current_resource.x = std::stof(match[1].str());
                current_resource.y = std::stof(match[2].str());
            }
        } else if (in_resources && line.find("    amount:") == 0) {
            std::regex amount_regex(R"(^\s*amount:\s*([\d.]+)\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, amount_regex)) {
                current_resource.amount = std::stof(match[1].str());
            }
        } else if (in_resources && line.find("    radius:") == 0) {
            std::regex radius_regex(R"(^\s*radius:\s*([\d.]+)\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, radius_regex)) {
                current_resource.radius = std::stof(match[1].str());
            }
        } else if (in_resources && line.find("    quality:") == 0) {
            std::regex quality_regex(R"(^\s*quality:\s*([\d.]+)\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, quality_regex)) {
                current_resource.quality = std::stof(match[1].str());
            }
        }
    }
    
    if (!current_resource.id.empty()) {
        map_data.resource_depots.push_back(current_resource);
    }
    
    return true;
}

bool MapLoader::load_spawnpoints_yaml(const fs::path& yaml_path, MapData& map_data) {
    std::ifstream stream(yaml_path);
    if (!stream.is_open()) {
        add_error("Failed to open spawnpoints file: " + yaml_path.string());
        return false;
    }
    
    std::string line;
    SpawnPoint current_spawn;
    bool in_spawnpoints = false;
    
    while (std::getline(stream, line)) {
        if (line.find("spawnpoints:") == 0) {
            in_spawnpoints = true;
            continue;
        }
        
        if (in_spawnpoints && line.find("  - id:") == 0) {
            if (!current_spawn.id.empty()) {
                map_data.spawn_points.push_back(current_spawn);
            }
            
            std::regex id_regex(R"(^\s*-\s*id:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, id_regex)) {
                current_spawn.id = match[1].str();
            }
        } else if (in_spawnpoints && line.find("    faction:") == 0) {
            std::regex faction_regex(R"(^\s*faction:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, faction_regex)) {
                current_spawn.faction = match[1].str();
            }
        } else if (in_spawnpoints && line.find("    position:") == 0) {
            std::regex pos_regex(R"(^\s*position:\s*\[\s*([\d.]+)\s*,\s*([\d.]+)\s*\])");
            std::smatch match;
            if (std::regex_search(line, match, pos_regex)) {
                current_spawn.x = std::stof(match[1].str());
                current_spawn.y = std::stof(match[2].str());
            }
        } else if (in_spawnpoints && line.find("    heading:") == 0) {
            std::regex heading_regex(R"(^\s*heading:\s*([\d.]+)\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, heading_regex)) {
                current_spawn.heading = std::stof(match[1].str());
            }
        } else if (in_spawnpoints && line.find("    type:") == 0) {
            std::regex type_regex(R"(^\s*type:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, type_regex)) {
                current_spawn.type = match[1].str();
            }
        }
    }
    
    if (!current_spawn.id.empty()) {
        map_data.spawn_points.push_back(current_spawn);
    }
    
    return true;
}

bool MapLoader::load_entities_yaml(const fs::path& yaml_path, MapData& map_data) {
    std::ifstream stream(yaml_path);
    if (!stream.is_open()) {
        add_error("Failed to open entities file: " + yaml_path.string());
        return false;
    }
    
    std::string line;
    MapEntity current_entity;
    bool in_entities = false;
    
    while (std::getline(stream, line)) {
        if (line.find("entities:") == 0) {
            in_entities = true;
            continue;
        }
        
        if (in_entities && line.find("  - id:") == 0) {
            if (!current_entity.id.empty()) {
                map_data.initial_entities.push_back(current_entity);
            }
            
            std::regex id_regex(R"(^\s*-\s*id:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, id_regex)) {
                current_entity.id = match[1].str();
            }
        } else if (in_entities && line.find("    type:") == 0) {
            std::regex type_regex(R"(^\s*type:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, type_regex)) {
                current_entity.type = match[1].str();
            }
        } else if (in_entities && line.find("    content_id:") == 0) {
            std::regex content_regex(R"(^\s*content_id:\s*["']?([^"']+)["']?\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, content_regex)) {
                current_entity.content_id = match[1].str();
            }
        } else if (in_entities && line.find("    position:") == 0) {
            std::regex pos_regex(R"(^\s*position:\s*\[\s*([\d.]+)\s*,\s*([\d.]+)\s*\])");
            std::smatch match;
            if (std::regex_search(line, match, pos_regex)) {
                current_entity.x = std::stof(match[1].str());
                current_entity.y = std::stof(match[2].str());
            }
        } else if (in_entities && line.find("    heading:") == 0) {
            std::regex heading_regex(R"(^\s*heading:\s*([\d.]+)\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, heading_regex)) {
                current_entity.heading = std::stof(match[1].str());
            }
        } else if (in_entities && line.find("    velocity:") == 0) {
            std::regex vel_regex(R"(^\s*velocity:\s*\[\s*([\d.-]+)\s*,\s*([\d.-]+)\s*\])");
            std::smatch match;
            if (std::regex_search(line, match, vel_regex)) {
                current_entity.velocity_x = std::stof(match[1].str());
                current_entity.velocity_y = std::stof(match[2].str());
            }
        } else if (in_entities && line.find("    hp:") == 0) {
            std::regex hp_regex(R"(^\s*hp:\s*([\d.]+)\s*$)");
            std::smatch match;
            if (std::regex_search(line, match, hp_regex)) {
                current_entity.hp = std::stof(match[1].str());
            }
        }
    }
    
    if (!current_entity.id.empty()) {
        map_data.initial_entities.push_back(current_entity);
    }
    
    return true;
}

void MapLoader::add_error(const std::string& message, const std::string& field, int line_number) {
    MapLoadError error;
    error.message = message;
    error.field = field;
    error.line_number = line_number;
    errors_.push_back(error);
}

std::string MapLoader::sha256_hex(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }
    
    return oss.str();
}

bool MapLoader::load_json_scenario(const fs::path& json_path, MapData& map_data) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        add_error("Failed to open JSON scenario: " + json_path.string());
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    auto parsed_opt = rts::data::JsonParser::parse(content);
    if (!parsed_opt.has_value()) {
        add_error("Failed to parse JSON: " + json_path.string());
        return false;
    }
    
    const auto& root = parsed_opt.value();
    if (root.type() != rts::data::JsonValue::Type::Object) {
        add_error("JSON scenario must be an object: " + json_path.string());
        return false;
    }
    
    const auto& root_obj = root.as_object();
    
    auto version_opt = root_obj.find("version");
    if (version_opt == root_obj.end() || version_opt->second.type() != rts::data::JsonValue::Type::Number) {
        add_error("Missing or invalid 'version' in JSON scenario: " + json_path.string());
        return false;
    }
    int version = static_cast<int>(version_opt->second.as_number());
    
    auto id_opt = root_obj.find("id");
    if (id_opt != root_obj.end() && id_opt->second.type() == rts::data::JsonValue::Type::String) {
        map_data.id = id_opt->second.as_string();
    }
    
    auto display_name_opt = root_obj.find("display_name");
    if (display_name_opt != root_obj.end() && display_name_opt->second.type() == rts::data::JsonValue::Type::String) {
        map_data.name = display_name_opt->second.as_string();
    }
    
    auto theater_opt = root_obj.find("theater");
    if (theater_opt == root_obj.end() || theater_opt->second.type() != rts::data::JsonValue::Type::Object) {
        add_error("Missing or invalid 'theater' in JSON scenario: " + json_path.string());
        return false;
    }
    
    const auto& theater = theater_opt->second.as_object();
    
    auto width_opt = theater.find("width");
    auto height_opt = theater.find("height");
    if (width_opt == theater.end() || height_opt == theater.end()) {
        add_error("Missing theater dimensions in JSON scenario: " + json_path.string());
        return false;
    }
    
    if (width_opt->second.type() != rts::data::JsonValue::Type::Number ||
        height_opt->second.type() != rts::data::JsonValue::Type::Number) {
        add_error("Theater dimensions must be numbers in JSON scenario: " + json_path.string());
        return false;
    }
    
    map_data.width = static_cast<uint32_t>(width_opt->second.as_number());
    map_data.height = static_cast<uint32_t>(height_opt->second.as_number());
    
    map_data.tile_size = 16.0f;
    map_data.max_elevation = 512;
    
    map_data.tiles_x = map_data.width / static_cast<uint32_t>(map_data.tile_size);
    map_data.tiles_y = map_data.height / static_cast<uint32_t>(map_data.tile_size);
    
    map_data.terrain_heights.resize(map_data.tiles_x * map_data.tiles_y, 0.0f);
    
    auto landmasses_opt = theater.find("landmasses");
    if (landmasses_opt != theater.end() && landmasses_opt->second.type() == rts::data::JsonValue::Type::Array) {
        const auto& landmasses = landmasses_opt->second.as_array();
        for (size_t i = 0; i < landmasses.size(); ++i) {
            auto landmass_value = landmasses[i];
            if (landmass_value.type() == rts::data::JsonValue::Type::Object) {
                const auto& landmass_obj = landmass_value.as_object();
                auto landmass_id_opt = landmass_obj.find("id");
                if (landmass_id_opt != landmass_obj.end() && 
                    landmass_id_opt->second.type() == rts::data::JsonValue::Type::String) {
                    std::string landmass_id = landmass_id_opt->second.as_string();
                    map_data.biomes["landmass_" + std::to_string(i)] = landmass_id;
                }
            }
        }
    }
    
    auto player_opt = root_obj.find("player");
    if (player_opt != root_obj.end() && player_opt->second.type() == rts::data::JsonValue::Type::Object) {
        const auto& player = player_opt->second.as_object();
        auto faction_id_opt = player.find("faction_id");
        auto spawn_opt = player.find("spawn");
        if (spawn_opt != player.end() && spawn_opt->second.type() == rts::data::JsonValue::Type::Array) {
            const auto& spawn_arr = spawn_opt->second.as_array();
            if (spawn_arr.size() >= 2) {
                SpawnPoint spawn;
                spawn.id = "player_spawn";
                if (faction_id_opt != player.end() && faction_id_opt->second.type() == rts::data::JsonValue::Type::Number) {
                    spawn.faction = "faction_" + std::to_string(static_cast<int>(faction_id_opt->second.as_number()));
                } else {
                    spawn.faction = "faction_0";
                }
                spawn.x = static_cast<float>(spawn_arr[0].as_number());
                spawn.y = static_cast<float>(spawn_arr[1].as_number());
                spawn.heading = 0.0f;
                spawn.type = "land";
                map_data.spawn_points.push_back(spawn);
            }
        }
    }
    
    auto ai_opt = root_obj.find("ai");
    if (ai_opt != root_obj.end() && ai_opt->second.type() == rts::data::JsonValue::Type::Object) {
        const auto& ai = ai_opt->second.as_object();
        auto faction_id_opt = ai.find("faction_id");
        auto spawn_opt = ai.find("spawn");
        if (spawn_opt != ai.end() && spawn_opt->second.type() == rts::data::JsonValue::Type::Array) {
            const auto& spawn_arr = spawn_opt->second.as_array();
            if (spawn_arr.size() >= 2) {
                SpawnPoint spawn;
                spawn.id = "ai_spawn";
                if (faction_id_opt != ai.end() && faction_id_opt->second.type() == rts::data::JsonValue::Type::Number) {
                    spawn.faction = "faction_" + std::to_string(static_cast<int>(faction_id_opt->second.as_number()));
                } else {
                    spawn.faction = "faction_1";
                }
                spawn.x = static_cast<float>(spawn_arr[0].as_number());
                spawn.y = static_cast<float>(spawn_arr[1].as_number());
                spawn.heading = 0.0f;
                spawn.type = "land";
                map_data.spawn_points.push_back(spawn);
            }
        }
    }
    
    return true;
}

} // namespace rts
