shader_type spatial;
render_mode unshaded, depth_draw_never;

uniform float opacity = 0.7;
uniform float age = 0.0;
varying vec3 local_point;

float hash(vec3 point) {
	return fract(sin(dot(point, vec3(17.13, 83.71, 41.53))) * 43758.5453);
}

float noise(vec3 point) {
	vec3 cell = floor(point);
	vec3 blend = fract(point);
	blend = blend * blend * (3.0 - 2.0 * blend);
	return mix(
		mix(mix(hash(cell), hash(cell + vec3(1, 0, 0)), blend.x),
			mix(hash(cell + vec3(0, 1, 0)), hash(cell + vec3(1, 1, 0)), blend.x), blend.y),
		mix(mix(hash(cell + vec3(0, 0, 1)), hash(cell + vec3(1, 0, 1)), blend.x),
			mix(hash(cell + vec3(0, 1, 1)), hash(cell + vec3(1, 1, 1)), blend.x), blend.y), blend.z);
}

void vertex() {
	local_point = VERTEX;
}

void fragment() {
	float cloud = noise(local_point * 4.0 + vec3(age * 0.3, -age * 0.6, 0));
	float edge = pow(max(dot(normalize(NORMAL), normalize(VIEW)), 0.0), 1.5);
	ALBEDO = mix(vec3(0.16, 0.18, 0.19), vec3(0.4, 0.42, 0.44), cloud);
	ALPHA = opacity * edge * smoothstep(0.15, 0.65, cloud);
}
