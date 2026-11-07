shader_type spatial;
render_mode cull_disabled;

uniform sampler2D ocean_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D shore_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D grassland_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D forest_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D mud_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D rocky_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform float texture_tiling = 384.0;
uniform float water_wave_speed : hint_range(0.0, 1.0) = 0.18;
uniform float water_wave_height : hint_range(0.0, 4.0) = 1.15;
uniform float terrain_brightness : hint_range(0.1, 3.0) = 1.0;
uniform float terrain_daylight : hint_range(0.1, 2.5) = 1.0;
uniform float terrain_ambient_fill : hint_range(0.0, 1.0) = 0.2;
uniform vec4 fog_beacon_lights[8];
uniform sampler2D territory_texture : repeat_enable, filter_linear_mipmap;
uniform float territory_intensity : hint_range(0.0, 1.0) = 0.4;
// Baked by tools/generate_terrain.py: river, forest, light, large-scale variation.
uniform sampler2D terrain_masks : repeat_disable, filter_linear_mipmap;
uniform bool masks_enabled = false;
uniform float world_size = 40000.0;

varying float detail_fade;
varying float terrain_elevation;
varying vec3 terrain_world_position;
varying vec3 terrain_world_normal;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float value_noise(vec2 p) {
	vec2 cell = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(cell), hash(cell + vec2(1.0, 0.0)), f.x),
		mix(hash(cell + vec2(0.0, 1.0)), hash(cell + vec2(1.0, 1.0)), f.x), f.y);
}

float layered_noise(vec2 p) {
	return value_noise(p) * 0.55 + value_noise(p * 2.3 + 7.1) * 0.3 + value_noise(p * 5.1 + 3.7) * 0.15;
}

// Blends two rotated, differently scaled samples along a noise boundary, so a
// tile's repeat period never shows as a grid.
vec3 untiled(sampler2D source, vec2 world, float tile_size) {
	vec2 uv = world / tile_size;
	vec2 turned = vec2(uv.x * 0.8 - uv.y * 0.6, uv.x * 0.6 + uv.y * 0.8) * 0.73 + vec2(0.37, 0.61);
	float blend = smoothstep(0.42, 0.58, layered_noise(world / (tile_size * 2.7)));
	return mix(texture(source, uv).rgb, texture(source, turned).rgb, blend);
}

// Near detail fades into a much larger sample, removing the distant tile pattern.
vec3 layered(sampler2D source, vec2 world, float near_tile, float far_tile, float distance_blend) {
	vec3 far_sample = untiled(source, world, far_tile);
	if (distance_blend >= 1.0) {
		return far_sample;
	}
	return mix(untiled(source, world, near_tile), far_sample, distance_blend);
}

void vertex() {
	vec3 world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	detail_fade = smoothstep(65.0, 220.0, length(world_position - CAMERA_POSITION_WORLD));
	terrain_elevation = VERTEX.y;
	// Only open water moves. Two broad, low-amplitude swells stay legible at
	// RTS scale without turning the terrain or camera target into a shimmer.
	float open_water = 1.0 - smoothstep(0.03, 0.38, COLOR.r);
	float swell_a = sin(UV.x * texture_tiling * 18.0 + TIME * water_wave_speed * 2.2);
	float swell_b = sin(UV.y * texture_tiling * 24.0 - TIME * water_wave_speed * 1.6 + UV.x * 4.0);
	VERTEX.y += (swell_a + swell_b * 0.55) * water_wave_height * open_water;
	terrain_world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	terrain_world_normal = normalize(mat3(MODEL_MATRIX) * NORMAL);
}

