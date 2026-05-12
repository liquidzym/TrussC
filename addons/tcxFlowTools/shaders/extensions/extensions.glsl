// =============================================================================
// tcxFlowTools extension helper passes
// =============================================================================

@vs extensions_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_split_velocity
layout(binding=0) uniform texture2D splitVelocityTex;
layout(binding=0) uniform sampler splitVelocitySmp;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 v = texture(sampler2D(splitVelocityTex, splitVelocitySmp), uv).xy * options.x;
    vec4 split = vec4(max(v.x, 0.0), max(v.y, 0.0), max(-v.x, 0.0), max(-v.y, 0.0));
    float energy = clamp(split.x + split.y + split.z + split.w, 0.0, 1.0);

    vec3 combined = vec3(
        split.x + split.z * 0.25,
        split.y + split.w * 0.35,
        split.z + split.w * 0.75
    );
    vec3 positive = vec3(split.x, split.y, energy * 0.08);
    vec3 negative = vec3(split.z * 0.45, split.w * 0.25, split.z + split.w);
    vec3 rgb = combined;
    if (options.y > 0.5 && options.y < 1.5) {
        rgb = positive;
    } else if (options.y > 1.5) {
        rgb = negative;
    }
    frag_color = vec4(clamp(rgb * color.rgb, 0.0, 1.0), color.a);
}
@end

@program tcx_flow_extensions_split_velocity extensions_vs fs_split_velocity
