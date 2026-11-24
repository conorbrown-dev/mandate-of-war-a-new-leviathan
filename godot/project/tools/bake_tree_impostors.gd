extends SceneTree
## Bakes hemi-octahedral impostor atlases for the forest (vegetation/tree_impostor.gdshader).
##
## Each source tree is rendered orthographically from FRAMES x FRAMES directions
## over the upper hemisphere, once for unlit colour and once for normals, and the
## frames are packed into one albedo and one normal PNG. The trees are drawn
## as models only close to the camera; these pictures carry the rest of the
## forest. The results and their sizes go to impostors.json.
##
## Needs a GPU (do not pass --headless):
##   godot --path godot/project --script res://tools/bake_tree_impostors.gd
## Sources are the meshes made by tools/build_near_trees.gd (run that first).

const FRAMES := 8
# Trees are seen closest and get the sharpest frames; small plants need fewer.
const FRAME_PIXELS := 384
const SMALL_FRAME_PIXELS := 256
const OUTPUT_DIR := "res://assets/vegetation/impostors"

# kind: conifer, broadleaf, sapling or shrub. source: a mesh made by
# tools/build_near_trees.gd, so the distant impostor and the model drawn close
# to the camera are the same tree.
const VARIANTS := [
	{"key": "pine_a", "kind": "conifer"},
	{"key": "pine_b", "kind": "conifer"},
	{"key": "pine_c", "kind": "conifer"},
	{"key": "pine_d", "kind": "conifer"},
	{"key": "pine_e", "kind": "conifer"},
	{"key": "oak_a", "kind": "broadleaf"},
	{"key": "oak_b", "kind": "broadleaf"},
	{"key": "oak_c", "kind": "broadleaf"},
	{"key": "ash_a", "kind": "broadleaf"},
	{"key": "ash_b", "kind": "broadleaf"},
	{"key": "ash_c", "kind": "broadleaf"},
	{"key": "aspen_a", "kind": "broadleaf"},
	{"key": "aspen_b", "kind": "broadleaf"},
	{"key": "aspen_c", "kind": "broadleaf"},
	{"key": "young_pine_a", "kind": "sapling"},
	{"key": "young_pine_b", "kind": "sapling"},
	{"key": "bush_a", "kind": "shrub"},
	{"key": "bush_b", "kind": "shrub"},
	{"key": "bush_c", "kind": "shrub"},
]
const NEAR_MESHES := "res://assets/vegetation/near"

const BAKE_SHADER := """
shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_opaque;
uniform sampler2D albedo_texture : source_color, filter_linear_mipmap;
uniform sampler2D occlusion_texture : hint_default_white, filter_linear_mipmap;
uniform vec4 albedo_color : source_color = vec4(1.0);
uniform vec2 uv_scale = vec2(1.0);
uniform bool use_alpha = false;
uniform bool bake_normals = false;
uniform vec3 crown_center;
uniform float crown_radius = 1.0;
// Geometry below this height is the patch of ground some sources stand on.
uniform float clip_height = -1000.0;
varying vec3 baked_position;
varying vec3 baked_normal;

void vertex() {
	baked_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	baked_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

vec3 to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

void fragment() {
	if (baked_position.y < clip_height) {
		discard;
	}
	vec4 texel = texture(albedo_texture, UV * uv_scale) * albedo_color;
	if (use_alpha && texel.a < 0.5) {
		discard;
	}
	vec3 normal = normalize(baked_normal) * (FRONT_FACING ? 1.0 : -1.0);
	// Leaves face every way; bending them toward the crown's outside gives the
	// canopy a rounded, lit volume once it is reduced to a picture.
	vec3 outward = baked_position - crown_center;
	outward.y *= 0.6;
	normal = normalize(mix(normal, normalize(outward + vec3(0.0, 0.001, 0.0)), 0.55));
	// The darker inside of the crown, which a flat picture otherwise loses.
	float depth = clamp(length((baked_position - crown_center) * vec3(1.0, 0.6, 1.0)) / crown_radius, 0.0, 1.0);
	float shade = texture(occlusion_texture, UV * uv_scale).r * mix(0.55, 1.0, depth);
	if (bake_normals) {
		// The viewport writes sRGB; undo that so the PNG holds the raw vector.
		ALBEDO = to_linear(normal * 0.5 + 0.5);
	} else {
		ALBEDO = texel.rgb * shade;
	}
}
"""

var _sources := ""


func _initialize() -> void:
	_sources = OS.get_environment("RTS_TREE_SOURCES")
	_run.call_deferred()


