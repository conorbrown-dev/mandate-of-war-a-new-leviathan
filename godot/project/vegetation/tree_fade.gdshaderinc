// Hand-over between a tree's real model and its impostor.
//
// Near the camera (terrain_vegetation.gd's near ring) a tree is drawn twice:
// once as a model and once as an impostor. Both work out the same fade from
// the same tree origin and camera, and each keeps the pixels the other throws
// away, using the same screen-space dither. So across the fade band the tree
// dissolves from one form into the other with no gap and no double drawing.
//
// fade 0 = model only, 1 = impostor only. A material with near_fade_end at or
// below near_fade_start never fades (always impostor), as for shrubs.

uniform float near_fade_start = 120.0;
uniform float near_fade_end = 150.0;

const float TREE_BAYER[16] = { 0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0 };

float tree_near_fade(vec3 tree_origin, vec3 camera) {
	if (near_fade_end <= near_fade_start) {
		return 1.0;
	}
	return clamp((distance(tree_origin, camera) - near_fade_start) / (near_fade_end - near_fade_start), 0.0, 1.0);
}

// A threshold in (0, 1) from a 4x4 ordered-dither pattern.
float tree_dither(vec2 fragment_coord) {
	ivec2 cell = ivec2(mod(fragment_coord, 4.0));
	return (TREE_BAYER[cell.y * 4 + cell.x] + 0.5) / 16.0;
}
