"""Convert the user-supplied tree sources into Godot-ready GLB assets.

The active forest uses the tactical and strategic oak variants through
``visual_definitions.json``.  The classic and animated sources are also
converted and retained as optional Godot assets, rather than copied into the
game in their editor-only FBX/Blend formats.

Run with Blender, from the repository root:

    blender --background --python scripts/import_tree_models.py -- \
      downloaded_assets/trees godot/project/assets/source_3d/nature/trees
"""

import argparse
import json
import os
import sys
from math import radians

import bpy
from mathutils import Matrix, Vector


VARIANTS = (
    {
        "asset_id": "oak_tree_tactical",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Big_oak_tree__001_LOD1",
        "usage": "active tactical forest MultiMesh",
        "embedded_textures": True,
    },
    {
        "asset_id": "oak_tree_big_003",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Big_oak_tree__003_LOD1",
        "usage": "active tactical forest variant; shares tactical oak materials at runtime",
        "embedded_textures": False,
    },
    {
        "asset_id": "oak_tree_large_001",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Large_oak_tree_001_LOD1",
        "usage": "active tactical forest variant; shares tactical oak materials at runtime",
        "embedded_textures": False,
    },
    {
        "asset_id": "oak_tree_medium_002",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Medium_oak_tree__002_LOD1",
        "usage": "active tactical forest variant; shares tactical oak materials at runtime",
        "embedded_textures": False,
    },
    {
        "asset_id": "oak_tree_small_003",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Small_oak_tree__003_LOD1",
        "usage": "active tactical forest variant; shares tactical oak materials at runtime",
        "embedded_textures": False,
    },
    {
        "asset_id": "oak_tree_sapling_001",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Sapling_oak_tree_001_LOD1",
        "usage": "active tactical forest variant; shares tactical oak materials at runtime",
        "embedded_textures": False,
    },
    {
        "asset_id": "oak_tree_strategic",
        "source": "oak-trees-pack-17var-lods-seasons-gameready/source/Oak pack.fbx",
        "object_name": "Big_oak_tree__001_LOD2",
        "usage": "active strategic forest MultiMesh",
        "embedded_textures": False,
    },
    {
        "asset_id": "oak_tree_classic",
        "source": "oak-trees/source/oaktrees.fbx",
        "object_name": None,
        "usage": "optional classic-tree source conversion",
    },
    {
        "asset_id": "oak_tree_animated",
        "source": "tree-animate/source/Sketchfab_2022_09_21_06_15_37.blend",
        "object_name": None,
        "usage": "optional animated-tree source conversion",
    },
    {
        "asset_id": "oak_tree_english_forest_01_a",
        "source": "tree_english_oak_forest_01_usd/Tree_English_Oak_Forest_01_A.usd",
        "object_name": None,
        "usage": "optional English-oak USD conversion, forest form A",
    },
    {
        "asset_id": "oak_tree_english_forest_01_b",
        "source": "tree_english_oak_forest_01_usd/Tree_English_Oak_Forest_01_B.usd",
        "object_name": None,
        "usage": "optional English-oak USD conversion, forest form B",
    },
    {
        "asset_id": "oak_tree_english_forest_01_c",
        "source": "tree_english_oak_forest_01_usd/Tree_English_Oak_Forest_01_C.usd",
        "object_name": None,
        "usage": "optional English-oak USD conversion, forest form C",
    },
    {
        "asset_id": "oak_tree_english_forest_01_d",
        "source": "tree_english_oak_forest_01_usd/Tree_English_Oak_Forest_01_D.usd",
        "object_name": None,
        "usage": "optional English-oak USD conversion, forest form D",
    },
    {
        "asset_id": "oak_tree_english_foliage_01",
        "source": "tree_english_oak_forest_01_usd/Tree_English_Oak_01_Foliage.usd",
        "object_name": None,
        "usage": "optional English-oak USD foliage conversion",
    },
    {
        "asset_id": "oak_tree_user_1",
        "source": "oak_tree1.glb",
        "object_name": None,
        "usage": "active user-supplied GLB forest form 1",
        "target_triangles": 3600,
        "target_height_m": 18.0,
    },
    {
        "asset_id": "oak_tree_user_2",
        "source": "oak_tree2.glb",
        "object_name": None,
        "usage": "active user-supplied GLB forest form 2",
        "target_triangles": 4300,
        "target_height_m": 13.0,
    },
    {
        "asset_id": "oak_tree_user_3",
        "source": "oak_tree3.glb",
        "object_name": None,
        "usage": "active user-supplied GLB forest form 3",
        "target_triangles": 1200,
        "target_height_m": 22.0,
    },
)


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def import_source(source_path):
    extension = os.path.splitext(source_path)[1].lower()
    if extension == ".fbx":
        # Blender 5 moved the maintained FBX importer to wm.fbx_import.
        bpy.ops.wm.fbx_import(filepath=source_path)
    elif extension == ".blend":
        bpy.ops.wm.open_mainfile(filepath=source_path)
    elif extension in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=source_path)
    elif extension in (".usd", ".usda", ".usdc", ".usdz"):
        if "usd_import" not in dir(bpy.ops.wm):
            raise RuntimeError(
                "USD import is unavailable in this Blender build. Install or use a Blender build "
                "compiled with the USD extension before converting USD assets."
            )
        bpy.ops.wm.usd_import(filepath=source_path)
    else:
        raise ValueError(f"Unsupported tree source format: {source_path}")


