#!/usr/bin/env python3
"""
Blender add-on for RTS unit and building model generation.
Run with: blender --background --python generate_all_blender.py -- <asset_type> <faction> <tier>
Or open in Blender UI: File > Scripting > Run Script
"""

import bpy
import json
from pathlib import Path

BASE_PATH = Path(__file__).parent.parent / "data" / "generated_units"
OUTPUT_DIR = BASE_PATH / "blender"

def clear_scene():
    """Clear all objects from the scene."""
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)

def create_faction_material(faction: str, name: str):
    """Create a material for a faction."""
    faction_colors = {
        "faction_a": (0.12, 0.62, 1.0, 1.0),
        "faction_b": (0.92, 0.20, 0.18, 1.0),
        "faction_c": (0.20, 0.85, 0.20, 1.0),
        "faction_0": (0.12, 0.62, 1.0, 1.0),
        "faction_1": (0.92, 0.20, 0.18, 1.0),
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

def create_ground_unit(name: str, faction: str, tier: int, role: str):
    """Create a ground unit mesh."""
    clear_scene()
    
    w, h, d = 2.0, 1.5, 2.0
    
    if tier == 4:
        w, h, d = w * 1.4, h * 1.4, d * 1.4
    elif tier == 3:
        w, h, d = w * 1.2, h * 1.2, d * 1.2
    elif tier == 2:
        w, h, d = w * 1.1, h * 1.1, d * 1.1
    
    mat = create_faction_material(faction, f"{name}_material")
    
    # Main body
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/2))
    body = bpy.context.object
    body.name = f"{name}_body"
    body.scale = (w/2, d/2, h/2)
    body.data.materials.append(mat)
    
    if role == "heavy" and tier >= 2:
        # Turret
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.3, depth=1,
                                           location=(0, 0, h + 0.3))
        turret = bpy.context.object
        turret.name = f"{name}_turret"
        turret.scale = (w * 0.4, w * 0.4, h * 0.3)
        turret.data.materials.append(mat)
    
    if tier >= 3:
        # Sensors
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.2, location=(w * 0.4, 0, h + 0.2))
        sensor = bpy.context.object
        sensor.name = f"{name}_sensor"
        
        sensor_mat = bpy.data.materials.new(name=f"{name}_sensor")
        sensor_mat.use_nodes = True
        bsdf = sensor_mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = (1.0, 1.0, 0.0, 1.0)
        sensor.data.materials.append(sensor_mat)
    
    body.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return body

def create_air_unit(name: str, faction: str, tier: int, role: str):
    """Create an air unit mesh."""
    clear_scene()
    
    w, h, d = 2.4, 0.8, 2.8
    
    if tier == 4:
        w, h, d = w * 1.3, h * 1.2, d * 1.3
    elif tier == 3:
        w, h, d = w * 1.2, h * 1.1, d * 1.2
    
    mat = create_faction_material(faction, f"{name}_material")
    
    # Fuselage
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/4))
    fuselage = bpy.context.object
    fuselage.name = f"{name}_fuselage"
    fuselage.scale = (w/4, d/2, h/2)
    fuselage.data.materials.append(mat)
    
    # Wings
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0))
    wing_l = bpy.context.object
    wing_l.name = f"{name}_wing_l"
    wing_l.scale = (w * 0.8, d * 0.2, h * 0.5)
    wing_l.rotation_euler = (0.6, 0, 0)
    wing_l.data.materials.append(mat)
    
    wing_r = wing_l.copy()
    wing_r.name = f"{name}_wing_r"
    wing_r.data = wing_r.data.copy()
    wing_r.scale = (-w * 0.8, d * 0.2, h * 0.5)
    wing_r.rotation_euler = (0.6, 0, 0)
    wing_r.data.materials.append(mat)
    bpy.context.collection.objects.link(wing_r)
    
    # Tail
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, -d/2))
    tail = bpy.context.object
    tail.name = f"{name}_tail"
    tail.scale = (w * 0.4, d * 0.2, h * 0.5)
    tail.data.materials.append(mat)
    
    # Nose cone
    bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.1, radius2=0, depth=0.8,
                                   location=(0, 0, d/2 + w*0.15))
    nose = bpy.context.object
    nose.name = f"{name}_nose"
    nose.data.materials.append(mat)
    
    fuselage.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return fuselage

