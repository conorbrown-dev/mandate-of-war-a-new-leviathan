shader_type spatial;
// Paved airfield surfaces: a runway with painted markings, taxiways with
// yellow guide lines, and a concrete apron. Each piece is a flat PlaneMesh
// whose local X runs across it and local Z along it, both in metres.
// Markings follow the ICAO layout, scaled to the compressed tactical strip.
render_mode specular_schlick_ggx;

uniform sampler2D asphalt_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D asphalt_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D concrete_albedo : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform sampler2D concrete_normal : hint_normal, filter_linear_mipmap_anisotropic, repeat_enable;
// 0 runway, 1 taxiway, 2 apron.
uniform int surface_kind = 0;
uniform float surface_length = 720.0;
uniform float surface_width = 45.0;
// Metres covered by one repeat of the photographed surface textures.
uniform float texture_metres = 2.0;
// Where another taxiway joins from the side, that edge line stops:
// side -1 or +1 (0 for none), over this range of distance from the middle.
uniform float edge_gap_side = 0.0;
uniform vec2 edge_gap_range = vec2(0.0);

const vec3 PAINT_WHITE = vec3(0.86, 0.87, 0.84);
const vec3 PAINT_YELLOW = vec3(0.86, 0.66, 0.12);

varying vec2 surface_position;

void vertex() {
	surface_position = VERTEX.xz;
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0)), f.x), f.y);
}

// 1 inside [low, high], antialiased so thin lines fade instead of shimmering.
float band(float x, float low, float high) {
	float aa = max(fwidth(x), 0.0001);
	return smoothstep(low - aa, low + aa, x) * (1.0 - smoothstep(high - aa, high + aa, x));
}

// A filled rectangle in (across, from-end) coordinates.
float box(vec2 p, vec2 low, vec2 high) {
	return band(p.x, low.x, high.x) * band(p.y, low.y, high.y);
}

float runway_paint(float across, float along) {
	float half_width = surface_width * 0.5;
	float side = abs(across);
	// Distance from the nearer end of the runway.
	float from_end = surface_length * 0.5 - abs(along);
	vec2 p = vec2(side, from_end);
	float paint = 0.0;
	// Side stripes, 0.9 m wide, just inside each edge.
	paint = max(paint, band(side, half_width - 1.6, half_width - 0.7));
	// Threshold "piano keys": 12 stripes 1.8 m wide, 30 m long, from 6 m in.
	float key = fract((side - 1.8) / 3.6);
	float keys = band(key, 0.0, 0.5) * band(side, 1.8, half_width - 3.0);
	paint = max(paint, keys * band(from_end, 6.0, 36.0));
	// Threshold bar across the full width.
	paint = max(paint, band(from_end, 3.0, 4.8) * band(side, 0.0, half_width - 0.7));
	// Centreline: 30 m dashes with 20 m gaps, clear of the designation numbers.
	float dash = band(fract((along + surface_length * 0.5) / 50.0), 0.0, 0.6);
	paint = max(paint, band(side, 0.0, 0.45) * dash * step(64.0, from_end));
	// Touchdown zone bars: a pair of three stripes each side, 22.5 m long.
	float tdz_stripes = band(fract((side - 3.0) / 3.0), 0.0, 0.6) * band(side, 3.0, 11.0);
	paint = max(paint, tdz_stripes * band(from_end, 90.0, 112.5));
	// Aiming point: two solid blocks, 45 m long and 9 m wide.
	paint = max(paint, box(p, vec2(5.0, 150.0), vec2(14.0, 195.0)));
	return paint;
}

float taxiway_paint(float across, float along) {
	float side = abs(across);
	float edge = band(side, surface_width * 0.5 - 1.0, surface_width * 0.5 - 0.6);
	edge = max(edge, band(side, surface_width * 0.5 - 1.8, surface_width * 0.5 - 1.4));
	if (edge_gap_side != 0.0 && sign(across) == edge_gap_side) {
		edge *= 1.0 - band(abs(along), edge_gap_range.x, edge_gap_range.y);
	}
	return max(edge, band(side, 0.0, 0.3));
}

