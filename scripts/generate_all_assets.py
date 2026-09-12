#!/usr/bin/env python3
"""
Comprehensive model generator for RTS game units and buildings.
Produces JSON mesh definitions for Blender and Godot.
"""

import sys
import json
from pathlib import Path

BASE_PATH = Path(__file__).parent.parent / "data" / "generated_units"

def get_faction_color(faction: str) -> list:
    """Get RGBA color for faction."""
    colors = {
        "faction_a": [0.12, 0.62, 1.0, 1.0],
        "faction_b": [0.92, 0.20, 0.18, 1.0],
        "faction_c": [0.20, 0.85, 0.20, 1.0],
        "faction_0": [0.12, 0.62, 1.0, 1.0],
        "faction_1": [0.92, 0.20, 0.18, 1.0],
        "elite_precision": [0.12, 0.62, 1.0, 1.0],
        "mass_warfare": [0.92, 0.20, 0.18, 1.0],
        "industrial_experimental": [0.20, 0.85, 0.20, 1.0],
    }
    return colors.get(faction.lower(), [0.5, 0.5, 0.5, 1.0])

def create_ground_unit_mesh(unit_id: str, faction: str, tier: int, role: str) -> dict:
    """Create ground unit mesh definition."""
    w = 2.0
    h = 1.5
    d = 2.0
    
    # Tier-based scaling
    if tier == 4:
        w *= 1.4
        h *= 1.4
        d *= 1.4
    elif tier == 3:
        w *= 1.2
        h *= 1.2
        d *= 1.2
    elif tier == 2:
        w *= 1.1
        h *= 1.1
        d *= 1.1
    
    color = get_faction_color(faction)
    
    # Role-specific geometry
    if role == "heavy":
        vertices = [
            # Main body
            [-w/2, -h/2, d/2], [w/2, -h/2, d/2], [w/2, h/2, d/2], [-w/2, h/2, d/2],  # 0-3 front
            [-w/2, -h/2, -d/2], [w/2, -h/2, -d/2], [w/2, h/2, -d/2], [-w/2, h/2, -d/2],  # 4-7 back
            # Turret base
            [-w*0.3, h/2, d*0.1], [w*0.3, h/2, d*0.1], [w*0.3, h/2 + h*0.3, d*0.1], [-w*0.3, h/2 + h*0.3, d*0.1],  # 8-11
        ]
        triangles = [
            # Front
            0, 1, 2, 0, 2, 3,
            # Right
            1, 5, 6, 1, 6, 2,
            # Back
            5, 4, 7, 5, 7, 6,
            # Left
            4, 0, 3, 4, 3, 7,
            # Top body
            3, 2, 6, 3, 6, 7,
            # Bottom
            4, 5, 1, 4, 1, 0,
            # Turret base
            8, 9, 10, 8, 10, 11,
            # Turret front
            8, 11, 10, 8, 10, 9,
        ]
        
    elif role == "interceptor" or role == "fighter":
        vertices = [
            # Body
            [-w/2, -h/2, d/2], [w/2, -h/2, d/2], [w/2, h/2, d/2], [-w/2, h/2, d/2],  # 0-3
            [-w/2, -h/2, -d/2], [w/2, -h/2, -d/2], [w/2, h/2, -d/2], [-w/2, h/2, -d/2],  # 4-7
            # Wings
            [-w*1.3, -h/2, d*0.5], [0, -h/2, d*0.5], [0, 0, d*0.5], [-w*1.3, 0, d*0.5],  # 8-11 left
            [w*1.3, -h/2, d*0.5], [0, -h/2, d*0.5], [0, 0, d*0.5], [w*1.3, 0, d*0.5],  # 12-15 right
            # Nose
            [0, 0, d/2 + w*0.3],  # 16
        ]
        triangles = [
            # Body
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
            # Left wing
            8, 9, 10, 8, 10, 11,
            # Right wing
            12, 13, 14, 12, 14, 15,
            # Nose
            3, 2, 16, 0, 3, 16, 2, 1, 16, 3, 0, 16,
        ]
        
    else:  # Standard ground unit
        vertices = [
            [-w/2, -h/2, d/2], [w/2, -h/2, d/2], [w/2, h/2, d/2], [-w/2, h/2, d/2],  # 0-3
            [-w/2, -h/2, -d/2], [w/2, -h/2, -d/2], [w/2, h/2, -d/2], [-w/2, h/2, -d/2],  # 4-7
        ]
        triangles = [
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        ]
    
    return {
        "unit_id": unit_id,
        "format_version": "1.0",
        "dimensions": {"width": w, "height": h, "depth": d},
        "base_type": "ground_unit",
        "role_details": {"role": role, "tier": tier, "faction": faction},
        "default_color": color,
        "lod_levels": [1.0, 0.75, 0.5, 0.25],
        "collider_type": "box",
        "geometry": {
            "type": "procedural_mesh",
            "vertices": vertices,
            "triangles": triangles,
            "normals": [],
            "uvs": []
        }
    }

