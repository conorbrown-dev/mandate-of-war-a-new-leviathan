#!/usr/bin/env python3
"""
Unit generation tool for RTS game asset pipeline.
Generates unit definitions, meshes, materials, and metadata from faction/tier/role specifications.
"""

import argparse
import json
import os
import sys
from pathlib import Path

# Base path for generated assets
BASE_PATH = Path(__file__).parent.parent / "data" / "generated_units"

def generate_unit(faction: str, tier: int, role: str, **kwargs) -> dict:
    """Generate a unit definition from faction/tier/role parameters."""
    unit_id = f"{faction}_t{tier}_{role}"
    
    # Base stats by tier
    tier_modifiers = {
        1: {"cost": 100, "health": 100, "speed": 2.0, "range": 30},
        2: {"cost": 250, "health": 250, "speed": 2.5, "range": 40},
        3: {"cost": 600, "health": 600, "speed": 3.0, "range": 50},
        4: {"cost": 1500, "health": 1500, "speed": 3.5, "range": 60},
    }
    
    base = tier_modifiers.get(tier, tier_modifiers[1])
    
    unit = {
        "id": unit_id,
        "name": f"{role.title()} {faction.upper()} T{tier}",
        "faction": faction,
        "tier": tier,
        "role": role,
        "cost": base["cost"],
        "health": base["health"],
        "speed": base["speed"],
        "max_speed": base["speed"],
        "turn_rate": 1.0,
        "view_range": base["range"],
        "attack_range": base["range"],
        "attack_damage": base["cost"] // 10,
        "attack_cooldown": 2.0,
        "movement_type": "ground" if role != "interceptor" and role != "fighter" else "air",
        "size_x": 2.0,
        "size_y": 2.0,
        "size_z": 2.0,
    }
    
    # Role-specific modifiers
    if role == "interceptor":
        unit["attack_cooldown"] = 1.0
        unit["speed"] *= 1.2
        unit["view_range"] = int(unit["view_range"] * 1.3)
        unit["role"] = "interceptor"
    elif role == "fighter":
        unit["attack_cooldown"] = 1.5
        unit["speed"] *= 1.1
        unit["attack_range"] = int(unit["attack_range"] * 1.2)
        unit["health"] = int(unit["health"] * 0.9)
        unit["role"] = "fighter"
    elif role == "bomber":
        unit["attack_cooldown"] = 3.0
        unit["speed"] *= 0.9
        unit["attack_damage"] *= 2
        unit["health"] = int(unit["health"] * 1.3)
        unit["movement_type"] = "air"
        unit["role"] = "bomber"
    elif role == "heavy":
        unit["speed"] *= 0.7
        unit["health"] = int(unit["health"] * 1.5)
        unit["cost"] = int(unit["cost"] * 1.3)
        unit["attack_cooldown"] = unit["attack_cooldown"] * 1.5
        unit["attack_range"] = int(unit["attack_range"] * 0.8)
        unit["role"] = "heavy"
    elif role == "support":
        unit["speed"] *= 0.8
        unit["health"] = int(unit["health"] * 0.8)
        unit["cost"] = int(unit["cost"] * 0.7)
        unit["attack_damage"] = int(unit["attack_damage"] * 0.5)
        unit["attack_range"] = int(unit["attack_range"] * 1.5)
        unit["role"] = "support"
    
    # Do not let omitted optional CLI arguments turn valid defaults into JSON null.
    for key, value in kwargs.items():
        if key in unit and value is not None:
            unit[key] = value
    
    return unit

def save_unit(unit: dict, output_dir: Path) -> Path:
    """Save unit definition to JSON file."""
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"{unit['id']}.json"
    
    with open(output_path, "w") as f:
        json.dump(unit, f, indent=2)
    
    return output_path

def generate_mesh(unit: dict, output_dir: Path) -> Path:
    """Generate a procedural placeholder mesh for the unit."""
    mesh_dir = output_dir / "meshes"
    mesh_dir.mkdir(parents=True, exist_ok=True)
    
    # Simple voxel-based mesh representation
    mesh_data = {
        "format_version": "1.0",
        "unit_id": unit["id"],
        "type": "procedural_voxel",
        "dimensions": {
            "width": unit["size_x"],
            "height": unit["size_z"],
            "depth": unit["size_y"],
        },
        "geometry": "box",
        "lod_levels": [1.0, 0.5, 0.25],
    }
    
    mesh_path = mesh_dir / f"{unit['id']}.mesh.json"
    with open(mesh_path, "w") as f:
        json.dump(mesh_data, f, indent=2)
    
    return mesh_path

def main():
    parser = argparse.ArgumentParser(
        description="Generate RTS unit assets from faction/tier/role specifications"
    )
    parser.add_argument("faction", help="Faction identifier (e.g., faction_a, faction_b)")
    parser.add_argument("tier", type=int, choices=[1, 2, 3, 4], help="Tier level (1-4)")
    parser.add_argument("role", help="Unit role (interceptor, fighter, bomber, heavy, support, ground)")
    parser.add_argument("--output", "-o", type=Path, default=BASE_PATH,
                       help="Output directory for generated assets")
    parser.add_argument("--name", help="Override unit name")
    parser.add_argument("--cost", type=int, help="Override unit cost")
    
    args = parser.parse_args()
    
    unit = generate_unit(
        args.faction,
        args.tier,
        args.role,
        name=args.name,
        cost=args.cost
    )
    
    mesh_path = generate_mesh(unit, args.output)
    unit["placeholder_mesh"] = str(mesh_path.relative_to(args.output))
    unit_path = save_unit(unit, args.output)
    print(f"Generated unit: {unit_path}")
    print(f"Generated mesh: {mesh_path}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