def create_naval_unit(name: str, faction: str, tier: int, role: str):
    """Create a naval unit mesh."""
    clear_scene()
    
    w, h, d = 3.2, 0.6, 5.0
    
    if tier == 4:
        w, h, d = w * 1.3, h * 1.1, d * 1.3
    elif tier == 3:
        w, h, d = w * 1.2, h * 1.05, d * 1.2
    
    mat = create_faction_material(faction, f"{name}_material")
    
    # Hull
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/4))
    hull = bpy.context.object
    hull.name = f"{name}_hull"
    hull.scale = (w/2, d/2, h/4)
    hull.data.materials.append(mat)
    
    # Superstructure
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/2))
    superstructure = bpy.context.object
    superstructure.name = f"{name}_superstructure"
    superstructure.scale = (w/4, d/4, h/2)
    superstructure.data.materials.append(mat)
    
    hull.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return hull

def create_command_center(name: str, faction: str):
    """Create command center building."""
    clear_scene()
    
    w, h, d = 8.0, 10.0, 8.0
    mat = create_faction_material(faction, f"{name}_material")
    
    # Base
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/4))
    base = bpy.context.object
    base.name = f"{name}_base"
    base.scale = (w/2, d/2, h/2)
    base.data.materials.append(mat)
    
    # Tower
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, h/2, d/4))
    tower = bpy.context.object
    tower.name = f"{name}_tower"
    tower.scale = (w/3, d/3, h/4)
    tower.data.materials.append(mat)
    
    # Top
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(0, h, 0))
    top = bpy.context.object
    top.name = f"{name}_top"
    top.data.materials.append(mat)
    
    base.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return base

def create_production_facility(name: str, faction: str):
    """Create production facility building."""
    clear_scene()
    
    w, h, d = 10.0, 5.0, 8.0
    mat = create_faction_material(faction, f"{name}_material")
    
    # Main structure
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/2))
    building = bpy.context.object
    building.name = f"{name}_main"
    building.scale = (w/2, d/2, h)
    building.data.materials.append(mat)
    
    # Roof
    bpy.ops.mesh.primitive_cylinder_add(vertices=4, radius=1, depth=1,
                                       location=(0, 0, h + 1))
    roof = bpy.context.object
    roof.name = f"{name}_roof"
    roof.scale = (w/2, d/2, 0.3)
    roof.data.materials.append(mat)
    
    building.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return building

def create_resource_extractor(name: str, faction: str):
    """Create resource extractor building."""
    clear_scene()
    
    w, h, d = 6.0, 12.0, 6.0
    mat = create_faction_material(faction, f"{name}_material")
    
    # Base
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 1))
    base = bpy.context.object
    base.name = f"{name}_base"
    base.scale = (w/2, d/2, 1)
    base.data.materials.append(mat)
    
    # Tower
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/2))
    tower = bpy.context.object
    tower.name = f"{name}_tower"
    tower.scale = (w/4, d/4, h/2)
    tower.data.materials.append(mat)
    
    # Drill
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.5, depth=1,
                                       location=(0, 0, h + 1))
    drill = bpy.context.object
    drill.name = f"{name}_drill"
    drill.scale = (w/3, w/3, 1)
    drill.data.materials.append(mat)
    
    base.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return base

def create_airbase(name: str, faction: str):
    """Create airbase building."""
    clear_scene()
    
    w, h, d = 12.0, 3.0, 4.0
    mat = create_faction_material(faction, f"{name}_material")
    
    # Runway area
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 0.15))
    runway = bpy.context.object
    runway.name = f"{name}_runway"
    runway.scale = (w/2, d/2, 0.3)
    runway.data.materials.append(mat)
    
    # Control tower
    bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, h/2))
    tower = bpy.context.object
    tower.name = f"{name}_tower"
    tower.scale = (w/4, d/4, h)
    tower.data.materials.append(mat)
    
    runway.name = name
    bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')
    
    return runway