def create_air_unit_mesh(unit_id: str, faction: str, tier: int, role: str) -> dict:
    """Create air unit mesh definition."""
    w = 2.4
    h = 0.8
    d = 2.8
    
    if tier == 4:
        w *= 1.3
        h *= 1.2
        d *= 1.3
    elif tier == 3:
        w *= 1.2
        h *= 1.1
        d *= 1.2
    
    color = get_faction_color(faction)
    
    vertices = [
        # Fuselage
        [-w/4, -h/2, d/2], [w/4, -h/2, d/2], [w/4, h/4, d/2], [-w/4, h/4, d/2],  # 0-3 front
        [-w/4, -h/2, -d/2], [w/4, -h/2, -d/2], [w/4, h/4, -d/2], [-w/4, h/4, -d/2],  # 4-7 back
        # Main wings
        [-w*0.8, -h/2, d*0.2], [0, -h/2, d*0.2], [0, 0, d*0.2], [-w*0.8, 0, d*0.2],  # 8-11 left
        [w*0.8, -h/2, d*0.2], [0, -h/2, d*0.2], [0, 0, d*0.2], [w*0.8, 0, d*0.2],  # 12-15 right
        # Tail wings
        [-w*0.4, -h/2, -d*0.4], [w*0.4, -h/2, -d*0.4], [w*0.4, h/2, -d*0.4], [-w*0.4, h/2, -d*0.4],  # 16-19
        # Nose tip
        [0, 0, d/2 + w*0.2],  # 20
    ]
    
    triangles = [
        # Fuselage
        0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
        5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
        3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        # Left wing
        8, 9, 10, 8, 10, 11,
        # Right wing
        12, 13, 14, 12, 14, 15,
        # Tail
        16, 17, 18, 16, 18, 19,
        # Nose
        3, 2, 20, 0, 3, 20, 2, 1, 20, 3, 0, 20,
    ]
    
    return {
        "unit_id": unit_id,
        "format_version": "1.0",
        "dimensions": {"width": w, "height": h, "depth": d},
        "base_type": "aircraft",
        "role_details": {"role": role, "tier": tier, "faction": faction},
        "default_color": color,
        "lod_levels": [1.0, 0.75, 0.5, 0.25],
        "collider_type": "box",
        "geometry": {
            "type": "procedural_mesh",
            "vertices": vertices,
            "triangles": triangles,
            "normals": [],
            "uvs": []
        }
    }

def create_naval_unit_mesh(unit_id: str, faction: str, tier: int, role: str) -> dict:
    """Create naval unit mesh definition."""
    w = 3.2
    h = 0.6
    d = 5.0
    
    if tier == 4:
        w *= 1.3
        h *= 1.1
        d *= 1.3
    elif tier == 3:
        w *= 1.2
        h *= 1.05
        d *= 1.2
    
    color = get_faction_color(faction)
    
    vertices = [
        # Hull (main)
        [-w/2, -h/2, d/2], [w/2, -h/2, d/2], [w/2, h/4, d/2], [-w/2, h/4, d/2],  # 0-3 front
        [-w/2, -h/2, -d/2], [w/2, -h/2, -d/2], [w/2, h/4, -d/2], [-w/2, h/4, -d/2],  # 4-7 back
        # Superstructure
        [-w/4, h/4, d/4], [w/4, h/4, d/4], [w/4, h/2, d/4], [-w/4, h/2, d/4],  # 8-11
        [-w/4, h/4, -d/4], [w/4, h/4, -d/4], [w/4, h/2, -d/4], [-w/4, h/2, -d/4],  # 12-15
    ]
    
    triangles = [
        # Hull front
        0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
        # Hull back
        5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
        # Hull top
        3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        # Superstructure front
        8, 9, 10, 8, 10, 11, 9, 12, 13, 9, 13, 10,
        # Superstructure back
        12, 15, 14, 12, 14, 13, 15, 11, 10, 15, 10, 14,
        # Superstructure sides
        11, 8, 3, 11, 3, 7, 15, 12, 4, 15, 4, 0,
    ]
    
    return {
        "unit_id": unit_id,
        "format_version": "1.0",
        "dimensions": {"width": w, "height": h, "depth": d},
        "base_type": "ship",
        "role_details": {"role": role, "tier": tier, "faction": faction},
        "default_color": color,
        "lod_levels": [1.0, 0.75, 0.5, 0.25],
        "collider_type": "box",
        "geometry": {
            "type": "procedural_mesh",
            "vertices": vertices,
            "triangles": triangles,
            "normals": [],
            "uvs": []
        }
    }

