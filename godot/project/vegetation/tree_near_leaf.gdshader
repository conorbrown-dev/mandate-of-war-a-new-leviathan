shader_type spatial;
// Leaves of a near tree model (terrain_vegetation.gd's near ring). Shaded as
// the impostor baker shades them (tools/bake_tree_impostors.gd): normals bent
// toward the outside of the crown and the inside of the crown darker, at the
// same brightness and tint as the tree's impostor, so the two match when one
// dissolves into the other.
render_mode cull_disabled, specular_disabled;

#include "res://vegetation/tree_fade.gdshaderinc"

uniform sampler2D albedo_texture : source_color, filter_linear_mipmap;
uniform vec4 albedo_color : source_color = vec4(1.0);
uniform float alpha_cutoff : hint_range(0.0, 1.0) = 0.5;
uniform float brightness = 1.0;
// Crown centre and half-width in model units, as the baker measured them.
uniform vec3 crown_center;
uniform float crown_radius = 1.0;

varying vec3 model_position;
varying flat float tree_fade;
varying vec3 tree_tint;

void vertex() {
	model_position = VERTEX;
	tree_fade = tree_near_fade(MODEL_MATRIX[3].xyz, INV_VIEW_MATRIX[3].xyz);
	tree_tint = COLOR.rgb;
}

void fragment() {
	if (tree_fade >= tree_dither(FRAGCOORD.xy)) {
		discard;
	}
	vec4 texel = texture(albedo_texture, UV) * albedo_color;
	if (texel.a < alpha_cutoff) {
		discard;
	}
	vec3 outward = model_position - crown_center;
	outward.y *= 0.6;
	vec3 outward_view = (VIEW_MATRIX * MODEL_MATRIX * vec4(outward + vec3(0.0, 0.001, 0.0), 0.0)).xyz;
	NORMAL = normalize(mix(NORMAL, normalize(outward_view), 0.55));
	float depth = clamp(length((model_position - crown_center) * vec3(1.0, 0.6, 1.0)) / crown_radius, 0.0, 1.0);
	vec3 rgb = texel.rgb * mix(0.55, 1.0, depth);
	ALBEDO = rgb * tree_tint * brightness;
	ROUGHNESS = 1.0;
	BACKLIGHT = rgb * 0.12;
}
