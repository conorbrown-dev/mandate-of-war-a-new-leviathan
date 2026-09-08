# Asset Pipeline

## Objective

Build an automated asset pipeline centered on Blender, enabling content expansion without hand-authoring every asset. The pipeline must not require proprietary cloud services.

## Pipeline Architecture

```
content_spec/
  units/
    t1_interceptor/
      specification.yaml  # design spec (faction, tier, role, lineage)
      overrides/
        model.blend     # optional artist-approved model
        material.mat    # optional artist-approved material
  factions/
    faction_a/
      specification.yaml
      palette/

pipeline/
  blender/
    generator.py        # main generation script
    operator/           # individual generation operators
  generate.py           # CLI entry point
  cache/                # generated assets and metadata
```

## Input Specification

### Unit Specification

```yaml
id: faction_a|t1_interceptor
faction: faction_a
tier: 1
role: interceptor
dimensions:
  length: 12.0
  wingspan: 14.0
  height: 3.5
movement:
  type: air
  speed: 220
  range: 3000
weapons:
  - primary: cannon
    count: 2
  - secondary: air_to_air_missile
    count: 4
lineage:
  primary: p51_mustang
  secondary: lightweight_modern_fighter
preserve:
  - aggressive_nose
  - canopy_profile
  - compact_fuselage
modernization:
  - turbofan_engine
  - composite_skin
  - missile_hardpoints
  - modern_sensor_package
faction_style: mass_warfare
cost:
  material: 1500
  energy: 1000
  research: 800
  build_time: 60
```

## Generation Operators

### 1. Mesh Generation

Generate procedural meshes from specification:

```python
def generate_mesh(spec):
    if spec.role == "interceptor":
        base = create_airfoil_fuselage(spec.dimensions)
    elif spec.role == "bomber":
        base = create_broad_fuselage(spec.dimensions)
    else:
        base = create_generic_aircraft(spec.dimensions)
    
    apply_preserved_features(base, spec.lineage.preserve)
    apply_modernization(base, spec.lineage.modernization)
    
    return base
```

### 2. UV Unwrapping

```python
def generate_uvs(mesh):
    bpy.ops.uv.smart_project(angle_limit=66.0, island_margin=0.05)
    apply_faction_mask(mesh, spec.faction)
```

### 3. Material Generation

```python
def generate_materials(mesh, spec):
    base_color = spec.faction.colors.primary
    roughness = 0.3 + (spec.tier * 0.1)
    material = create_pbr_material(
        base_color=base_color,
        roughness=roughness,
        metallic=0.2
    )
    mesh.active_material = material
```

### 4. Collision Mesh Generation

```python
def generate_collision_mesh(mesh):
    collision = mesh.copy()
    decimate_to_convex_hull(collision)
    collision.name = f"{mesh.name}_collision"
    return collision
```

### 5. LOD Generation

```python
def generate_lods(mesh):
    lods = []
    for level in range(1, 4):
        lod = mesh.copy()
        decimate(lod, ratio=1.0 / (2 ** level))
        lod.name = f"{mesh.name}_LOD{level}"
        lods.append(lod)
    return lods
```

### 6. Icon Generation

```python
def generate_icon(mesh, output_path):
    setup_viewport(mesh, view="diagonal")
    render_to_file(output_path / f"{mesh.name}.png", resolution=256)
```

## CLI Interface

```bash
python generate.py unit \
    --spec units/t1_interceptor/specification.yaml \
    --output cache/units/t1_interceptor

python generate.py faction \
    --spec factions/faction_a/specification.yaml \
    --output cache/factions/faction_a

python generate.py validate \
    --cache cache/units/t1_interceptor

python generate.py export \
    --input cache/units/t1_interceptor \
    --output godot/import/t1_interceptor
```

## Cache System

```
cache/
  units/
    t1_interceptor/
      meshes/
        t1_interceptor.blend
        t1_interceptor_collision.blend
        t1_interceptor_LOD1.blend
        t1_interceptor_LOD2.blend
        t1_interceptor_LOD3.blend
      materials/
        t1_interceptor.mat
      textures/
        t1_interceptor_albedo.png
        t1_interceptor_normal.png
        t1_interceptor_roughness.png
      metadata/
        generated_at: 2026-09-04T12:34:56Z
        spec_hash: b7a38f2e
        dimensions: { length: 12.0, wingspan: 14.0, height: 3.5 }
        generated_by: pipeline/v1.0
```

## Faction Palettes

```yaml
faction_a:
  name: "Faction Alpha"
  colors:
    primary: [0.2, 0.4, 0.8, 1.0]
    secondary: [0.8, 0.8, 0.9, 1.0]
    accent: [0.0, 0.6, 1.0, 1.0]
  texture_styles:
    - noise_grunge
    - brushed_metal
    - matte_finish
  tech_level: 2

faction_b:
  name: "Faction Beta"
  colors:
    primary: [0.8, 0.2, 0.2, 1.0]
    secondary: [0.3, 0.3, 0.3, 1.0]
    accent: [1.0, 0.6, 0.0, 1.0]
  texture_styles:
    - geometric_patterns
    - polished_metal
    - glossy_finish
  tech_level: 2
```

## Integration with Game Engine

### Runtime Loading

```cpp
struct AssetRegistry {
    std::unordered_map<ContentId, AssetHandle> assets;
    
    AssetHandle load_asset(const ContentId& id);
    MeshData get_mesh(const AssetHandle& handle);
    MaterialData get_material(const AssetHandle& handle);
};
```

## Performance Targets

| Asset Type | Generation Time | File Size (LOD0) | File Size (LOD3) |
|------------|-----------------|------------------|------------------|
| Aircraft (small) | < 2s | < 500 KB | < 100 KB |
| Aircraft (large) | < 5s | < 2 MB | < 500 KB |
| Naval (destroyer) | < 10s | < 5 MB | < 1.5 MB |
| Naval (carrier) | < 20s | < 20 MB | < 5 MB |

## Future Enhancements

1. GPU-accelerated mesh generation
2. AI-assisted texturing
3. Physically-based material generation
4. Procedural weapon placement
5. Animation rigging automation
6. Performance optimization suggestions