def create_building_mesh(building_id: str, faction: str, btype: str, tier: int) -> dict:
    """Create building mesh definition."""
    w = 8.0
    h = 6.0
    d = 8.0
    
    color = get_faction_color(faction)
    
    if btype == "command_center":
        h = 10.0
        vertices = [
            # Base
            [-w/2, 0, d/2], [w/2, 0, d/2], [w/2, h/2, d/2], [-w/2, h/2, d/2],  # 0-3
            [-w/2, 0, -d/2], [w/2, 0, -d/2], [w/2, h/2, -d/2], [-w/2, h/2, -d/2],  # 4-7
            # Tower
            [-w/3, h/2, d/3], [w/3, h/2, d/3], [w/3, h*0.9, d/3], [-w/3, h*0.9, d/3],  # 8-11
            [-w/3, h/2, -d/3], [w/3, h/2, -d/3], [w/3, h*0.9, -d/3], [-w/3, h*0.9, -d/3],  # 12-15
            # Top
            [-w/4, h*0.9, 0], [w/4, h*0.9, 0], [w/4, h, 0], [-w/4, h, 0],  # 16-19
        ]
        triangles = [
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
            8, 9, 10, 8, 10, 11, 9, 12, 13, 9, 13, 10,
            12, 15, 14, 12, 14, 13, 15, 11, 10, 15, 10, 14,
            11, 8, 3, 11, 3, 7, 15, 12, 4, 15, 4, 0,
            16, 17, 18, 16, 18, 19, 17, 16, 19, 17, 19, 18,
        ]
        
    elif btype == "production_facility":
        h = 5.0
        vertices = [
            [-w/2, 0, d/2], [w/2, 0, d/2], [w/2, h, d/2], [-w/2, h, d/2],  # 0-3
            [-w/2, 0, -d/2], [w/2, 0, -d/2], [w/2, h, -d/2], [-w/2, h, -d/2],  # 4-7
            [-w/2, 0, d/4], [w/2, 0, d/4], [w/2, h, d/4], [-w/2, h, d/4],  # 8-11
        ]
        triangles = [
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
            8, 9, 10, 8, 10, 11, 11, 10, 2, 11, 2, 3,
        ]
        
    elif btype == "resource_extractor":
        h = 8.0
        vertices = [
            [-w/4, 0, d/4], [w/4, 0, d/4], [w/4, h*0.8, d/4], [-w/4, h*0.8, d/4],  # 0-3 base
            [-w/4, 0, -d/4], [w/4, 0, -d/4], [w/4, h*0.8, -d/4], [-w/4, h*0.8, -d/4],  # 4-7
            [-w/2, h*0.8, 0], [w/2, h*0.8, 0], [w/2, h, 0], [-w/2, h, 0],  # 8-11 top
        ]
        triangles = [
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
            8, 9, 10, 8, 10, 11, 11, 10, 2, 11, 2, 3,
        ]
        
    elif btype == "airbase":
        w = 12.0
        d = 4.0
        h = 3.0
        vertices = [
            [-w/2, 0, d/2], [w/2, 0, d/2], [w/2, h/2, d/2], [-w/2, h/2, d/2],  # 0-3
            [-w/2, 0, -d/2], [w/2, 0, -d/2], [w/2, h/2, -d/2], [-w/2, h/2, -d/2],  # 4-7
            [-w/2, 0, d/4], [w/2, 0, d/4], [w/2, h/2, d/4], [-w/2, h/2, d/4],  # 8-11
        ]
        triangles = [
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
            8, 9, 10, 8, 10, 11, 11, 10, 2, 11, 2, 3,
        ]
        
    else:  # Basic structure
        vertices = [
            [-w/2, 0, d/2], [w/2, 0, d/2], [w/2, h, d/2], [-w/2, h, d/2],  # 0-3
            [-w/2, 0, -d/2], [w/2, 0, -d/2], [w/2, h, -d/2], [-w/2, h, -d/2],  # 4-7
        ]
        triangles = [
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        ]
    
    return {
        "unit_id": building_id,
        "format_version": "1.0",
        "dimensions": {"width": w, "height": h, "depth": d},
        "base_type": "building",
        "role_details": {"role": btype, "tier": tier, "faction": faction},
        "default_color": color,
        "lod_levels": [1.0, 0.75, 0.5, 0.25],
        "collider_type": "box",
        "geometry": {
            "type": "procedural_mesh",
            "vertices": vertices,
            "triangles": triangles,
            "normals": [],
            "uvs": []
        }
    }

