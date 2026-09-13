# Map Format

> **Goal 05 status (2026-09-12):** `MapLoader::save_map()` and `load_map()`
> round-trip an editable YAML map header plus `terrain.bin`, `resources.yaml`,
> `spawnpoints.yaml`, and `entities.yaml`. The native assertion preserves
> terrain samples, resource data, spawn heading, and entity content/velocity.
> This is the serialization foundation; an interactive Godot map-editor UI is
> still future work.

The standalone Godot editor can export its editable terrain, spawn, resource,
and entity state to this canonical sidecar bundle; `test_map_editor_export.gd`
then loads the export through the native `MapLoader` binding.

## Objective

Define a data-driven map format supporting terrain, entities, resource deposits, spawns, and objectives. Maps must be deterministic for simulation reproducibility and scalable to large areas.

## Map Structure

```yaml
map_version: "1.0"
id: "test_01"
name: "Test Map 01"
description: "Small test map for early validation"
author: "System Generated"

dimensions:
  width: 4096
  height: 4096
  tile_size: 16
  max_elevation: 512

layers:
  terrain:
    type: "grid"
    data: "terrain.bin"
    tiles_x: 256
    tiles_y: 256
  resources:
    type: "sparse"
    data: "resources.bin"
    max_resources: 100
  spawnpoints:
    type: "list"
    data: "spawnpoints.yaml"
  entities:
    type: "list"
    data: "entities.yaml"

factions:
  faction_a:
    primary_spawn: spawn_a_1
    secondary_spawn: spawn_a_2
  faction_b:
    primary_spawn: spawn_b_1
    secondary_spawn: spawn_b_2

biomes:
  ocean:
    terrain_type: deep_water
    resource_multiplier: 0.5
  coast:
    terrain_type: shallow_water
    resource_multiplier: 0.8
  plains:
    terrain_type: flat_terrain
    resource_multiplier: 1.0
  hills:
    terrain_type: rolling_hills
    resource_multiplier: 1.2
  mountains:
    terrain_type: rough_terrain
    resource_multiplier: 0.7

metadata:
  created_at: "2026-09-04T12:00:00Z"
  simulation_hash: "deterministic_seed_001"
  valid_factions: ["faction_a", "faction_b"]
  min_players: 2
  max_players: 8
```

## Terrain Layer

### Grid Format

```
terrain.bin: 4-byte floats, row-major, heightmap

Dimensions: 256 × 256 tiles = 65,536 height samples
Format: IEEE 754 float32, little-endian
Range: 0.0 to 512.0 meters
```

### Biome Assignment

Biomes applied per-tile based on height:

```
height < 20 → ocean
20 <= height < 50 → coast
50 <= height < 200 → plains
200 <= height < 400 → hills
height >= 400 → mountains
```

## Resource Deposits

Sparse dataset:

```yaml
resources:
  - id: "resource_001"
    type: "material"
    position: [512.0, 1024.0]
    amount: 100000
    radius: 64.0
    quality: 1.0

  - id: "resource_002"
    type: "energy"
    position: [2048.0, 2048.0]
    amount: 200000
    radius: 128.0
    quality: 1.0
```

## Spawn Points

```yaml
spawnpoints:
  - id: "spawn_a_1"
    faction: "faction_a"
    position: [256.0, 256.0]
    heading: 0.0
    type: "land"

  - id: "spawn_a_2"
    faction: "faction_a"
    position: [256.0, 3840.0]
    heading: 0.0
    type: "land"

  - id: "spawn_b_1"
    faction: "faction_b"
    position: [3840.0, 3840.0]
    heading: 3.14159
    type: "land"

  - id: "spawn_b_2"
    faction: "faction_b"
    position: [3840.0, 256.0]
    heading: 3.14159
    type: "land"
```

## Entities (Buildings, Units at Start)

```yaml
entities:
  - id: "unit_001"
    type: "unit"
    content_id: "faction_a|t1_miner"
    position: [384.0, 256.0]
    heading: 0.0
    velocity: [0.0, 0.0]
    hp: 1.0

  - id: "building_001"
    type: "building"
    content_id: "faction_a|t1_factory"
    position: [320.0, 256.0]
    heading: 0.0
    hp: 1.0
```

## Map Validation

### Determinism Check

Maps must be deterministic:

```
Map hash = SHA-256(
    map_id ||
    dimensions.width ||
    dimensions.height ||
    terrain_data_hash ||
    resources_hash ||
    spawnpoints_hash ||
    entities_hash
)
```

## Map Loading

### Save/load contract

`save_map(path, map)` accepts a non-JSON map path only after `validate_map()`
passes. It writes the header at `path` and the four named sidecars beside it.
Floats use `max_digits10` precision and terrain uses native little-endian
float32 samples. Failed validation or any failed write returns `false` and
records a `MapLoadError`; callers must not treat a partial write as a saved
map.

### C++ Interface

```cpp
struct MapData {
    std::string id;
    uint32_t width;
    uint32_t height;
    std::vector<float> terrain_heights;
    std::vector<ResourceDepot> resource_depots;
    std::vector<SpawnPoint> spawn_points;
    std::vector<MapEntity> initial_entities;
    std::unordered_map<FactionId, FactionSpawnData> faction_data;
};

class MapLoader {
    MapData load_map(const fs::path& map_path);
    void validate_map(const MapData& map);
    std::string compute_hash(const MapData& map);
};

// Usage
MapLoader loader;
MapData map = loader.load_map("maps/test_01.map");
loader.validate_map(map);
std::string hash = loader.compute_hash(map);
```

### Terrain Access

```cpp
class TerrainGrid {
    std::vector<float> heights;
    uint32_t width;
    uint32_t height;
    float tile_size;
    
    float get_height(float x, float y) const;
    Biome get_biome(float x, float y) const;
    std::vector<float> get_heights_in_range(
        float min_x, float min_y, float max_x, float max_y
    ) const;
};
```

## File Format Options

### Option 1: YAML + Binary

Binary terrain/resources for performance

```yaml
# map_header.yaml
map_version: "1.0"
id: "test_01"
dimensions:
  width: 4096
  height: 4096
  tile_size: 16

resources: [...]
spawnpoints: [...]
entities: [...]
```

```binary
terrain.bin  # float32 heightmap
resources.bin  # binary resource list
```

### Option 2: SQLite Database

```sql
CREATE TABLE map_info (
    id TEXT,
    width INTEGER,
    height INTEGER,
    tile_size REAL,
    biome_count INTEGER
);

CREATE TABLE terrain_tiles (
    x INTEGER,
    y INTEGER,
    height REAL
);

CREATE TABLE resources (
    id TEXT,
    type TEXT,
    x REAL,
    y REAL,
    amount REAL,
    radius REAL
);

CREATE TABLE spawnpoints (
    id TEXT,
    faction TEXT,
    x REAL,
    y REAL,
    heading REAL
);
```

## Best Practices

1. Keep maps deterministic (no timestamps in map data)
2. Use binary for terrain (large, dense)
3. Use YAML for sparse data (resources, spawns, entities)
4. Validate maps before loading
5. Compute and store map hash for caching/deduplication
6. Support map versioning (upgrade old maps automatically)

## Future Enhancements

1. Heightmap compression (RLE, LZ4)
2. Terrain texturing metadata
3. Dynamic terrain changes (craters, destruction)
4. Map themes/presets (arctic, desert, jungle)
5. Map metadata (difficulty, recommended units)
6. Map streaming (LOD terrain)
