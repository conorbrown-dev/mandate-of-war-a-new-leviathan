shader_type spatial;
// Trunk and branches of a near tree model (terrain_vegetation.gd's near ring).
// Darkened inside the crown and brightened like the tree's impostor
// (tools/bake_tree_impostors.gd), and dissolved into it with the shared fade.
render_mode specular_disabled;

#include "res://vegetation/tree_fade.gdshaderinc"

uniform sampler2D albedo_texture : source_color, hint_default_white, filter_linear_mipmap, repeat_enable;
uniform sampler2D normal_texture : hint_normal, filter_linear_mipmap, repeat_enable;
uniform bool use_normal_texture = false;
uniform vec4 albedo_color : source_color = vec4(1.0);
uniform vec2 uv_scale = vec2(1.0);
uniform float brightness = 1.0;
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
	vec2 uv = UV * uv_scale;
	float depth = clamp(length((model_position - crown_center) * vec3(1.0, 0.6, 1.0)) / crown_radius, 0.0, 1.0);
	// The impostor tints the whole tree, trunk included, so the model does too.
	ALBEDO = texture(albedo_texture, uv).rgb * albedo_color.rgb * mix(0.55, 1.0, depth) * tree_tint * brightness;
	if (use_normal_texture) {
		NORMAL_MAP = texture(normal_texture, uv).rgb;
	}
	ROUGHNESS = 1.0;
}