void fragment() {
	vec2 tiled_uv = UV * texture_tiling;
	vec2 water_drift = vec2(0.013, -0.009) * TIME * water_wave_speed;
	vec3 ocean_a = texture(ocean_texture, tiled_uv + water_drift).rgb;
	vec3 ocean_b = texture(ocean_texture, tiled_uv * 1.18 - water_drift * 0.72 + vec2(0.37, 0.11)).rgb;
	float ripple = 0.5 + 0.5 * sin((UV.x * 150.0 + UV.y * 94.0) - TIME * water_wave_speed * 5.0);
	vec3 ocean = mix(ocean_a, ocean_b, 0.42 + ripple * 0.16);
	ocean = mix(ocean * vec3(0.72, 0.95, 1.08), vec3(0.075, 0.19, 0.27), 0.62);
	vec2 ground = terrain_world_position.xz;
	float view_distance = length(terrain_world_position - CAMERA_POSITION_WORLD);
	float distance_blend = smoothstep(500.0, 3200.0, view_distance);
	vec4 masks = masks_enabled ? texture(terrain_masks, UV) : vec4(0.0, 0.0, 1.0, 0.5);
	float broad = layered_noise(ground / 2200.0);
	float fine = layered_noise(ground / 170.0);
	vec3 shore = layered(shore_texture, ground, 90.0, 1300.0, distance_blend);
	vec3 grassland = layered(grassland_texture, ground, 46.0, 900.0, distance_blend);
	vec3 forest = layered(forest_texture, ground, 38.0, 760.0, distance_blend);
	vec3 mud = layered(mud_texture, ground, 52.0, 1100.0, distance_blend);
	vec3 rocky = layered(rocky_texture, ground, 64.0, 1500.0, distance_blend);

	float water_to_shore = smoothstep(0.16, 0.30, COLOR.r);
	float shore_to_grass = smoothstep(0.30, 0.46, COLOR.r);
	vec3 terrain = mix(ocean, shore, water_to_shore);
	float shoreline_foam = smoothstep(0.13, 0.2, COLOR.r) * (1.0 - smoothstep(0.2, 0.26, COLOR.r));
	terrain = mix(terrain, vec3(0.58, 0.73, 0.76), shoreline_foam * (0.24 + ripple * 0.16));
	float steepness = 1.0 - clamp(terrain_world_normal.y, 0.0, 1.0);
	float elevation = terrain_elevation + (broad - 0.5) * 240.0;
	// Grass shifts between lush and dry across the map rather than one flat hue.
	vec3 lush = grassland * vec3(0.98, 1.1, 0.78);
	vec3 dry = grassland * vec3(1.2, 1.08, 0.78);
	float dryness = smoothstep(0.34, 0.68, masks.a * 0.65 + broad * 0.35);
	vec3 land = mix(lush, dry, dryness);
	land = mix(land, mud * vec3(1.05, 0.98, 0.86), smoothstep(0.58, 0.74, fine * 0.55 + masks.a * 0.45) * 0.62);
	// Pasture patches and fine tonal grain stop open ground reading as one surface.
	float pasture = layered_noise(ground / 460.0 + 19.0);
	land = mix(land, land * vec3(1.26, 1.12, 0.82), smoothstep(0.54, 0.62, pasture) * 0.42);
	land = mix(land, land * vec3(0.8, 0.92, 0.76), (1.0 - smoothstep(0.36, 0.44, pasture)) * 0.42);
	land *= 0.8 + 0.4 * mix(layered_noise(ground / 26.0), fine, distance_blend);
	// Low ground near drainage stays greener, and the highest pasture thins out.
	land = mix(land, lush * 0.88, (1.0 - smoothstep(30.0, 190.0, elevation)) * 0.34);
	land = mix(land, mix(land, rocky, 0.55) * vec3(1.02, 0.98, 0.9), smoothstep(720.0, 1320.0, elevation));
	// Steep faces expose stone in horizontal strata, whatever the map direction.
	vec3 strata = texture(rocky_texture, vec2(ground.x + ground.y, terrain_world_position.y * 1.8) / 900.0).rgb;
	vec3 cliff = mix(rocky, strata, 0.3) * vec3(1.06, 1.0, 0.93);
	float exposed = smoothstep(0.10, 0.23, steepness + (fine - 0.5) * 0.09);
	land = mix(land, cliff, exposed);
	// Snow settles only on the summits and slides off the steepest rock.
	float snow = smoothstep(1560.0, 1860.0, elevation) * (1.0 - smoothstep(0.2, 0.36, steepness));
	land = mix(land, vec3(0.86, 0.88, 0.91) * (0.86 + fine * 0.16), snow);
	// The forest floor sits under the 3D trees. Past their view range, the same
	// mask becomes a canopy so woodland still reads from strategic zoom.
	float woodland = masks.g * (1.0 - exposed);
	float canopy_blend = smoothstep(3400.0, 9000.0, view_distance);
	vec3 canopy = mix(vec3(0.07, 0.15, 0.07), vec3(0.17, 0.26, 0.11), layered_noise(ground / 150.0));
	canopy = mix(canopy, canopy * vec3(0.8, 0.92, 1.0), smoothstep(380.0, 1100.0, elevation));
	land = mix(land, mix(forest * vec3(0.5, 0.64, 0.42), canopy, canopy_blend), woodland);
	// River banks, then the water itself, which runs along the same baked mask.
	float bank = smoothstep(0.02, 0.5, masks.r);
	land = mix(land, mix(mud, shore, 0.5) * 0.9, bank * 0.8);
	float river = smoothstep(0.55, 0.8, masks.r);
	vec3 river_water = mix(ocean * vec3(0.8, 1.04, 1.1), vec3(0.17, 0.33, 0.37), 0.5);
	land = mix(land, river_water, river);
	land *= 0.86 + broad * 0.28;
	land = mix(land, vec3(dot(land, vec3(0.3, 0.59, 0.11))), 0.18);
	land *= mix(1.0, masks.b, 0.92);
	// Faint distance haze separates ridgelines at strategic zoom.
	land = mix(land, vec3(0.6, 0.68, 0.76), (1.0 - exp(-view_distance / 75000.0)) * 0.3);
	terrain = mix(terrain, land, shore_to_grass);
	// Land and sea both dissolve into the sky color before the far plane.
	terrain = mix(terrain, vec3(0.56, 0.66, 0.72), smoothstep(26000.0, 88000.0, view_distance));

	vec4 territory_color = texture(territory_texture, tiled_uv);
	vec3 territory_override = vec3(0.0);
	territory_override += territory_color.r * vec3(1.0, 0.4, 0.0);  // RECONZONE/CLAIMED
	territory_override += territory_color.g * vec3(0.1, 0.8, 0.1);  // SECURED
	territory_override += territory_color.b * vec3(0.3, 0.3, 1.0);  // CONTESTED
	terrain = mix(terrain, terrain + territory_override * territory_intensity, territory_color.a);
	float local_beacon_light = 0.0;
	for (int index = 0; index < 8; index++) {
		vec4 beacon = fog_beacon_lights[index];
		if (beacon.z > 0.0) {
			float distance_to_beacon = distance(terrain_world_position.xz, beacon.xy);
			local_beacon_light += (1.0 - smoothstep(beacon.z * 0.15, beacon.z, distance_to_beacon)) * beacon.w;
		}
	}
	vec3 unlit_terrain = terrain;
	terrain *= terrain_brightness * terrain_daylight;
	// Floodlights must remain useful in nighttime conditions, independent of
	// the terrain's global daylight attenuation.
	terrain += unlit_terrain * clamp(local_beacon_light, 0.0, 1.25);

	ALBEDO = terrain;
	// Compatibility-renderer terrain can lose sky ambient in directional
	// shadows. A restrained material fill keeps daytime valleys readable.
	EMISSION = terrain * terrain_ambient_fill + unlit_terrain * clamp(local_beacon_light, 0.0, 1.0) * 0.35;
	float wet = max(1.0 - shore_to_grass, smoothstep(0.55, 0.8, masks.r));
	ROUGHNESS = mix(0.9, 0.28, wet);
	SPECULAR = mix(0.2, 0.82, wet);
	METALLIC = 0.0;
}