void fragment() {
	float across = surface_position.x;
	float along = surface_position.y;
	vec2 uv = surface_position / texture_metres;
	// A second, rotated sample at a larger scale hides the texture repeat.
	vec2 uv_large = mat2(vec2(0.8, -0.6), vec2(0.6, 0.8)) * surface_position / (texture_metres * 7.3);
	float patches = value_noise(surface_position / 23.0) * 0.6 + value_noise(surface_position / 7.0) * 0.4;

	vec3 base;
	vec3 normal_sample;
	if (surface_kind == 2) {
		base = mix(texture(concrete_albedo, uv).rgb, texture(concrete_albedo, uv_large).rgb, 0.35);
		normal_sample = texture(concrete_normal, uv).rgb;
		// Poured slabs, 7.5 m square, with dark sealed joints.
		vec2 slab = abs(fract(surface_position / 7.5 + 0.5) - 0.5) * 7.5;
		float joint = max(band(slab.x, 0.0, 0.06), band(slab.y, 0.0, 0.06));
		base *= mix(1.0, 0.55, joint);
		base *= 0.92 + 0.16 * hash(floor(surface_position / 7.5));
	} else {
		base = mix(texture(asphalt_albedo, uv).rgb, texture(asphalt_albedo, uv_large).rgb, 0.4);
		normal_sample = texture(asphalt_normal, uv).rgb;
		base *= 0.78;
		// Asphalt is laid in 7.5 m lanes; each lane weathers to its own shade
		// and the joints between them are sealed with darker tar.
		float lane = floor(across / 7.5 + 0.5);
		base *= 0.9 + 0.2 * hash(vec2(lane, surface_length));
		float lane_joint = band(abs(fract(across / 7.5 + 0.5) - 0.5) * 7.5, 0.0, 0.12);
		base *= 1.0 - 0.45 * lane_joint;
		// Repair patches: squarish rectangles of fresher, darker surface.
		vec2 patch_cell = floor(surface_position / vec2(9.0, 23.0));
		vec2 patch_local = fract(surface_position / vec2(9.0, 23.0));
		float has_patch = step(0.88, hash(patch_cell + 17.0));
		float in_patch = band(patch_local.x, 0.15, 0.8) * band(patch_local.y, 0.2, 0.65);
		base *= 1.0 - 0.3 * has_patch * in_patch;
		// Hairline cracks sealed with tar.
		float crack = value_noise(surface_position * vec2(0.35, 0.08));
		base *= 1.0 - 0.35 * band(crack, 0.495, 0.505);
	}
	// Weathering: broad patches of older, lighter and fresher, darker surface.
	base *= 0.78 + 0.4 * patches;

	if (surface_kind == 0) {
		float from_end = surface_length * 0.5 - abs(along);
		// Black rubber deposits where wheels touch down.
		float streaks = value_noise(vec2(across * 1.4, along * 0.02));
		float rubber = band(abs(across), 0.0, 9.0) * smoothstep(60.0, 120.0, from_end) * (1.0 - smoothstep(220.0, 320.0, from_end));
		base *= 1.0 - rubber * (0.35 + 0.35 * streaks);
	}

	float paint = 0.0;
	vec3 paint_colour = PAINT_WHITE;
	if (surface_kind == 0) {
		paint = runway_paint(across, along);
	} else if (surface_kind == 1) {
		paint = taxiway_paint(across, along);
		paint_colour = PAINT_YELLOW;
	}
	// Worn paint lets the surface show through in places.
	paint *= 0.72 + 0.28 * value_noise(surface_position * 0.9);

	ALBEDO = mix(base, paint_colour, paint);
	ROUGHNESS = mix(0.9, 0.7, paint);
	NORMAL_MAP = mix(normal_sample, vec3(0.5, 0.5, 1.0), paint * 0.7);
	NORMAL_MAP_DEPTH = 0.6;
}
