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

varying float detail_fade;
varying float terrain_elevation;
varying vec3 terrain_world_position;

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
}

void fragment() {
	vec2 tiled_uv = UV * texture_tiling;
	vec2 water_drift = vec2(0.013, -0.009) * TIME * water_wave_speed;
	vec3 ocean_a = texture(ocean_texture, tiled_uv + water_drift).rgb;
	vec3 ocean_b = texture(ocean_texture, tiled_uv * 1.18 - water_drift * 0.72 + vec2(0.37, 0.11)).rgb;
	float ripple = 0.5 + 0.5 * sin((UV.x * 150.0 + UV.y * 94.0) - TIME * water_wave_speed * 5.0);
	vec3 ocean = mix(ocean_a, ocean_b, 0.42 + ripple * 0.16);
	vec3 shore = texture(shore_texture, tiled_uv).rgb;
	vec3 grassland = texture(grassland_texture, tiled_uv).rgb;
	vec3 forest = texture(forest_texture, tiled_uv * 0.82).rgb;
	vec3 mud = texture(mud_texture, tiled_uv * 1.12).rgb;
	vec3 rocky = texture(rocky_texture, tiled_uv * 0.68).rgb;

	float water_to_shore = smoothstep(0.03, 0.38, COLOR.r);
	float shore_to_grass = smoothstep(0.36, 0.74, COLOR.r);
	vec3 terrain = mix(ocean, shore, water_to_shore);
	float shoreline_foam = smoothstep(0.10, 0.34, COLOR.r) * (1.0 - smoothstep(0.34, 0.52, COLOR.r));
	terrain = mix(terrain, vec3(0.58, 0.73, 0.76), shoreline_foam * (0.24 + ripple * 0.16));
	vec3 land = grassland;
	float variation = 0.5 + 0.5 * sin(UV.x * 18.0 + sin(UV.y * 13.0) * 2.0);
	land = mix(land, mud, smoothstep(0.34, 0.52, variation) * 0.45);
	land = mix(land, forest, smoothstep(0.58, 0.76, variation) * 0.38);
	land = mix(land, rocky, smoothstep(0.82, 0.96, variation) * 0.28);
	// Elevation bands keep valleys and mountain belts visible without relying on
	// dynamic lighting in the compatibility renderer.
	float uplands = smoothstep(120.0, 620.0, terrain_elevation);
	float summits = smoothstep(650.0, 1250.0, terrain_elevation);
	land = mix(land, rocky, uplands * 0.65);
	land = mix(land, vec3(0.46, 0.47, 0.44), summits * 0.70);
	// Do not push strategic-distance terrain to a dark green. That older fade
	// overpowered clear daytime brightness and made the map read as night.
	land = mix(land, land * vec3(0.94, 0.98, 0.94), detail_fade * 0.20);
	terrain = mix(terrain, land, shore_to_grass);

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
	ROUGHNESS = mix(0.28, 0.9, shore_to_grass);
	SPECULAR = mix(0.82, 0.35, shore_to_grass);
	METALLIC = 0.0;
}