def generate_all_assets(output_dir: Path | None = None) -> None:
    """Generate all units and buildings."""
    if output_dir is None:
        output_dir = BASE_PATH / "meshes"
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    assets = []
    
    # Units from unit_faction_stats.json
    stats_path = Path(__file__).parent.parent / "data" / "unit_faction_stats.json"
    if stats_path.exists():
        with open(stats_path, 'r') as f:
            stats = json.load(f)
        
        for unit_type in stats["unit_types"]:
            name = unit_type["type"].lower().replace("_", "_")
            faction = unit_type["faction"].lower().replace("_", "")
            
            if unit_type.get("is_aircraft"):
                mesh = create_air_unit_mesh(name, faction, 1, "fighter")
            elif unit_type.get("is_naval"):
                mesh = create_naval_unit_mesh(name, faction, 1, "patrol")
            else:
                mesh = create_ground_unit_mesh(name, faction, 1, "ground")
            
            assets.append(mesh)
    
    # Additional faction_a units
    factions = ["faction_a", "faction_b", "faction_c"]
    tiers = [1, 2, 3, 4]
    roles = {
        "faction_a": ["interceptor", "fighter", "heavy", "support"],
        "faction_b": ["ground", "heavy", "fighter", "bomber"],
        "faction_c": ["ground", "heavy", "interceptor", "support"],
    }
    
    for faction in factions:
        for tier in tiers[1:]:
            for role in roles.get(faction, ["ground"]):
                if tier == 1 and role == "ground":
                    continue  # Skip tier 1 ground, already covered
                    
                if role in ["interceptor", "fighter", "bomber"]:
                    mesh = create_air_unit_mesh(f"{faction}_t{tier}_{role}", faction, tier, role)
                elif role == "heavy":
                    mesh = create_ground_unit_mesh(f"{faction}_t{tier}_heavy", faction, tier, "heavy")
                elif role == "patrol":
                    mesh = create_naval_unit_mesh(f"{faction}_t{tier}_patrol", faction, tier, "patrol")
                else:
                    mesh = create_ground_unit_mesh(f"{faction}_t{tier}_{role}", faction, tier, "ground")
                
                assets.append(mesh)
    
    # Buildings
    buildings = ["command_center", "production_facility", "resource_extractor", "airbase"]
    for faction in factions:
        for building in buildings:
            mesh = create_building_mesh(f"{faction}_{building}", faction, building, 1)
            assets.append(mesh)
    
    # Save all meshes
    for mesh in assets:
        mesh_path = output_dir / f"{mesh['unit_id']}.mesh.json"
        with open(mesh_path, 'w') as f:
            json.dump(mesh, f, indent=2)
        print(f"Generated: {mesh_path.name}")
    
    print(f"\nTotal assets generated: {len(assets)}")

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate RTS unit and building mesh definitions")
    parser.add_argument("--all", action="store_true", help="Generate all units and buildings")
    parser.add_argument("input", nargs="?", help="Unit/Building JSON file path")
    parser.add_argument("--output", "-o", type=Path, default=BASE_PATH / "meshes",
                       help="Output directory for mesh definitions")
    
    args = parser.parse_args()
    
    if args.all:
        generate_all_assets()
    elif args.input:
        with open(args.input, 'r') as f:
            unit_data = json.load(f)
        
        movement_type = unit_data.get("movement_type", "ground")
        role = unit_data.get("role", "ground")
        faction = unit_data.get("faction", "faction_a")
        tier = unit_data.get("tier", 1)
        
        if movement_type == "air":
            mesh = create_air_unit_mesh(unit_data["id"], faction, tier, role)
        elif movement_type == "sea":
            mesh = create_naval_unit_mesh(unit_data["id"], faction, tier, role)
        elif unit_data.get("building_type"):
            mesh = create_building_mesh(unit_data["id"], faction, unit_data["building_type"], tier)
        else:
            mesh = create_ground_unit_mesh(unit_data["id"], faction, tier, role)
        
        output_path = args.output / f"{unit_data['id']}.mesh.json"
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'w') as f:
            json.dump(mesh, f, indent=2)
        
        print(f"Generated: {output_path}")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
