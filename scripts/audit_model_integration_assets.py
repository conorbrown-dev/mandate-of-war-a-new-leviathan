#!/usr/bin/env python3
"""Create the non-gameplay CC0 model inventory required by the integration pack.

The asset library may live outside a Git worktree while artists prepare it.  The
output stores repository-relative paths so it remains valid when the library is
later placed under downloaded_assets/ in the main checkout.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def category_for(path: Path) -> str:
    parts = {part.lower() for part in path.parts}
    if "naval" in parts:
        return "naval"
    if "air" in parts or "fighter_jets" in parts:
        return "air"
    if "logistics" in parts or "containers" in parts or "freight" in path.stem.lower():
        return "logistics"
    if "ground" in parts or "tanks" in parts or "tank" in path.stem.lower():
        return "ground"
    return "unknown"


def role_for(path: Path, category: str) -> str:
    name = path.stem.lower()
    for token, role in (("carrier", "carrier"), ("destroyer", "destroyer"),
                        ("submarine", "submarine"), ("bomber", "bomber"),
                        ("helicopter", "rotary_air"), ("fighter", "fighter"),
                        ("howitzer", "artillery"), ("engineer", "engineering"),
                        ("freight", "transport"), ("tank", "main_battle_tank"),
                        ("armor", "armored_vehicle")):
        if token in name:
            return role
    return f"{category}_prototype"


def source_for(path: Path) -> str | None:
    text = path.as_posix().lower()
    if "fighter_jets" in text or "air/fighters" in text:
        return "oga_fighter_jets"
    if "modern_tanks" in text:
        return "oga_modern_tanks"
    if "/t34_" in text:
        return "oga_t34"
    if "freeciv" in text:
        return "oga_freeciv_units"
    if "low_poly_tanks" in text:
        return "oga_low_poly_tanks"
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("asset_root", type=Path, help="CC0 Asset Import Pack root")
    parser.add_argument("output", type=Path, help="inventory JSON output")
    parser.add_argument("--repository-prefix", default="downloaded_assets/Mandate_of_War_CC0_Asset_Import_Pack")
    args = parser.parse_args()

    ready = args.asset_root / "godot_ready"
    manifest_path = args.asset_root / "manifests" / "cc0_sources.json"
    sources: dict[str, dict] = {}
    if manifest_path.is_file():
        for source in json.loads(manifest_path.read_text()).get("assets", []):
            sources[source["id"]] = source

    models = []
    for glb in sorted(ready.rglob("*.glb")):
        relative = glb.relative_to(args.asset_root)
        meta_path = glb.with_suffix(".meta.json")
        meta = json.loads(meta_path.read_text()) if meta_path.is_file() else {}
        category = meta.get("category", category_for(relative))
        source_id = source_for(relative)
        triangles = int(meta.get("triangles_all_exported_meshes", 0))
        models.append({
            "asset_id": "cc0." + ".".join(glb.relative_to(ready).with_suffix("").parts).lower().replace(" ", "_"),
            "glb_path": f"{args.repository_prefix}/{relative.as_posix()}",
            "meta_path": f"{args.repository_prefix}/{meta_path.relative_to(args.asset_root).as_posix()}" if meta_path.is_file() else None,
            "category": category,
            "prototype_role": role_for(glb, category),
            "bounds_m": meta.get("bounds_m"),
            "triangle_count": triangles or None,
            "provenance_source_id": source_id,
            "alternate_or_duplicate": "basic_replacements" in glb.name or "tank_collection" in glb.name.lower(),
            "status": "needs_cleanup" if triangles > 20000 else "usable",
            "validation": "metadata-present" if meta else "metadata-missing; Godot import pending",
        })

    output = {"schema_version": 1, "purpose": "prototype/donor visuals only", "models": models}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n")
    print(f"inventory_models={len(models)} output={args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
