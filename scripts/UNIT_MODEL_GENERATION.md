# Unit Model Generation Guide

## Overview

This project uses a two-phase pipeline for unit model generation:
1. **JSON Mesh Definitions** - Standalone mesh data for procedural generation
2. **Blender Add-ons** - 3D modeling tools for detailed asset creation

## Generating Mesh Definitions

```bash
# Generate all unit meshes from unit_faction_stats.json
python3 scripts/generate_unit_mesh.py --all

# Generate a specific unit from JSON
python3 scripts/generate_unit_mesh.py data/generated_units/faction_a_t1_interceptor.json

# Generate with custom output
python3 scripts/generate_unit_mesh.py data/generated_units/faction_b_t2_heavy.json -o data/mymeshes/
```

Output: `data/generated_units/meshes/*.mesh.json`

## Using Blender

### Prerequisites
- Install Blender 4.0+ (https://blender.org)

### Generate Unit in Blender

```bash
# From command line (background mode)
blender --background --python scripts/generate_unit_blender.py -- \
  data/generated_units/faction_a_t1_interceptor.json

# Or open Blender and run the script in the Scripting workspace
# File > Scripting > Run Script > scripts/generate_unit_blender.py
```

### Blender Script Features
- **Ground units**: Box with optional turret (tier 2+), sensors (tier 3+)
- **Air units**: Low-profile body with wings and nose cone
- **Naval units**: Hull with superstructure
- **Faction colors**: Auto-applied based on JSON faction field
- **Tier levels**: Visual distinction with sensors/equipment
- **Auto-save**: Saves `.blend` files to `data/generated_units/blender/`

## JSON Input Format

```json
{
  "id": "unit_name",
  "faction": "faction_a",
  "tier": 1,
  "movement_type": "ground|air|sea",
  "size_x": 2.0,
  "size_y": 2.0,
  "size_z": 2.0
}
```

## Generated Mesh Format

```json
{
  "unit_id": "unit_name",
  "format_version": "1.0",
  "dimensions": {
    "width": 2.0,
    "height": 1.0,
    "depth": 2.4
  },
  "base_type": "ground_unit|aircraft|ship",
  "role_details": {
    "role": "interceptor",
    "tier": 1,
    "faction": "faction_a"
  },
  "default_color": [0.12, 0.62, 1.0, 1.0],
  "lod_levels": [1.0, 0.75, 0.5, 0.25],
  "collider_type": "box",
  "geometry": {
    "type": "procedural_mesh",
    "vertices": [...],
    "triangles": [...]
  }
}
```

##Godot Integration

1. Mesh definitions are consumed by the game engine at runtime
2. For Blender models: Export `.gltf` from Blender, load in Godot
3. The `RtsExtension` uses MultiMeshInstance3D for unit rendering
4. Unit colors applied via vertex colors or materials

## Script Files

| File | Purpose |
|------|---------|
| `scripts/generate_unit_mesh.py` | Standalone mesh generator (no Blender needed) |
| `scripts/generate_unit_blender.py` | Blender add-on for 3D modeling |
| `scripts/generate_unit.py` | High-level unit definition generator |
| `generate.py` | Faction/tier/role unit generator |

## Next Steps

1. Generate meshes: `python3 scripts/generate_unit_mesh.py --all`
2. Review `data/generated_units/meshes/*.mesh.json`
3. Open Blender and import `.gltf` / `.blend` files
4. Fine-tune models in Blender (textures, details, animations)
5. Export to Godot-compatible formats