def generate_from_json(json_path: str, output_dir: Path | None = None) -> None:
    """Generate a model from a unit JSON file."""
    with open(json_path, 'r') as f:
        unit_data = json.load(f)
    
    unit_id = unit_data.get("id", "unknown_unit")
    faction = unit_data.get("faction", "faction_a")
    tier = unit_data.get("tier", 1)
    movement_type = unit_data.get("movement_type", "ground")
    
    if output_dir is None:
        output_dir = BASE_PATH / "blender"
    
    if movement_type == "air":
        body = create_air_unit(unit_id, faction, tier, unit_data.get("role", "fighter"))
    elif movement_type == "sea":
        body = create_naval_unit(unit_id, faction, tier, unit_data.get("role", "patrol"))
    elif unit_data.get("building_type"):
        btype = unit_data.get("building_type", "command_center")
        if btype == "command_center":
            body = create_command_center(unit_id, faction)
        elif btype == "production_facility":
            body = create_production_facility(unit_id, faction)
        elif btype == "resource_extractor":
            body = create_resource_extractor(unit_id, faction)
        elif btype == "airbase":
            body = create_airbase(unit_id, faction)
        else:
            body = create_ground_unit(unit_id, faction, tier, "ground")
    else:
        body = create_ground_unit(unit_id, faction, tier, unit_data.get("role", "ground"))
    
    output_dir.mkdir(parents=True, exist_ok=True)
    blend_path = output_dir / f"{unit_id}.blend"
    bpy.ops.wm.save_mainfile(filepath=str(blend_path))
    
    print(f"Generated: {blend_path}")
    print(f"  Type: {movement_type}, Faction: {faction}, Tier: {tier}")

def generate_all_models():
    """Generate all unit and building models."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    
    # Generate units from stats
    stats_path = Path(__file__).parent.parent / "data" / "unit_faction_stats.json"
    if stats_path.exists():
        with open(stats_path, 'r') as f:
            stats = json.load(f)
        
        for unit_type in stats["unit_types"]:
            name = unit_type["type"].lower().replace("_", "_")
            faction = unit_type["faction"].lower().replace("_", "")
            
            if unit_type.get("is_aircraft"):
                create_air_unit(name, faction, 1, "fighter")
            elif unit_type.get("is_naval"):
                create_naval_unit(name, faction, 1, "patrol")
            else:
                create_ground_unit(name, faction, 1, "ground")
            
            blend_path = OUTPUT_DIR / f"{name}.blend"
            bpy.ops.wm.save_mainfile(filepath=str(blend_path))
            print(f"Generated: {blend_path.name}")
    
    # Generate faction variants
    factions = ["faction_a", "faction_b", "faction_c"]
    tiers = [2, 3, 4]
    
    for faction in factions:
        for tier in tiers:
            if faction == "faction_a":
                roles = ["interceptor", "fighter", "heavy", "support"]
            elif faction == "faction_b":
                roles = ["ground", "heavy", "fighter", "bomber"]
            else:
                roles = ["ground", "heavy", "interceptor", "support"]
            
            for role in roles:
                if tier == 2 and role in ["interceptor", "fighter", "heavy", "ground"]:
                    if role in ["interceptor", "fighter"]:
                        create_air_unit(f"{faction}_t{tier}_{role}", faction, tier, role)
                    elif role == "heavy":
                        create_ground_unit(f"{faction}_t{tier}_heavy", faction, tier, "heavy")
                    else:
                        create_ground_unit(f"{faction}_t{tier}_ground", faction, tier, "ground")
                    
                    blend_path = OUTPUT_DIR / f"{faction}_t{tier}_{role}.blend"
                    bpy.ops.wm.save_mainfile(filepath=str(blend_path))
                    print(f"Generated: {blend_path.name}")
    
    # Generate buildings
    buildings = ["command_center", "production_facility", "resource_extractor", "airbase"]
    for faction in factions:
        for building in buildings:
            if building == "command_center":
                create_command_center(f"{faction}_{building}", faction)
            elif building == "production_facility":
                create_production_facility(f"{faction}_{building}", faction)
            elif building == "resource_extractor":
                create_resource_extractor(f"{faction}_{building}", faction)
            elif building == "airbase":
                create_airbase(f"{faction}_{building}", faction)
            
            blend_path = OUTPUT_DIR / f"{faction}_{building}.blend"
            bpy.ops.wm.save_mainfile(filepath=str(blend_path))
            print(f"Generated: {blend_path.name}")
    
    print(f"\nTotal models generated:See Blender output window")

if __name__ == "__main__":
    import sys
    
    if "--all" in sys.argv:
        generate_all_models()
    elif len(sys.argv) > 2 and not sys.argv[-1].startswith("-"):
        generate_from_json(sys.argv[-1])