def mesh_objects():
    return [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]


def flatten_mesh_hierarchy(objects):
    """Bake parent transforms into meshes before unit normalization/export."""
    for obj in objects:
        world_matrix = obj.matrix_world.copy()
        obj.parent = None
        obj.matrix_world = world_matrix
    bpy.context.view_layer.update()


def rotate_for_godot_up_axis(objects, degrees):
    if degrees is None:
        return
    rotation = Matrix.Rotation(radians(float(degrees)), 4, "X")
    for obj in objects:
        obj.data.transform(rotation)
    bpy.context.view_layer.update()


def retain_variant(object_name):
    if object_name is None:
        return
    selected = bpy.data.objects.get(object_name)
    if selected is None or selected.type != "MESH":
        raise RuntimeError(f"Expected mesh object not found: {object_name}")
    for obj in list(bpy.context.scene.objects):
        if obj != selected:
            bpy.data.objects.remove(obj, do_unlink=True)


def repair_texture_paths(source_root):
    """Resolve FBX's original-machine texture paths by basename locally."""
    candidates = {}
    for root, _, files in os.walk(source_root):
        for filename in files:
            candidates.setdefault(filename.lower(), os.path.join(root, filename))
    for image in bpy.data.images:
        if image.packed_file is not None:
            continue
        filename = os.path.basename(bpy.path.abspath(image.filepath)).lower()
        resolved = candidates.get(filename)
        if resolved:
            image.filepath = resolved
            image.reload()


def _load_texture(path, non_color=False):
    image = bpy.data.images.load(path, check_existing=True)
    if non_color:
        image.colorspace_settings.name = "Non-Color"
    return image


def _principled_texture_material(name, color_path, normal_path, roughness_path, alpha_cutout=False):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    material.node_tree.links.new(shader.outputs["BSDF"], output.inputs["Surface"])
    color = nodes.new("ShaderNodeTexImage")
    color.image = _load_texture(color_path)
    material.node_tree.links.new(color.outputs["Color"], shader.inputs["Base Color"])
    if alpha_cutout:
        material.node_tree.links.new(color.outputs["Alpha"], shader.inputs["Alpha"])
        material.surface_render_method = "DITHERED"
    if normal_path:
        normal = nodes.new("ShaderNodeTexImage")
        normal.image = _load_texture(normal_path, non_color=True)
        normal_map = nodes.new("ShaderNodeNormalMap")
        material.node_tree.links.new(normal.outputs["Color"], normal_map.inputs["Color"])
        material.node_tree.links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])
    if roughness_path:
        roughness = nodes.new("ShaderNodeTexImage")
        roughness.image = _load_texture(roughness_path, non_color=True)
        material.node_tree.links.new(roughness.outputs["Color"], shader.inputs["Roughness"])
    return material


