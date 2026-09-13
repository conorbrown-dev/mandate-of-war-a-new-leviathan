# Goal 10 — Terrain System

**Status:** VERIFIED on 2026-09-13

## Objective

Implement a data-driven terrain system with heightmaps, biomes, materials, and terrain-aware gameplay mechanics. The terrain must be deterministic, scalable, and visually distinct.

## Acceptance Criteria

| ID | State | Acceptance requirement | Current evidence / next proof |
|---|---|---|---|
| G10-HEIGHTMAP | `COMPLETE` | Heightmap in JSON scenarios (float32 grid) | JSON `two_landmass_skirmish.json` validates; `skirmish_config.gd` parses terrain section |
| G10-ROADS | `COMPLETE` | Roads/craters modify terrain walkability | `RoadNetwork` completion applies traversal costs and structure/crater blockers to `Pathfinding`; road construction and traversal tests pass |
| G10-BIOMES | `COMPLETE` | Biome per grid cell (ocean/coast/plains/hills/mountains) | HeightMap.gd `get_biome()` implements all biomes per thresholds |
| G10-MATERIALS | `COMPLETE` | Per-biome materials (water, sand, grass, rock, snow) | `terrain.gdshader` binds authored water/shore/grass/forest/mud/rock textures and deterministic elevation bands |
| G10-COLLISION | `COMPLETE` | Unit collision with terrain height | `Terrain::height_at()` is authoritative and ground-unit Z follows the heightfield; regression test passes |
| G10-RENDERING | `COMPLETE` | Vertex-height mesh (Godot mesh) | HeightMap.gd `generate_terrain_mesh()` produces ArrayMesh with vertices/normals/colors |
| G10-TESTS | `COMPLETE` | Terrain load/validate/generate render and collision tests | CTest 3/3 and direct terrain/road assertions pass |

## Terrain Heightmap Format

```json
{
  "version": 1,
  "theater": {
    "width": 320,
    "height": 320,
    "terrain": {
      "type": "heightmap",
      "data": "terrain.bin",
      "tiles_x": 320,
      "tiles_y": 320
    }
  }
}
```

### Biome Thresholds (height in world units)

```
height < 0.0     → ocean (water)
0.0 <= height < 5.0   → coast (sand)
5.0 <= height < 30.0  → plains (grass)
30.0 <= height < 80.0 → hills (mixed grass/rock)
height >= 80.0   → mountains (rock/snow)
```

## Implementation Steps

1. **JSON Schema**
   - Add `theater.terrain.heightmap` section to scenario schema
   - Define `terrain.bin` format (float32 heightmap)

2. **C++ Terrain Loader**
   - Extend `MapLoader` to parse heightmap from JSON + binary
   - Generate height grid (320×320 float32)

3. **Biome Assignment**
   - Map height values to biomes per thresholds above
   - Biome grid (320×320 enum)

4. **Godot Procedural Mesh**
   - Generate vertex positions from heightmap
   - Generate normals for lighting
   - Assign materials per biome column (vertical texture blending)

5. **Collision Integration**
   - Unit height query from terrain grid
   - Update pathfinding cell walkability per terrain type

6. **Testing**
   - Load heightmap, verify grid size
   - Verify biome assignment
   - Verify mesh vertex count = 320×320
   - Verify collision height accuracy

## Next Steps

1. Create binary terrain heightmap file (320x320 float32, row-major)
2. Update `two_landmass_skirmish.json` to include terrain.heightmap section
3. Call `HeightMap.generate_terrain_mesh()` in `main.gd _ready()` after loading scenario
4. Replace static landmass meshes with MeshInstance3D using generated mesh
5. Add collision detection for units using terrain height grid
6. Add integration tests for terrain generation