func _run() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUTPUT_DIR))
	var shader := Shader.new()
	shader.code = BAKE_SHADER
	var catalog := []
	var scenes := {}
	for variant in VARIANTS:
		var source := NEAR_MESHES.path_join("%s.res" % variant.key)
		variant = variant.duplicate()
		variant["source"] = source
		variant["node"] = ""
		if not scenes.has(source):
			scenes[source] = _load_scene(source)
		var scene: Node3D = scenes[source]
		if scene == null:
			push_error("Could not load %s" % source)
			quit(1)
			return
		var tree := _tree_node(scene, String(variant.node))
		if tree == null:
			push_error("No tree %s in %s" % [variant.node, source])
			quit(1)
			return
		var entry: Dictionary = await _bake(variant, tree, shader)
		if entry.is_empty():
			quit(1)
			return
		catalog.append(entry)
		print("BAKED %s height=%.1f radius=%.1f" % [variant.key, entry.height, entry.radius])
	var file := FileAccess.open(OUTPUT_DIR.path_join("impostors.json"), FileAccess.WRITE)
	file.store_string(JSON.stringify({
		"version": 2,
		"notes": "Generated by tools/bake_tree_impostors.gd. Sizes are in model metres; the tree stands on its origin.",
		"frames": FRAMES,
		"frame_pixels": FRAME_PIXELS,
		"variants": catalog,
	}, "  "))
	file.close()
	quit(0)


func _load_scene(source: String) -> Node3D:
	if source.begins_with("res://"):
		var resource: Resource = load(source)
		if resource is Mesh:
			var holder := Node3D.new()
			var model := MeshInstance3D.new()
			model.mesh = resource
			holder.add_child(model)
			return holder
		return (resource as PackedScene).instantiate() if resource is PackedScene else null
	if _sources.is_empty():
		push_error("Set RTS_TREE_SOURCES to the folder holding the downloaded Poly Haven trees")
		return null
	var document := GLTFDocument.new()
	var state := GLTFState.new()
	var path := _sources.path_join(source.trim_prefix("polyhaven:"))
	if document.append_from_file(path, state) != OK:
		return null
	return document.generate_scene(state) as Node3D


func _tree_node(scene: Node3D, node_name: String) -> Node3D:
	if node_name.is_empty():
		return scene
	return scene.find_child(node_name, true, false) as Node3D