def configure_oak_pack_materials(source_root, strategic=False):
    """The donor FBX has Windows texture paths and an empty leaf material."""
    if strategic:
        bark = bpy.data.materials.new("OakBark_Strategic")
        bark.diffuse_color = (0.18, 0.10, 0.045, 1.0)
        leaves = bpy.data.materials.new("OakLeaves_Strategic")
        leaves.diffuse_color = (0.075, 0.24, 0.06, 1.0)
        for obj in mesh_objects():
            if len(obj.data.materials) >= 1:
                obj.data.materials[0] = bark
            if len(obj.data.materials) >= 2:
                obj.data.materials[1] = leaves
        return
    textures = os.path.join(source_root, "oak-trees-pack-17var-lods-seasons-gameready", "textures")
    bark = _principled_texture_material(
        "OakBark_Imported",
        os.path.join(textures, "Oak_bark_tex_Basecolor.png"),
        None,
        None,
    )
    leaves = _principled_texture_material(
        "OakLeavesSummer_Imported",
        os.path.join(textures, "Oak_cluster_summer_RGBA.png"),
        None,
        None,
        alpha_cutout=True,
    )
    for obj in mesh_objects():
        if len(obj.data.materials) >= 1:
            obj.data.materials[0] = bark
        if len(obj.data.materials) >= 2:
            obj.data.materials[1] = leaves


def configure_classic_oak_materials(source_root):
    textures = os.path.join(source_root, "oak-trees", "textures")
    bark = _principled_texture_material(
        "ClassicOakBark_Imported",
        os.path.join(textures, "bark1.png"),
        None,
        None,
    )
    leaves = _principled_texture_material(
        "ClassicOakLeaves_Imported",
        os.path.join(textures, "oakbranchcolor.png"),
        None,
        None,
        alpha_cutout=True,
    )
    for obj in mesh_objects():
        for index, material in enumerate(obj.data.materials):
            if material is not None and material.name.lower().startswith("bark"):
                obj.data.materials[index] = bark
            elif material is not None and material.name.lower().startswith("leaf"):
                obj.data.materials[index] = leaves


def _world_bounds(objects):
    corners = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    if not corners:
        raise RuntimeError("Imported tree source has no geometry")
    minimum = [min(point[index] for point in corners) for index in range(3)]
    maximum = [max(point[index] for point in corners) for index in range(3)]
    return minimum, maximum


def reduce_triangle_budget(objects, target_triangles):
    if target_triangles is None:
        return
    current_triangles = sum(
        len(polygon.vertices) - 2
        for obj in objects
        for polygon in obj.data.polygons
        if len(polygon.vertices) >= 3
    )
    if current_triangles <= target_triangles:
        return
    # Several source GLBs split every leaf card into a separate mesh.  A
    # per-object decimator cannot simplify those one-triangle meshes, so join
    # them first and apply the tactical budget to the complete tree.
    if len(objects) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        bpy.ops.object.join()
        objects[:] = [bpy.context.view_layer.objects.active]
    ratio = max(0.001, float(target_triangles) / float(current_triangles))
    for obj in objects:
        modifier = obj.modifiers.new("RTS_TacticalDecimate", "DECIMATE")
        modifier.decimate_type = "COLLAPSE"
        modifier.ratio = ratio
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modifier.name)


