shader_type spatial;
render_mode unshaded, depth_test_disabled, depth_draw_never, cull_disabled, fog_disabled, shadows_disabled;

// Full-screen pass. It rebuilds the world position of each pixel from the depth
// buffer, so terrain, trees and buildings all dim together.
uniform sampler2D depth_texture : hint_depth_texture, repeat_disable, filter_nearest;
uniform sampler2D vision_texture : repeat_disable, filter_linear;
uniform vec2 world_size = vec2(40000.0, 40000.0);
uniform vec3 fog_color : source_color = vec3(0.03, 0.05, 0.08);
uniform float fog_strength : hint_range(0.0, 1.0) = 0.62;

void vertex() {
	POSITION = vec4(VERTEX.xy * 2.0, 1.0, 1.0);
}

void fragment() {
	float depth = texture(depth_texture, SCREEN_UV).r;
	vec3 ndc = vec3(SCREEN_UV, depth) * 2.0 - 1.0;
	vec4 view_position = INV_PROJECTION_MATRIX * vec4(ndc, 1.0);
	view_position.xyz /= view_position.w;
	vec3 world_position = (INV_VIEW_MATRIX * vec4(view_position.xyz, 1.0)).xyz;
	float seen = texture(vision_texture, world_position.xz / world_size + 0.5).r;
	ALBEDO = fog_color;
	ALPHA = (1.0 - smoothstep(0.15, 0.85, seen)) * fog_strength;
}
