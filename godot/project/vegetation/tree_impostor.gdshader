shader_type spatial;
// Hemi-octahedral tree impostor. Each tree is one camera-facing quad that
// samples an atlas of the real model rendered from frames x frames directions
// over the upper hemisphere (tools/bake_tree_impostors.gd). The three frames
// nearest the view direction are reprojected onto their own planes and
// blended, so the tree turns and tilts with the camera instead of popping.
render_mode world_vertex_coords, cull_disabled, specular_disabled;

// Near the camera the tree's real model takes over (terrain_vegetation.gd).
#include "res://vegetation/tree_fade.gdshaderinc"

uniform sampler2D albedo_atlas : source_color, filter_linear_mipmap, repeat_disable;
uniform sampler2D normal_atlas : hint_default_white, filter_linear_mipmap, repeat_disable;
uniform float frames = 8.0;
// Bounding sphere of the baked model, in model units.
uniform float radius = 10.0;
uniform float center_height = 10.0;
uniform float alpha_cutoff : hint_range(0.0, 1.0) = 0.5;
uniform float brightness = 1.7;

varying vec2 frame_uv_a;
varying vec2 frame_uv_b;
varying vec2 frame_uv_c;
varying vec2 frame_cell_a;
varying vec2 frame_cell_b;
varying vec2 frame_cell_c;
varying vec3 frame_weights;
varying vec3 tree_right;
varying vec3 tree_forward;
varying vec3 tree_tint;
varying flat float tree_fade;

vec2 hemi_encode(vec3 direction) {
	direction.y = max(direction.y, 0.0);
	direction /= abs(direction.x) + abs(direction.y) + abs(direction.z);
	return vec2(direction.x + direction.z, direction.x - direction.z) * 0.5 + 0.5;
}

vec3 hemi_decode(vec2 uv) {
	vec2 p = uv * 2.0 - 1.0;
	float x = (p.x + p.y) * 0.5;
	float z = (p.x - p.y) * 0.5;
	return normalize(vec3(x, 1.0 - abs(x) - abs(z), z));
}

// The camera basis each frame was rendered with (Godot look_at, +Y up).
void frame_basis(vec3 direction, out vec3 right, out vec3 up) {
	vec3 hint = abs(direction.y) > 0.999 ? vec3(0.0, 0.0, -1.0) : vec3(0.0, 1.0, 0.0);
	right = normalize(cross(hint, direction));
	up = cross(direction, right);
}

vec2 frame_uv(vec2 cell, vec3 eye, vec3 ray, vec3 center, mat3 rotation, float size) {
	vec3 direction = hemi_decode(cell / (frames - 1.0));
	vec3 right;
	vec3 up;
	frame_basis(direction, right, up);
	direction = rotation * direction;
	right = rotation * right;
	up = rotation * up;
	float facing = dot(ray, direction);
	float distance_along = dot(center - eye, direction) / (abs(facing) < 0.0001 ? -0.0001 : facing);
	vec3 local = eye + ray * distance_along - center;
	return vec2(dot(local, right), -dot(local, up)) / size + 0.5;
}

