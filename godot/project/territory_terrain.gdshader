shader_type spatial;
render_mode cull_disabled, unshaded;

uniform sampler2D ocean_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D shore_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D grassland_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D forest_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D mud_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D rocky_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform float texture_tiling = 8.0;
uniform sampler2D territory_texture : repeat_enable, filter_linear_mipmap;
uniform float territory_intensity : hint_range(0.0, 1.0) = 0.4;

varying float detail_fade;

void vertex() {
	vec3 world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	detail_fade = smoothstep(65.0, 220.0, length(world_position - CAMERA_POSITION_WORLD));
}

void fragment() {
	vec2 tiled_uv = UV * texture_tiling;
	vec3 ocean = texture(ocean_texture, tiled_uv).rgb;
	vec3 shore = texture(shore_texture, tiled_uv).rgb;
	vec3 grassland = texture(grassland_texture, tiled_uv).rgb;
	vec3 forest = texture(forest_texture, tiled_uv * 0.82).rgb;
	vec3 mud = texture(mud_texture, tiled_uv * 1.12).rgb;
	vec3 rocky = texture(rocky_texture, tiled_uv * 0.68).rgb;

	float water_to_shore = smoothstep(0.03, 0.38, COLOR.r);
	float shore_to_grass = smoothstep(0.36, 0.74, COLOR.r);
	vec3 terrain = mix(ocean, shore, water_to_shore);
	vec3 land = grassland;
	float variation = 0.5 + 0.5 * sin(UV.x * 18.0 + sin(UV.y * 13.0) * 2.0);
	land = mix(land, mud, smoothstep(0.34, 0.52, variation) * 0.45);
	land = mix(land, forest, smoothstep(0.58, 0.76, variation) * 0.38);
	land = mix(land, rocky, smoothstep(0.82, 0.96, variation) * 0.28);
	land = mix(land, vec3(0.25, 0.39, 0.22), detail_fade * 0.62);
	terrain = mix(terrain, land, shore_to_grass);

	vec4 territory_color = texture(territory_texture, tiled_uv);
	vec3 territory_override = vec3(0.0);
	territory_override += territory_color.r * vec3(1.0, 0.4, 0.0);  // RECONZONE/CLAIMED
	territory_override += territory_color.g * vec3(0.1, 0.8, 0.1);  // SECURED
	territory_override += territory_color.b * vec3(0.3, 0.3, 1.0);  // CONTESTED
	terrain = mix(terrain, terrain + territory_override * territory_intensity, territory_color.a);

	ALBEDO = terrain;
}
