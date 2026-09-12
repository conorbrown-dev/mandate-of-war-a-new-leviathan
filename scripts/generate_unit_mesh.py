#!/usr/bin/env python3
"""
Blender mesh data generator for RTS game.
Produces JSON mesh definitions compatible with Godot's MultiMeshInstance3D.
"""

import sys
import json
from pathlib import Path

def generate_unit_mesh(unit_data: dict) -> dict:
    """
    Generate mesh definition for a unit based on its JSON properties.
    Returns a mesh definition that can be used by Blender or procedural generation.
    """
    unit_id = unit_data.get("id", "unknown_unit")
    movement_type = unit_data.get("movement_type", "ground")
    role = unit_data.get("role", "ground")
    tier = unit_data.get("tier", 1)
    faction = unit_data.get("faction", "faction_a")
    
    size_x = unit_data.get("size_x", 2.0)
    size_y = unit_data.get("size_y", 2.0)
    size_z = unit_data.get("size_z", 2.0)
    
    # Calculate dimensions based on movement type
    if movement_type == "air":
        # Air units are more streamlined (low profile)
        width = size_x * 1.5
        height = size_z * 0.5
        depth = size_y * 1.2
    elif movement_type == "sea":
        # Naval units are low and wide
        width = size_x * 1.8
        height = size_z * 0.3
        depth = size_y * 1.4
    else:
        # Ground units
        width = size_x
        height = size_z
        depth = size_y
    
    # Determine color from faction
    faction_colors = {
        "faction_a": [0.12, 0.62, 1.0, 1.0],    # Blue
        "faction_b": [0.92, 0.20, 0.18, 1.0],   # Red
        "faction_c": [0.20, 0.85, 0.20, 1.0],   # Green
        "faction_0": [0.12, 0.62, 1.0, 1.0],    # Player (blue)
        "faction_1": [0.92, 0.20, 0.18, 1.0],   # AI (red)
    }
    color = faction_colors.get(faction, [0.5, 0.5, 0.5, 1.0])
    
    # Base mesh type depends on unit type
    if movement_type == "air":
        base_type = "aircraft"
    elif movement_type == "sea":
        base_type = "ship"
    else:
        base_type = "ground_unit"
    
    # Generate mesh definition
    mesh_data = {
        "unit_id": unit_id,
        "format_version": "1.0",
        "dimensions": {
            "width": round(width, 2),
            "height": round(height, 2),
            "depth": round(depth, 2)
        },
        "base_type": base_type,
        "role_details": {
            "role": role,
            "tier": tier,
            "faction": faction
        },
        "default_color": color,
        "lod_levels": [1.0, 0.75, 0.5, 0.25],
        "collider_type": "box",
        "geometry": {
            "type": "procedural_mesh",
            "vertices": generate_procedural_vertices(width, height, depth, role, movement_type),
            "triangles": generate_procedural_triangles(role),
            "normals": [],
            "uvs": []
        }
    }
    
    return mesh_data

def generate_procedural_vertices(width: float, height: float, depth: float, 
                                  role: str, movement_type: str) -> list:
    """Generate vertex data for the unit mesh."""
    w = width / 2
    h = height / 2
    d = depth / 2
    
    # Standard box vertices (8 corners)
    vertices = [
        # Front face
        [-w, -h, d],  # 0
        [w, -h, d],   # 1
        [w, h, d],    # 2
        [-w, h, d],   # 3
        # Back face
        [-w, -h, -d], # 4
        [w, -h, -d],  # 5
        [w, h, -d],   # 6
        [-w, h, -d],  # 7
    ]
    
    # Role-specific modifications
    if movement_type == "air":
        # Aircraft wings
        wing_w = w * 1.5
        wing_h = h * 0.1
        wing_d = d * 0.8
        
        wing_vertices = [
            [-wing_w, 0, wing_d],  # Left wing tip
            [0, 0, wing_d],        # Left wing root
            [0, -wing_h, wing_d],
            [-wing_w, -wing_h, wing_d],
            
            [wing_w, 0, wing_d],   # Right wing tip
            [0, 0, wing_d],        # Right wing root (duplicate)
            [0, -wing_h, wing_d],  # Reuse from left
            [wing_w, -wing_h, wing_d],
        ]
        vertices.extend(wing_vertices)
        
        # Nose cone
        vertices.append([0, 0, d + w * 0.5])  # Nose tip
        
    elif movement_type == "sea":
        # Ship hull additional vertices
        hull_depth = h * 0.6
        vertices.extend([
            [-w * 0.8, -h - hull_depth, d * 0.5],   # Hull left
            [w * 0.8, -h - hull_depth, d * 0.5],    # Hull right
            [0, -h - hull_depth, -d * 0.5],         # Hull back
        ])
    
    return vertices

def generate_procedural_triangles(role: str) -> list:
    """Generate triangle indices for unit mesh."""
    # Standard box faces (12 triangles, 36 indices)
    triangles = [
        # Front
        0, 1, 2,
        0, 2, 3,
        # Right
        1, 5, 6,
        1, 6, 2,
        # Back
        5, 4, 7,
        5, 7, 6,
        # Left
        4, 0, 3,
        4, 3, 7,
        # Top
        3, 2, 6,
        3, 6, 7,
        # Bottom
        4, 5, 1,
        4, 1, 0,
    ]
    
    return triangles

def generate_all_units(output_dir: Path) -> None:
    """Generate meshes for all unit types defined in unit_faction_stats.json."""
    stats_path = Path(__file__).parent.parent / "data" / "unit_faction_stats.json"
    
    with open(stats_path, 'r') as f:
        stats = json.load(f)
    
    meshes_dir = output_dir / "meshes"
    meshes_dir.mkdir(parents=True, exist_ok=True)
    
    for unit_type in stats["unit_types"]:
        unit_data = {
            "id": unit_type["type"].lower().replace("_", " ").title().replace(" ", "_"),
            "movement_type": "air" if unit_type.get("is_aircraft", False) else 
                           "sea" if unit_type.get("is_naval", False) else "ground",
            "size_x": 2.0,
            "size_y": 2.0,
            "size_z": 2.0,
            "faction": unit_type["faction"].lower().replace("_", ""),
            "tier": 1,
            "role": "ground",
        }
        
        if unit_type.get("is_aircraft"):
            unit_data["role"] = "interceptor"
            unit_data["tier"] = 1
        elif unit_type.get("is_naval"):
            unit_data["role"] = "patrol"
            unit_data["tier"] = 1
        
        mesh_def = generate_unit_mesh(unit_data)
        
        mesh_file = meshes_dir / f"{unit_data['id']}.mesh.json"
        with open(mesh_file, 'w') as f:
            json.dump(mesh_def, f, indent=2)
        
        print(f"Generated: {mesh_file.name}")

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate unit mesh definitions for RTS game")
    parser.add_argument("input", nargs="?", help="Unit JSON file path")
    parser.add_argument("--all", action="store_true", help="Generate all units from unit_faction_stats.json")
    parser.add_argument("--output", "-o", type=Path, default=Path("data/generated_units/meshes"),
                       help="Output directory for mesh definitions")
    
    args = parser.parse_args()
    
    if args.all:
        generate_all_units(args.output)
    elif args.input:
        with open(args.input, 'r') as f:
            unit_data = json.load(f)
        
        mesh_def = generate_unit_mesh(unit_data)
        
        output_path = args.output / f"{unit_data['id']}.mesh.json"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            json.dump(mesh_def, f, indent=2)
        
        print(f"Generated: {output_path}")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