func _bake(variant: Dictionary, tree: Node3D, shader: Shader) -> Dictionary:
	# A copy of just this tree, standing on the origin.
	var model := tree.duplicate() as Node3D
	model.transform = Transform3D.IDENTITY
	var bounds := AABB()
	var first := true
	var materials: Array[ShaderMaterial] = []
	for mesh_instance in _mesh_instances(model):
		var local := _relative_transform(model, mesh_instance)
		var box := local * mesh_instance.mesh.get_aabb()
		bounds = box if first else bounds.merge(box)
		first = false
		for surface in range(mesh_instance.mesh.get_surface_count()):
			var material := ShaderMaterial.new()
			material.shader = shader
			var source := mesh_instance.get_active_material(surface) as BaseMaterial3D
			if source != null:
				material.set_shader_parameter("albedo_texture", source.albedo_texture)
				material.set_shader_parameter("albedo_color", source.albedo_color)
				material.set_shader_parameter("uv_scale", Vector2(source.uv1_scale.x, source.uv1_scale.y))
				var occlusion: Texture2D = source.ao_texture if source.ao_texture != null else source.roughness_texture
				if occlusion != null and String(variant.source).begins_with("polyhaven:"):
					material.set_shader_parameter("occlusion_texture", occlusion)
				material.set_shader_parameter("use_alpha", source.transparency != BaseMaterial3D.TRANSPARENCY_DISABLED
					and source.albedo_texture != null and source.albedo_texture.get_image() != null
					and source.albedo_texture.get_image().detect_alpha() != Image.ALPHA_NONE)
			mesh_instance.set_surface_override_material(surface, material)
			materials.append(material)
	if first:
		push_error("%s has no meshes" % variant.key)
		return {}
	# The impostor's sphere is centred on the trunk axis, so the tree keeps its footing.
	var center_height := bounds.get_center().y
	var radius := 0.0
	for corner in range(8):
		var point := bounds.get_endpoint(corner)
		radius = maxf(radius, Vector3(point.x, point.y - center_height, point.z).length())
	radius *= 1.02
	var crown := Vector3(0.0, bounds.position.y + bounds.size.y * 0.62, 0.0)
	# Some sources stand on a mound of ground; clip_below trims it off.
	var clip := bounds.position.y + bounds.size.y * float(variant.get("clip_below", 0.0))
	for material in materials:
		material.set_shader_parameter("clip_height", clip)
		material.set_shader_parameter("crown_center", crown)
		material.set_shader_parameter("crown_radius", maxf(bounds.size.x, bounds.size.z) * 0.5)

	var pixels := FRAME_PIXELS if String(variant.kind) in ["conifer", "broadleaf"] else SMALL_FRAME_PIXELS
	var viewport := SubViewport.new()
	viewport.size = Vector2i(pixels, pixels)
	viewport.transparent_bg = true
	viewport.own_world_3d = true
	viewport.msaa_3d = Viewport.MSAA_8X
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	viewport.add_child(model)
	var camera := Camera3D.new()
	camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	camera.size = radius * 2.0
	camera.near = 0.05
	camera.far = radius * 4.0 + 10.0
	viewport.add_child(camera)
	camera.current = true

	var atlases := {}
	for pass_name in ["albedo", "normal"]:
		for material in materials:
			material.set_shader_parameter("bake_normals", pass_name == "normal")
		var atlas := Image.create_empty(FRAMES * pixels, FRAMES * pixels, false, Image.FORMAT_RGBA8)
		for row in range(FRAMES):
			for column in range(FRAMES):
				var direction := _hemi_decode(Vector2(column, row) / float(FRAMES - 1))
				var center := Vector3(0.0, center_height, 0.0)
				var hint := Vector3(0.0, 0.0, -1.0) if absf(direction.y) > 0.999 else Vector3.UP
				camera.look_at_from_position(center + direction * (radius * 2.0 + 5.0), center, hint)
				await RenderingServer.frame_post_draw
				await RenderingServer.frame_post_draw
				var frame := viewport.get_texture().get_image()
				frame.convert(Image.FORMAT_RGBA8)
				atlas.blit_rect(frame, Rect2i(Vector2i.ZERO, frame.get_size()), Vector2i(column, row) * pixels)
		atlases[pass_name] = atlas
	viewport.queue_free()

	var albedo_path := OUTPUT_DIR.path_join("%s_albedo.png" % variant.key)
	var normal_path := OUTPUT_DIR.path_join("%s_normal.png" % variant.key)
	var normal_atlas: Image = atlases.normal
	var albedo_atlas: Image = atlases.albedo
	# Lighting detail survives at half resolution; colour carries the detail.
	normal_atlas.resize(normal_atlas.get_width() / 2, normal_atlas.get_height() / 2, Image.INTERPOLATE_BILINEAR)
	albedo_atlas.save_png(ProjectSettings.globalize_path(albedo_path))
	normal_atlas.save_png(ProjectSettings.globalize_path(normal_path))
	# Mean colour of the covered pixels, so the game can even out exposure
	# between sources photographed under different light.
	var sum := Vector3.ZERO
	var covered := 0.0
	var probe := albedo_atlas.duplicate() as Image
	probe.resize(256, 256, Image.INTERPOLATE_BILINEAR)
	for y in range(probe.get_height()):
		for x in range(probe.get_width()):
			var texel := probe.get_pixel(x, y)
			if texel.a > 0.5:
				sum += Vector3(texel.r, texel.g, texel.b) / texel.a
				covered += 1.0
	var mean := sum / maxf(covered, 1.0)
	return {
		"key": variant.key,
		"kind": variant.kind,
		"source": variant.source,
		"albedo": albedo_path,
		"normal": normal_path,
		"radius": snappedf(radius, 0.001),
		"center_height": snappedf(center_height, 0.001),
		"height": snappedf(bounds.end.y, 0.001),
		"crown_width": snappedf(maxf(bounds.size.x, bounds.size.z), 0.001),
		"frame_pixels": pixels,
		"mean_color": [snappedf(mean.x, 0.001), snappedf(mean.y, 0.001), snappedf(mean.z, 0.001)],
	}


func _mesh_instances(node: Node) -> Array[MeshInstance3D]:
	var found: Array[MeshInstance3D] = []
	if node is MeshInstance3D and (node as MeshInstance3D).mesh != null:
		found.append(node)
	for child in node.get_children():
		found.append_array(_mesh_instances(child))
	return found


func _relative_transform(ancestor: Node3D, node: Node3D) -> Transform3D:
	var transform := Transform3D.IDENTITY
	var current: Node = node
	while current != null and current != ancestor:
		transform = (current as Node3D).transform * transform
		current = current.get_parent()
	return transform


# Must match hemi_decode in vegetation/tree_impostor.gdshader.
func _hemi_decode(uv: Vector2) -> Vector3:
	var p := uv * 2.0 - Vector2.ONE
	var x := (p.x + p.y) * 0.5
	var z := (p.x - p.y) * 0.5
	return Vector3(x, 1.0 - absf(x) - absf(z), z).normalized()
