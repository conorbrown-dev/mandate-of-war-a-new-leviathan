#!/usr/bin/env python3
"""
Blender add-on for RTS unit model generation.
Run this inside Blender: File > Scripting > Run Script
"""

import bpy
import json
from pathlib import Path

def clear_scene():
    """Clear all objects from the scene."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)

def create_faction_material(faction: str, name: str) -> bpy.types.Material:
    """Create a material for a faction."""
    faction_colors = {
        "faction_a": (0.12, 0.62, 1.0, 1.0),    # Blue
        "faction_b": (0.92, 0.20, 0.18, 1.0),   # Red
        "faction_c": (0.20, 0.85, 0.20, 1.0),   # Green
        "faction_0": (0.12, 0.62, 1.0, 1.0),    # Player (blue)
        "faction_1": (0.92, 0.20, 0.18, 1.0),   # AI (red)
        "elite_precision": (0.12, 0.62, 1.0, 1.0),
        "mass_warfare": (0.92, 0.20, 0.18, 1.0),
        "industrial_experimental": (0.20, 0.85, 0.20, 1.0),
    }
    
    color = faction_colors.get(faction.lower(), (0.5, 0.5, 0.5, 1.0))
    
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = 0.68
        bsdf.inputs["Metallic"].default_value = 0.3
    
    return mat

def create_ground_unit(name: str, faction: str, tier: int, 
                       width: float = 2.0, height: float = 2.0, depth: float = 2.0):
    """Create a ground unit mesh."""
    clear_scene()
    
    # Create main body
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, height/2))
    body = bpy.context.object
    body.name = f"{name}_body"
    body.scale = (width/2, depth/2, height/2)
    
    mat = create_faction_material(faction, f"{name}_material")
    body.data.materials.append(mat)
    
    # Add details based on tier
    if tier >= 2:
        # Turret for tier 2+
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.3, depth=1,
                                           location=(0, 0, height + 0.3))
        turret = bpy.context.object
        turret.name = f"{name}_turret"
        turret.scale = (width * 0.4, width * 0.4, height * 0.3)
        turret.data.materials.append(mat)
    
    if tier >= 3:
        # Additional equipment for tier 3+
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.2, location=(width * 0.4, 0, height + 0.2))
        sensor = bpy.context.object
        sensor.name = f"{name}_sensor"
        
        sensor_mat = bpy.data.materials.new(name=f"{name}_sensor")
        sensor_mat.use_nodes = True
        bsdf = sensor_mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (1.0, 1.0, 0.0, 1.0)  # Yellow
        sensor.data.materials.append(sensor_mat)
    
    body.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return body

def create_air_unit(name: str, faction: str, tier: int,
                    width: float = 2.0, height: float = 1.0, depth: float = 2.4):
    """Create an air unit mesh."""
    clear_scene()
    
    # Main body (low profile)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, height/2))
    body = bpy.context.object
    body.name = f"{name}_body"
    body.scale = (width/2, depth/2, height/2)
    
    mat = create_faction_material(faction, f"{name}_material")
    body.data.materials.append(mat)
    
    # Wings
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0))
    wing_l = bpy.context.object
    wing_l.name = f"{name}_wing_l"
    wing_l.scale = (width * 1.2, height * 0.1, depth * 0.4)
    wing_l.rotation_euler = (0.6, 0, 0)
    wing_l.data.materials.append(mat)
    
    wing_r = wing_l.copy()
    wing_r.name = f"{name}_wing_r"
    wing_r.data = wing_r.data.copy()
    wing_r.scale = (-width * 1.2, height * 0.1, depth * 0.4)
    wing_r.rotation_euler = (0.6, 0, 0)
    wing_r.data.materials.append(mat)
    bpy.context.collection.objects.link(wing_r)
    
    # Nose cone
    bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.1, radius2=0, depth=0.8,
                                   location=(0, 0, depth/2 + 0.4))
    nose = bpy.context.object
    nose.name = f"{name}_nose"
    nose.data.materials.append(mat)
    
    body.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return body

def create_naval_unit(name: str, faction: str, tier: int,
                      width: float = 3.0, height: float = 0.6, depth: float = 4.0):
    """Create a naval unit mesh."""
    clear_scene()
    
    # Hull (low profile)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, height/2))
    body = bpy.context.object
    body.name = f"{name}_hull"
    body.scale = (width/2, depth/2, height/2)
    
    mat = create_faction_material(faction, f"{name}_material")
    body.data.materials.append(mat)
    
    # Superstructure
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, height + 0.3))
    superstructure = bpy.context.object
    superstructure.name = f"{name}_superstructure"
    superstructure.scale = (width * 0.3, depth * 0.2, height * 0.4)
    superstructure.data.materials.append(mat)
    
    body.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return body

def generate_from_json(json_path: str, output_dir: Path | None = None) -> None:
    """Generate a unit mesh from a unit JSON file."""
    with open(json_path, 'r') as f:
        unit_data = json.load(f)
    
    unit_id = unit_data.get("id", "unknown_unit")
    faction = unit_data.get("faction", "faction_a")
    tier = unit_data.get("tier", 1)
    movement_type = unit_data.get("movement_type", "ground")
    
    width = unit_data.get("size_x", 2.0)
    height = unit_data.get("size_z", 2.0)
    depth = unit_data.get("size_y", 2.0)
    
    if output_dir is None:
        output_dir = Path(json_path).parent.parent / "blender"
    
    if movement_type == "air":
        body = create_air_unit(unit_id, faction, tier, width, height * 0.5, depth * 1.2)
    elif movement_type == "sea":
        body = create_naval_unit(unit_id, faction, tier, width * 1.2, height * 0.3, depth * 1.3)
    else:
        body = create_ground_unit(unit_id, faction, tier, width, height, depth)
    
    output_dir.mkdir(parents=True, exist_ok=True)
    blend_path = output_dir / f"{unit_id}.blend"
    bpy.ops.wm.save_mainfile(filepath=str(blend_path))
    
    print(f"Generated: {blend_path}")
    print(f"  Type: {movement_type}, Faction: {faction}, Tier: {tier}")

def generate_all_units_from_stats(stats_path: Path, output_dir: Path) -> None:
    """Generate meshes for all units in unit_faction_stats.json."""
    if stats_path is None:
        stats_path = Path(__file__).resolve().parents[1] / "data" / "unit_faction_stats.json"
    
    if output_dir is None:
        output_dir = stats_path.parent / "blender"
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    with open(stats_path, 'r') as f:
        stats = json.load(f)
    
    for unit_type in stats["unit_types"]:
        name = unit_type["type"].lower().replace("_", "_")
        faction = unit_type["faction"].lower().replace("_", "")
        
        if unit_type.get("is_aircraft"):
            create_air_unit(name, faction, 1)
        elif unit_type.get("is_naval"):
            create_naval_unit(name, faction, 1)
        else:
            create_ground_unit(name, faction, 1)
        
        blend_path = output_dir / f"{name}.blend"
        bpy.ops.wm.save_mainfile(filepath=str(blend_path))
        print(f"Generated: {blend_path.name}")

if __name__ == "__main__":
    import sys
    import json
    from pathlib import Path
    
    if len(sys.argv) > 1:
        json_path = sys.argv[1]
        generate_from_json(json_path)
    else:
        print("Blender RTS Unit Generator")
        print("Usage: blender --background --python generate_unit_blender.py -- <unit_json_path>")
        print("Or run interactively in Blender's Scripting workspace")
