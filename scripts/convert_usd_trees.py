"""Convert supplied USD tree geometry to Godot-readable GLB assets.

Run with the temporary usd-core runtime installed by this workspace's asset
workflow:

    PYTHONPATH=/tmp/mandate_usd_tools python3 scripts/convert_usd_trees.py \
      downloaded_assets/trees/tree_english_oak_forest_01_usd \
      godot/project/assets/source_3d/nature/trees

The supplied English-oak stages have no material bindings, so this preserves
their geometry and records that limitation in their metadata.  They are kept
as optional source conversions instead of being added to the active tactical
forest, whose triangle budget is intentionally much lower.
"""

import argparse
import json
from pathlib import Path

import numpy as np
import trimesh
from pxr import Usd, UsdGeom


SOURCES = (
    ("oak_tree_english_forest_01_a", "Tree_English_Oak_Forest_01_A.usd", False),
    ("oak_tree_english_forest_01_b", "Tree_English_Oak_Forest_01_B.usd", False),
    ("oak_tree_english_forest_01_c", "Tree_English_Oak_Forest_01_C.usd", False),
    ("oak_tree_english_forest_01_d", "Tree_English_Oak_Forest_01_D.usd", False),
    ("oak_tree_english_foliage_01", "Tree_English_Oak_01_Foliage.usd", True),
)


def triangulate(face_counts, face_indices):
    triangles = []
    offset = 0
    for count in face_counts:
        face = face_indices[offset:offset + count]
        offset += count
        for index in range(1, count - 1):
            triangles.append((face[0], face[index], face[index + 1]))
    return np.asarray(triangles, dtype=np.int64)


def meshes_for_stage(stage, include_all):
    meshes = [UsdGeom.Mesh(prim) for prim in stage.Traverse() if prim.IsA(UsdGeom.Mesh)]
    if not meshes:
        raise RuntimeError("USD stage has no meshes")
    if include_all:
        return meshes
    return [max(meshes, key=lambda mesh: len(mesh.GetPointsAttr().Get() or []))]


def convert(source_root, output_root, asset_id, filename, include_all):
    source_path = source_root / filename
    stage = Usd.Stage.Open(str(source_path))
    if stage is None:
        raise RuntimeError(f"Unable to open {source_path}")
    xforms = UsdGeom.XformCache(Usd.TimeCode.Default())
    vertices = []
    faces = []
    vertex_offset = 0
    for mesh in meshes_for_stage(stage, include_all):
        points = mesh.GetPointsAttr().Get() or []
        indices = mesh.GetFaceVertexIndicesAttr().Get() or []
        counts = mesh.GetFaceVertexCountsAttr().Get() or []
        if not points or not indices or not counts:
            continue
        matrix = xforms.GetLocalToWorldTransform(mesh.GetPrim())
        transformed = np.asarray(
            [[*matrix.TransformAffine(point)] for point in points], dtype=np.float64
        )
        vertices.append(transformed)
        faces.append(triangulate(counts, indices) + vertex_offset)
        vertex_offset += len(transformed)
    if not vertices or not faces:
        raise RuntimeError(f"No triangle geometry in {source_path}")
    vertices = np.vstack(vertices)
    faces = np.vstack(faces)
    minimum = vertices.min(axis=0)
    maximum = vertices.max(axis=0)
    vertices -= np.array([(minimum[0] + maximum[0]) * 0.5, minimum[1], (minimum[2] + maximum[2]) * 0.5])
    mesh = trimesh.Trimesh(vertices=vertices, faces=faces, process=False)
    output_root.mkdir(parents=True, exist_ok=True)
    output_path = output_root / f"{asset_id}.glb"
    mesh.export(str(output_path), file_type="glb")
    bounds = maximum - minimum
    metadata = {
        "category": "nature",
        "godot_forward": "-Z",
        "godot_up": "+Y",
        "bounds_m": {axis: float(bounds[index]) for index, axis in enumerate(("x", "y", "z"))},
        "triangles_all_exported_meshes": int(len(faces)),
        "usage": "optional English-oak USD geometry conversion; deferred from active forest due to source triangle count",
        "source_asset": f"downloaded_assets/trees/tree_english_oak_forest_01_usd/{filename}",
        "source_object": "largest render mesh" if not include_all else "all foliage stage meshes",
        "source_pipeline": "User-provided USD conversion; usd-core 26.8; geometry-only GLB",
        "material_status": "No USD material binding was present in the supplied stage; material authoring remains deferred.",
    }
    output_path.with_suffix(".meta.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"IMPORT_USD_TREE {asset_id} triangles={len(faces)} -> {output_path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source_root", type=Path)
    parser.add_argument("output_root", type=Path)
    args = parser.parse_args()
    for asset_id, filename, include_all in SOURCES:
        convert(args.source_root, args.output_root, asset_id, filename, include_all)


if __name__ == "__main__":
    main()
