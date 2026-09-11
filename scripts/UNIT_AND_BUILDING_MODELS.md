# Unit and Building Model Generation Guide

## Overview

This project provides automated mesh generation for:
- **Units**: Ground vehicles (t1-t4), aircraft (t1-t4), naval vessels (t1-t4)
- **Buildings**: Command centers, production facilities, resource extractors, airbases
- **Factions**: Faction A (Blue), Faction B (Red), Faction C (Green)

## Generated Assets

### Units (51 total)
| Tier | Ground | Air | Naval |
|------|--------|-----|-------|
| T1 | 9 units (3 factions × 3 roles) | 2 units | 1 unit |
| T2 | 12 units | 12 units | 0 units |
| T3 | 12 units | 12 units | 0 units |
| T4 | 12 units | 12 units | 0 units |

### Buildings (12 total)
- 4 building types × 3 factions

**Total: 61 assets**

## Mesh JSON Generator

```bash
# Generate all units and buildings
python3 scripts/generate_all_assets.py --all

# Generate from specific JSON
python3 scripts/generate_all_assets.py <unit.json>

# Output location
data/generated_units/meshes/*.mesh.json
```

### Mesh JSON Format
```json
{
  "unit_id": "faction_a_command_center",
  "format_version": "1.0",
  "dimensions": {"width": 8.0, "height": 10.0, "depth": 8.0},
  "base_type": "building",
  "role_details": {"role": "command_center", "tier": 1, "faction": "faction_a"},
  "default_color": [0.12, 0.62, 1.0, 1.0],
  "lod_levels": [1.0, 0.75, 0.5, 0.25],
  "collider_type": "box",
  "geometry": {
    "type": "procedural_mesh",
    "vertices": [[x,y,z], ...],
    "triangles": [indices]
  }
}
```

## Blender Model Generator

### Prerequisites
- Blender 4.0+ installed (https://blender.org)
- Verify: `blender --version`

### Usage

```bash
# Generate all unit and building models
blender --background --python scripts/generate_all_blender.py -- --all

# Generate single model from JSON
blender --background --python scripts/generate_all_blender.py -- <unit.json>

# Or open Blender UI and run scripts/generate_all_blender.py
```

### Blender Model Features
- **Faction colors** auto-applied
- **Tier-based scaling** (T2+ have turrets, T3+ have sensors)
- **Role-specific geometry**:
  - Ground: Box with optional turret
  - Aircraft: Fuselage + wings + tail + nose cone
  - Naval: Hull + superstructure
  - Command center: Base + tower + top sphere
  - Production facility: Main structure + roof
  - Resource extractor: Base + tower + drill
  - Airbase: Runway + control tower

### Output
```
data/generated_units/blender/*.blend
```

## Godot Integration

### For JSON Mesh Definitions
1. Place `.mesh.json` files in `data/generated_units/meshes/`
2. Use custom loader in Godot to parse vertices/triangles
3. Create `MeshInstance3D` with generated mesh data

### For Blender Models
1. Import `.blend` or export `.gltf` from Blender
2. Load in Godot as `AnimatedSprite3D` or `MeshInstance3D`
3. Apply materials (faction colors are in material nodes)
4. Rig for animation if needed

## Asset Tree

```
data/generated_units/
├── meshes/                 # JSON mesh definitions (61 files)
│   ├── elite_*.mesh.json
│   ├── mass_*.mesh.json
│   ├── industrial_*.mesh.json
│   ├── faction_a_t*.mesh.json
│   ├── faction_b_t*.mesh.json
│   ├── faction_c_t*.mesh.json
│   └── faction_*_*.mesh.json
└── blender/                # Blender model files (generated on demand)
    ├── *.blend
    └── *.gltf (exported)
```

## Customization

### Add New Unit Types
Edit `scripts/generate_all_assets.py`:
1. Add role to unit type mapping
2. Add new generator function (e.g., `create_transport_unit_mesh()`)
3. Add to `generate_all_assets()` loop

### Add New Building Types
1. Create `create_<building>_mesh()` function
2. Add to building generation switch
3. Add to building JSON

### Adjust Scaling
Modify tier multipliers in generator:
```python
if tier == 4:
    w, h, d = w * 1.4, h * 1.4, d * 1.4
```

## Next Steps

1. **Review mesh definitions**: `cat data/generated_units/meshes/*.mesh.json`
2. **Generate Blender files**: Run Blender script
3. **Import into Godot**: Use GDExtension loader
4. **Customize**: Add textures, animations, sounds
5. **Test**: Run `./build/rts_scale_benchmark` with new assets

## Troubleshooting

### No Blender Installed
- Ubuntu/Debian: `sudo apt install blender`
- Fedora: `sudo dnf install blender`
- Arch: `sudo pacman -S blender`

### LSP Warnings for `bpy`
Expected - `bpy` is only available inside Blender runtime. Ignore in IDE.

### Path Issues
All scripts use relative paths from project root. Verify working directory is `/home/conor/repos/mandate-of-war`.
