shader_type spatial;
render_mode unshaded, cull_disabled, shadows_disabled, fog_disabled, depth_draw_never;

// Range outline ribbon. Each segment is a quad whose corners carry their own
// position, the far end of the segment (CUSTOM0) and a side of -1 or +1 (UV.x).
// The corners are pushed apart in screen space, so the line keeps one pixel
// width at any zoom, the same way the fog of war edge does.
uniform vec4 line_color : source_color = vec4(1.0);
uniform float width_px : hint_range(0.0, 12.0) = 3.0;

void vertex() {
	vec4 here = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	vec4 there = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(CUSTOM0.xyz, 1.0);
	here.w = max(here.w, 0.001);
	there.w = max(there.w, 0.001);
	vec2 along = (there.xy / there.w - here.xy / here.w) * VIEWPORT_SIZE;
	along = length(along) > 0.0001 ? normalize(along) : vec2(1.0, 0.0);
	vec2 across = vec2(-along.y, along.x);
	vec2 offset = across * UV.x * width_px / VIEWPORT_SIZE;
	// Nudge toward the camera (reverse-Z) so the line does not sink into slopes.
	POSITION = vec4(here.xy + offset * here.w, here.z + 0.0004 * here.w, here.w);
}

void fragment() {
	ALBEDO = line_color.rgb;
	ALPHA = line_color.a;
}
