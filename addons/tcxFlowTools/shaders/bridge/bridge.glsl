// =============================================================================
// tcxFlowTools bridge fullscreen passes
// =============================================================================

@vs bridge_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_bridge_luminance_mask
layout(binding=0) uniform texture2D bridgeInputTex;
layout(binding=0) uniform sampler bridgeInputSmp;

layout(binding=0) uniform bridge_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv);
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    float mask = step(options.y, y);
    frag_color = vec4(vec3(y * mask * options.x), src.a * mask) * color;
}
@end

@fs fs_bridge_velocity
layout(binding=0) uniform texture2D bridgeInputTex;
layout(binding=0) uniform sampler bridgeInputSmp;

layout(binding=0) uniform bridge_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy * max(options.z, 1.0);
    float l = dot(texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv - vec2(px.x, 0.0)).rgb, vec3(0.2126, 0.7152, 0.0722));
    float r = dot(texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv + vec2(px.x, 0.0)).rgb, vec3(0.2126, 0.7152, 0.0722));
    float u = dot(texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv - vec2(0.0, px.y)).rgb, vec3(0.2126, 0.7152, 0.0722));
    float d = dot(texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv + vec2(0.0, px.y)).rgb, vec3(0.2126, 0.7152, 0.0722));
    vec2 v = vec2(r - l, d - u) * options.x * color.xy;
    frag_color = vec4(v, 0.0, 1.0);
}
@end

@fs fs_bridge_density
layout(binding=0) uniform texture2D bridgeInputTex;
layout(binding=0) uniform sampler bridgeInputSmp;

layout(binding=0) uniform bridge_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv);
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    float mask = step(options.y, y);
    frag_color = vec4(color.rgb * y * options.x * mask, max(src.a, y) * mask);
}
@end

@fs fs_bridge_temperature
layout(binding=0) uniform texture2D bridgeInputTex;
layout(binding=0) uniform sampler bridgeInputSmp;

layout(binding=0) uniform bridge_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(bridgeInputTex, bridgeInputSmp), uv);
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    float t = max(y - options.y, 0.0) * options.x;
    frag_color = vec4(t, t * 0.25, 1.0 - t, src.a) * color;
}
@end

@program tcx_flow_bridge_luminance_mask bridge_vs fs_bridge_luminance_mask
@program tcx_flow_bridge_velocity bridge_vs fs_bridge_velocity
@program tcx_flow_bridge_density bridge_vs fs_bridge_density
@program tcx_flow_bridge_temperature bridge_vs fs_bridge_temperature
