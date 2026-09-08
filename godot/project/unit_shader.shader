shader_type spatial;

render_mode cull_disabled, unshaded;

uniform float unit_size : hint_range(0.1, 5.0) = 1.0;
uniform vec4 unit_color : source_color = vec4(0.0, 1.0, 0.0, 1.0);
uniform float health_ratio : hint_range(0.0, 1.0) = 1.0;

void vertex() {
    // Instance transform matrix comes from Godot's instancing system
}

void fragment() {
    ALBEDO = unit_color.rgb;
    EMISSION = unit_color.rgb * 0.2;
    ROUGHNESS = 0.3;
    METALLIC = 0.1;
}

void light() {
    // Unshaded mode, so no lighting contribution
}
