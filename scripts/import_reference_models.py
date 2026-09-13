"""Convert selected user-provided reference models into Godot-ready GLBs.

The output is presentation-only. Stable game visual IDs and simulation data are
not read or changed by this script. Blender imports each source, applies the
scene transforms, and exports an embedded-texture GLB for Godot.
"""

import argparse
import json
import os
import sys

import bpy
from mathutils import Matrix, Vector


MODELS = {
    "elite_mbt": "ares_apc.glb",
    "elite_artillery": "J41_O_I_120_skins.glb",
    "elite_anti_air": "osa-akm_sam_system.glb",
    "mass_swarm_tank": "raketa_t-62ab_wotheat.glb",
    "mass_assault_vehicle": "strike_fighters_tanks_pack.glb",
    "mass_anti_air": "buk-m3_9k317_sam.glb",
    "industrial_mbt": "strike_fighters_apcifv_pack.glb",
    "industrial_missile_platform": "strike_fighters_radar_pack.glb",
    "industrial_engineering": "boxer_afv_engineering_hmg.glb",
    "elite_fighter": "Untitled.glb",
    "industrial_fighter_f15c": "f-15c_eagle_usa.fbx",
    "elite_vtol": "z-10me_war_thunder_leviathans.glb",
    "mass_recon": "strike_fighters_apcifv_pack.glb",
    "industrial_logistics_truck": "ares_apc.glb",
    "industrial_destroyer": "USS76-2-512-ratio50-weld-dedub-prune-flatten-join-quantize-jpeg.glb",
    "industrial_carrier": "USS76-2-512-ratio50-weld-dedub-prune-flatten-join-quantize-jpeg.glb",
}

CATEGORIES = {
    "elite_mbt": "ground", "elite_artillery": "ground", "elite_anti_air": "ground",
    "mass_swarm_tank": "ground", "mass_assault_vehicle": "ground", "mass_anti_air": "ground",
    "industrial_mbt": "ground", "industrial_missile_platform": "ground", "industrial_engineering": "ground",
    "elite_fighter": "air", "industrial_fighter_f15c": "air", "elite_vtol": "air", "mass_recon": "ground",
    "industrial_logistics_truck": "logistics", "industrial_destroyer": "naval", "industrial_carrier": "naval",
}
PROTOTYPE_TRIANGLE_BUDGET = 20000
REFERENCE_ENVELOPE_METERS = 8.0


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def import_source(path):
    extension = os.path.splitext(path)[1].lower()
    if extension == ".fbx":
        # Blender 5 moved the FBX importer to wm.fbx_import. Keep the older
        # operator for the project's older Blender installations.
        if hasattr(bpy.ops.wm, "fbx_import"):
            bpy.ops.wm.fbx_import(filepath=path)
        else:
            bpy.ops.import_scene.fbx(
                filepath=path,
                directory=os.path.dirname(path),
                files=[{"name": os.path.basename(path)}],
            )
    elif extension in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    else:
        raise ValueError(f"Unsupported source format: {path}")


def export_model(source_path, output_path):
    clear_scene()
    import_source(source_path)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=output_path,
        export_format="GLB",
        export_apply=True,
        export_image_format="AUTO",
        export_materials="EXPORT",
        export_texcoords=True,
        export_normals=True,
    )


def write_metadata(asset_id, output_path):
    mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    if not mesh_objects:
        raise RuntimeError(f"No mesh objects imported for {asset_id}")
    corners = []
    triangle_count = 0
    for obj in mesh_objects:
        corners.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
        triangle_count += sum(len(poly.vertices) - 2 for poly in obj.data.polygons if len(poly.vertices) >= 3)
    minimum = [min(point[index] for point in corners) for index in range(3)]
    maximum = [max(point[index] for point in corners) for index in range(3)]
    metadata = {
        "category": CATEGORIES[asset_id],
        "godot_forward": "-Z",
        "godot_up": "+Y",
        "bounds_m": {axis: maximum[index] - minimum[index] for index, axis in enumerate(("x", "y", "z"))},
        "triangles_all_exported_meshes": triangle_count,
        "collision_recommendation": "Use simple BoxShape3D/CapsuleShape3D or custom RTS footprint data; do not use render mesh collision per unit.",
        "source_pipeline": "User-provided reference_models conversion",
    }
    with open(os.path.splitext(output_path)[0] + ".meta.json", "w", encoding="utf-8") as handle:
        json.dump(metadata, handle, indent=2)
        handle.write("\n")


def decimate_for_prototype():
    mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    triangle_count = sum(sum(len(poly.vertices) - 2 for poly in obj.data.polygons if len(poly.vertices) >= 3) for obj in mesh_objects)
    if triangle_count <= PROTOTYPE_TRIANGLE_BUDGET:
        return
    bpy.ops.object.select_all(action="DESELECT")
    for obj in mesh_objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_objects[0]
    bpy.ops.object.join()
    combined = mesh_objects[0]
    modifier = combined.modifiers.new(name="PrototypeTriangleBudget", type="DECIMATE")
    modifier.ratio = max(0.001, PROTOTYPE_TRIANGLE_BUDGET / float(triangle_count))
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def normalize_scene_envelope():
    mesh_objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    corners = [obj.matrix_world @ Vector(corner) for obj in mesh_objects for corner in obj.bound_box]
    if not corners:
        raise RuntimeError("Imported scene has no geometry")
    dimensions = [max(point[index] for point in corners) - min(point[index] for point in corners) for index in range(3)]
    maximum = max(dimensions)
    if maximum <= 0.0:
        raise RuntimeError("Imported scene has invalid bounds")
    factor = REFERENCE_ENVELOPE_METERS / maximum
    scale_matrix = Matrix.Diagonal((factor, factor, factor, 1.0))
    # Apply the factor to each top-level object's world matrix. Parenting to a
    # scaled empty retains each child's world transform and made the old
    # metadata (and some exports) retain source-sized models.
    for obj in mesh_objects:
        if obj.parent is None:
            obj.matrix_world = scale_matrix @ obj.matrix_world
    bpy.context.view_layer.update()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source_root")
    parser.add_argument("output_root")
    parser.add_argument("--only", choices=sorted(MODELS.keys()), help="Convert one asset instead of the complete reference pack")
    blender_args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    args = parser.parse_args(blender_args)
    source_root = os.path.abspath(args.source_root)
    output_root = os.path.abspath(args.output_root)
    selected_models = {args.only: MODELS[args.only]} if args.only else MODELS
    for asset_id, filename in selected_models.items():
        matches = []
        for root, _, files in os.walk(source_root):
            if filename in files:
                matches.append(os.path.join(root, filename))
        if not matches:
            raise FileNotFoundError(f"No source found for {asset_id}: {filename}")
        source_path = matches[0]
        output_path = os.path.join(output_root, f"{asset_id}.glb")
        print(f"IMPORT_REFERENCE {asset_id} <- {source_path}")
        clear_scene()
        import_source(source_path)
        decimate_for_prototype()
        normalize_scene_envelope()
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        write_metadata(asset_id, output_path)
        # Re-export the already-imported scene after metadata inspection.
        bpy.ops.export_scene.gltf(
            filepath=output_path,
            export_format="GLB",
            export_apply=True,
            export_image_format="AUTO",
            export_materials="EXPORT",
            export_texcoords=True,
            export_normals=True,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
