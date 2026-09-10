shader_type spatial;
// This must remain readable in Godot's Compatibility renderer on the demo
// machine. The generated grid has no authoring-time winding guarantee, so do
// not cull it; unshaded terrain keeps the material legible under every scene
// lighting configuration.
render_mode cull_disabled, unshaded;

uniform sampler2D ocean_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D shore_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D grassland_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D forest_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D mud_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform sampler2D rocky_texture : source_color, repeat_enable, filter_linear_mipmap;
uniform float texture_tiling = 8.0;
varying float detail_fade;

void vertex() {
	vec3 world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	detail_fade = smoothstep(65.0, 220.0, length(world_position - CAMERA_POSITION_WORLD));
}

void fragment() {
	// Keep texture coordinates fixed in world space. Mip sampling handles the
	// far view; changing UV scale with zoom made the ground appear to wave.
	vec2 tiled_uv = UV * texture_tiling;
	vec3 ocean = texture(ocean_texture, tiled_uv).rgb;
	vec3 shore = texture(shore_texture, tiled_uv).rgb;
	vec3 grassland = texture(grassland_texture, tiled_uv).rgb;
	vec3 forest = texture(forest_texture, tiled_uv * 0.82).rgb;
	vec3 mud = texture(mud_texture, tiled_uv * 1.12).rgb;
	vec3 rocky = texture(rocky_texture, tiled_uv * 0.68).rgb;

	// HeightMap stores the land mask in vertex-color red: 0 is open water,
	// 1 is established land, with a broad blend band for a natural shoreline.
	float water_to_shore = smoothstep(0.03, 0.38, COLOR.r);
	float shore_to_grass = smoothstep(0.36, 0.74, COLOR.r);
	vec3 terrain = mix(ocean, shore, water_to_shore);
	vec3 land = grassland;
	// Deterministic macro variation keeps the imported surfaces readable while
	// breaking up the uniform procedural grass on the larger theater.
	float variation = 0.5 + 0.5 * sin(UV.x * 18.0 + sin(UV.y * 13.0) * 2.0);
	land = mix(land, mud, smoothstep(0.34, 0.52, variation) * 0.45);
	land = mix(land, forest, smoothstep(0.58, 0.76, variation) * 0.38);
	land = mix(land, rocky, smoothstep(0.82, 0.96, variation) * 0.28);
	// Let mipmaps preserve the material response, then gently desaturate its
	// high-frequency contrast at tactical/strategic distance. The UVs remain
	// fixed, so this cannot produce the zoom-dependent wave seen previously.
	land = mix(land, vec3(0.25, 0.39, 0.22), detail_fade * 0.62);
	terrain = mix(terrain, land, shore_to_grass);

	ALBEDO = terrain;
}
