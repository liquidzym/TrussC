// =============================================================================
// tcxFlowTools visualization fullscreen passes
// =============================================================================

@vs visualization_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_visualize_scalar
layout(binding=0) uniform texture2D visualInputTex;
layout(binding=0) uniform sampler visualInputSmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    float v = texture(sampler2D(visualInputTex, visualInputSmp), uv).r * options.x;
    frag_color = vec4(color.rgb * clamp(v, 0.0, 1.0), color.a);
}
@end

@fs fs_visualize_density
layout(binding=0) uniform texture2D visualDensityTex;
layout(binding=0) uniform sampler visualDensitySmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 c = max(texture(sampler2D(visualDensityTex, visualDensitySmp), uv), vec4(0.0));
    float energy = clamp((c.r + c.g + c.b) * 0.38 + c.a * 0.55, 0.0, 1.0);
    float alpha = clamp(pow(energy, 0.72) * 1.35, 0.0, 1.0);
    vec3 rgb = vec3(
        clamp(c.r * 1.18 + energy * 0.04, 0.0, 1.0),
        clamp(c.g * 1.18 + energy * 0.05, 0.0, 1.0),
        clamp(c.b * 1.18 + energy * 0.06, 0.0, 1.0)
    );
    frag_color = vec4(rgb * color.rgb, alpha * color.a);
}
@end

@fs fs_visualize_velocity_color
layout(binding=0) uniform texture2D visualVelocityTex;
layout(binding=0) uniform sampler visualVelocitySmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 v = texture(sampler2D(visualVelocityTex, visualVelocitySmp), uv).xy * options.x;
    float mag = clamp(length(v), 0.0, 1.0);
    frag_color = vec4(max(v.x, 0.0) + mag * 0.1, max(v.y, 0.0) + mag * 0.1, max(-v.x - v.y, 0.0), 1.0) * color;
}
@end

@fs fs_visualize_pressure
layout(binding=0) uniform texture2D visualPressureTex;
layout(binding=0) uniform sampler visualPressureSmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    float p = texture(sampler2D(visualPressureTex, visualPressureSmp), uv).r * options.x;
    vec3 neg = vec3(0.05, 0.28, 0.85) * max(-p, 0.0);
    vec3 pos = vec3(0.95, 0.95, 0.95) * max(p, 0.0);
    frag_color = vec4(clamp(neg + pos, 0.0, 1.0), 1.0) * color;
}
@end

@fs fs_visualize_temperature
layout(binding=0) uniform texture2D visualTemperatureTex;
layout(binding=0) uniform sampler visualTemperatureSmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    float t = clamp(texture(sampler2D(visualTemperatureTex, visualTemperatureSmp), uv).r * options.x, 0.0, 1.0);
    vec3 cold = vec3(0.05, 0.12, 0.85);
    vec3 hot = vec3(1.0, 0.32, 0.08);
    vec3 c = mix(cold, hot, t) * smoothstep(0.01, 0.12, t);
    frag_color = vec4(c, 1.0) * color;
}
@end

@program tcx_flow_visualize_scalar visualization_vs fs_visualize_scalar
@program tcx_flow_visualize_density visualization_vs fs_visualize_density
@program tcx_flow_visualize_velocity_color visualization_vs fs_visualize_velocity_color
@program tcx_flow_visualize_pressure visualization_vs fs_visualize_pressure
@program tcx_flow_visualize_temperature visualization_vs fs_visualize_temperature