def bake_to_ground_origin(objects, target_height_m=None):
    minimum, maximum = _world_bounds(objects)
    current_height = maximum[1] - minimum[1]
    if target_height_m is not None:
        if current_height <= 0.0:
            raise RuntimeError("Imported tree source has no measurable height")
        factor = float(target_height_m) / current_height
        for obj in objects:
            obj.scale *= factor
        bpy.context.view_layer.update()
        minimum, maximum = _world_bounds(objects)
    center = Vector(((minimum[0] + maximum[0]) * 0.5, minimum[1], (minimum[2] + maximum[2]) * 0.5))
    for obj in objects:
        obj.location -= center
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def metadata(asset_id, source_relative, usage, source_object):
    objects = mesh_objects()
    corners = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    minimum = [min(point[index] for point in corners) for index in range(3)]
    maximum = [max(point[index] for point in corners) for index in range(3)]
    triangles = sum(
        len(polygon.vertices) - 2
        for obj in objects
        for polygon in obj.data.polygons
        if len(polygon.vertices) >= 3
    )
    return {
        "category": "nature",
        "godot_forward": "-Z",
        "godot_up": "+Y",
        "bounds_m": {axis: maximum[index] - minimum[index] for index, axis in enumerate(("x", "y", "z"))},
        "triangles_all_exported_meshes": triangles,
        "usage": usage,
        "source_asset": "downloaded_assets/trees/" + source_relative,
        "source_object": source_object,
        "source_pipeline": "User-provided tree conversion; Blender 5; embedded-texture GLB",
    }


def convert_variant(source_root, output_root, variant):
    source_relative = variant["source"]
    source_path = os.path.join(source_root, source_relative)
    if not os.path.isfile(source_path):
        raise FileNotFoundError(source_path)
    clear_scene()
    import_source(source_path)
    retain_variant(variant["object_name"])
    objects = mesh_objects()
    if not objects:
        raise RuntimeError(f"No mesh objects imported for {variant['asset_id']}")
    flatten_mesh_hierarchy(objects)
    rotate_for_godot_up_axis(objects, variant.get("godot_x_rotation_degrees"))
    repair_texture_paths(source_root)
    if source_relative.startswith("oak-trees-pack-"):
        configure_oak_pack_materials(source_root, not bool(variant.get("embedded_textures", False)))
    elif source_relative.startswith("oak-trees/"):
        configure_classic_oak_materials(source_root)
    reduce_triangle_budget(objects, variant.get("target_triangles"))
    bake_to_ground_origin(objects, variant.get("target_height_m"))
    result_metadata = metadata(variant["asset_id"], source_relative, variant["usage"], variant["object_name"])
    output_path = os.path.join(output_root, variant["asset_id"] + ".glb")
    os.makedirs(output_root, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=output_path,
        export_format="GLB",
        export_apply=True,
        export_image_format="AUTO",
        export_materials="EXPORT",
        export_texcoords=True,
        export_normals=True,
    )
    with open(os.path.splitext(output_path)[0] + ".meta.json", "w", encoding="utf-8") as handle:
        json.dump(result_metadata, handle, indent=2)
        handle.write("\n")
    print(f"IMPORT_TREE {variant['asset_id']} triangles={result_metadata['triangles_all_exported_meshes']} -> {output_path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source_root")
    parser.add_argument("output_root")
    blender_args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    args = parser.parse_args(blender_args)
    source_root = os.path.abspath(args.source_root)
    output_root = os.path.abspath(args.output_root)
    for variant in VARIANTS:
        # Keep the established FBX/Blend converter usable on distribution Blender
        # builds that omit USD support, while making the missing capability explicit.
        if os.path.splitext(variant["source"])[1].lower() in (".usd", ".usda", ".usdc", ".usdz") and "usd_import" not in dir(bpy.ops.wm):
            print(f"SKIP_TREE {variant['asset_id']} USD import is unavailable in this Blender build")
            continue
        convert_variant(source_root, output_root, variant)
    return 0


if __name__ == "__main__":
    sys.exit(main())