void vertex() {
	// MODEL_MATRIX holds yaw and a scale, which may stretch the crown across
	// (x and z) differently from its height (y); VERTEX is in world space.
	// Trees cleared from a levelled site are scaled to nothing; the clamp keeps
	// them a harmless point instead of dividing by zero.
	float width_scale = max(length(MODEL_MATRIX[0].xyz), 0.00001);
	float height_scale = max(length(MODEL_MATRIX[1].xyz), 0.00001);
	vec3 eye = INV_VIEW_MATRIX[3].xyz;
	tree_fade = tree_near_fade(MODEL_MATRIX[3].xyz, eye);
	mat3 rotation = mat3(MODEL_MATRIX[0].xyz / width_scale, MODEL_MATRIX[1].xyz / height_scale, MODEL_MATRIX[2].xyz / width_scale);
	vec3 center = MODEL_MATRIX[3].xyz + MODEL_MATRIX[1].xyz * center_height;
	vec3 to_eye = normalize(eye - center);
	vec3 right;
	vec3 up;
	frame_basis(to_eye, right, up);
	vec2 corner = UV * 2.0 - 1.0;
	VERTEX = center + (right * corner.x - up * corner.y) * radius * max(width_scale, height_scale);
	NORMAL = to_eye;

	// Frame choice and reprojection happen in the tree's own unscaled space,
	// where the baked frames are exact; a stretched crown then stays correct.
	vec3 to_model_scale = vec3(1.0 / width_scale, 1.0 / height_scale, 1.0 / width_scale);
	vec3 model_eye = (transpose(rotation) * (eye - MODEL_MATRIX[3].xyz)) * to_model_scale;
	vec3 model_vertex = (transpose(rotation) * (VERTEX - MODEL_MATRIX[3].xyz)) * to_model_scale;
	vec3 model_center = vec3(0.0, center_height, 0.0);
	vec2 grid = hemi_encode(normalize(model_eye - model_center)) * (frames - 1.0);
	vec2 base = min(floor(grid), vec2(frames - 2.0));
	vec2 fraction = grid - base;
	vec3 ray = normalize(model_vertex - model_eye);
	float size = radius * 2.0;
	if (fraction.x + fraction.y < 1.0) {
		frame_cell_a = base;
		frame_weights = vec3(1.0 - fraction.x - fraction.y, fraction.x, fraction.y);
	} else {
		frame_cell_a = base + vec2(1.0);
		frame_weights = vec3(fraction.x + fraction.y - 1.0, 1.0 - fraction.y, 1.0 - fraction.x);
	}
	frame_cell_b = base + vec2(1.0, 0.0);
	frame_cell_c = base + vec2(0.0, 1.0);
	frame_uv_a = frame_uv(frame_cell_a, model_eye, ray, model_center, mat3(1.0), size);
	frame_uv_b = frame_uv(frame_cell_b, model_eye, ray, model_center, mat3(1.0), size);
	frame_uv_c = frame_uv(frame_cell_c, model_eye, ray, model_center, mat3(1.0), size);
	tree_right = rotation[0];
	tree_forward = rotation[2];
	tree_tint = COLOR.rgb;
}

vec4 sample_frame(sampler2D atlas, vec2 cell, vec2 uv) {
	if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
		return vec4(0.0);
	}
	return texture(atlas, (cell + uv) / frames);
}

void fragment() {
	// The other half of the dither the near model keeps.
	if (tree_fade < tree_dither(FRAGCOORD.xy)) {
		discard;
	}
	vec4 a = sample_frame(albedo_atlas, frame_cell_a, frame_uv_a);
	vec4 b = sample_frame(albedo_atlas, frame_cell_b, frame_uv_b);
	vec4 c = sample_frame(albedo_atlas, frame_cell_c, frame_uv_c);
	vec4 color = a * frame_weights.x + b * frame_weights.y + c * frame_weights.z;
	// Mipmapping averages leaves into the clear background; restore coverage
	// with distance so far forests do not thin out to sticks.
	// The mip level, from screen derivatives (textureQueryLod needs GL 4).
	vec2 texel_uv = (frame_cell_a + frame_uv_a) / frames * vec2(textureSize(albedo_atlas, 0));
	float lod = 0.5 * log2(max(max(dot(dFdx(texel_uv), dFdx(texel_uv)), dot(dFdy(texel_uv), dFdy(texel_uv))), 1.0));
	float coverage = color.a * (1.0 + max(lod - 1.0, 0.0) * 0.35);
	if (coverage < alpha_cutoff) {
		discard;
	}
	vec3 rgb = color.rgb / max(color.a, 0.001);
	vec3 n = sample_frame(normal_atlas, frame_cell_a, frame_uv_a).rgb * frame_weights.x
		+ sample_frame(normal_atlas, frame_cell_b, frame_uv_b).rgb * frame_weights.y
		+ sample_frame(normal_atlas, frame_cell_c, frame_uv_c).rgb * frame_weights.z;
	n = n / max(color.a, 0.001) * 2.0 - 1.0;
	vec3 tree_up = cross(tree_forward, tree_right);
	vec3 world_normal = normalize(tree_right * n.x + tree_up * n.y + tree_forward * n.z);
	NORMAL = normalize((VIEW_MATRIX * vec4(world_normal, 0.0)).xyz);
	ALBEDO = rgb * tree_tint * brightness;
	ROUGHNESS = 1.0;
	BACKLIGHT = rgb * 0.12;
}
