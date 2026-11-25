shader_type spatial;
// Road and rail-bed surfaces. The mesh from main.gd's _make_road_mesh carries
// UV.x as metres across the strip (centre at 0) and UV.y as metres along it.
// Roads get photographed asphalt, worn edges and painted lines; ballast gets
// the same texture tinted grey-brown with no paint.
render_mode specular_schlick_ggx;

uniform sampler2D asphalt_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D asphalt_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
// 0 road with markings, 1 ballast (no markings).
uniform int surface_kind = 0;
uniform float surface_width = 12.0;
// Metres covered by one repeat of the photographed texture.
uniform float texture_metres = 2.0;
uniform vec3 tint : source_color = vec3(1.0);

const vec3 PAINT_WHITE = vec3(0.86, 0.87, 0.84);
const vec3 PAINT_YELLOW = vec3(0.86, 0.66, 0.12);
const vec3 SHOULDER = vec3(0.30, 0.27, 0.21);

varying vec2 strip_position;

void vertex() {
	strip_position = UV;
}

// 1 inside [low, high], antialiased so thin lines fade instead of shimmering.
float band(float x, float low, float high) {
	float aa = max(fwidth(x), 0.0001);
	return smoothstep(low - aa, low + aa, x) * (1.0 - smoothstep(high - aa, high + aa, x));
}

void fragment() {
	float across = strip_position.x;
	float along = strip_position.y;
	float half_width = surface_width * 0.5;
	float side = abs(across);
	vec2 tex_uv = strip_position / texture_metres;
	vec3 base = texture(asphalt_albedo, tex_uv).rgb * tint;
	vec3 colour = base;
	if (surface_kind == 0) {
		// Worn, sandy shoulder along each edge.
		float shoulder = smoothstep(half_width - 1.0, half_width - 0.2, side);
		colour = mix(colour, SHOULDER, shoulder * 0.55);
		// Solid white edge lines just inside the shoulder.
		float edge = band(side, half_width - 1.1, half_width - 0.9);
		// Dashed yellow centreline: 3 m dashes every 9 m.
		float dash = band(fract(along / 9.0), 0.0, 1.0 / 3.0);
		float centre = band(side, 0.0, 0.12) * dash;
		colour = mix(colour, PAINT_WHITE, edge * 0.85);
		colour = mix(colour, PAINT_YELLOW, centre * 0.9);
	} else {
		colour *= vec3(1.0, 0.97, 0.9);
	}
	ALBEDO = colour;
	NORMAL_MAP = texture(asphalt_normal, tex_uv).rgb;
	ROUGHNESS = 0.92;
	SPECULAR = 0.25;
}
