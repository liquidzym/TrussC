// =============================================================================
// tcxFlowTools optical-flow fullscreen passes
// =============================================================================
// The flow pass follows the ofxFlowTools and PixelFlow pattern:
// current/previous luminance difference plus spatial gradients, threshold,
// scale, and temporal smoothing.

@vs optical_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_optical_luminance
layout(binding=0) uniform texture2D oflowColorTex;
layout(binding=0) uniform sampler oflowColorSmp;

layout(binding=0) uniform optical_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(oflowColorTex, oflowColorSmp), uv);
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    frag_color = vec4(y, y, y, src.a) * color;
}
@end

@fs fs_optical_difference
layout(binding=0) uniform texture2D oflowCurrentTex;
layout(binding=0) uniform sampler oflowCurrentSmp;
layout(binding=1) uniform texture2D oflowPreviousTex;
layout(binding=1) uniform sampler oflowPreviousSmp;

layout(binding=0) uniform optical_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    float current = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv).r;
    float previous = texture(sampler2D(oflowPreviousTex, oflowPreviousSmp), uv).r;
    float diff = current - previous;
    frag_color = vec4(diff, abs(diff), 0.0, 1.0) * color;
}
@end

@fs fs_optical_gradient
layout(binding=0) uniform texture2D oflowCurrentTex;
layout(binding=0) uniform sampler oflowCurrentSmp;

layout(binding=0) uniform optical_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy * max(options.y, 1.0);
    float l = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv - vec2(px.x, 0.0)).r;
    float r = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv + vec2(px.x, 0.0)).r;
    float u = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv - vec2(0.0, px.y)).r;
    float d = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv + vec2(0.0, px.y)).r;
    frag_color = vec4((r - l) * 0.5, (d - u) * 0.5, 0.0, 1.0);
}
@end

@fs fs_optical_flow
layout(binding=0) uniform texture2D oflowCurrentTex;
layout(binding=0) uniform sampler oflowCurrentSmp;
layout(binding=1) uniform texture2D oflowPreviousTex;
layout(binding=1) uniform sampler oflowPreviousSmp;

layout(binding=0) uniform optical_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy * max(options.y, 1.0);
    float current = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv).r;
    float previous = texture(sampler2D(oflowPreviousTex, oflowPreviousSmp), uv).r;
    float l = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv - vec2(px.x, 0.0)).r;
    float r = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv + vec2(px.x, 0.0)).r;
    float u = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv - vec2(0.0, px.y)).r;
    float d = texture(sampler2D(oflowCurrentTex, oflowCurrentSmp), uv + vec2(0.0, px.y)).r;
    vec2 grad = vec2((r - l) * 0.5, (d - u) * 0.5);
    float dt = current - previous;
    float denom = max(dot(grad, grad) + options.z, 0.000001);
    vec2 flow = -dt * grad / denom;
    flow *= options.x * color.xy;
    float len = length(flow);
    float nextLen = max(len - options.w, 0.0);
    if (len > 0.000001) {
        flow *= nextLen / len;
    } else {
        flow = vec2(0.0);
    }
    frag_color = vec4(flow, 0.0, 1.0);
}
@end

@fs fs_optical_temporal_smooth
layout(binding=0) uniform texture2D oflowCurrentFlowTex;
layout(binding=0) uniform sampler oflowCurrentFlowSmp;
layout(binding=1) uniform texture2D oflowPreviousFlowTex;
layout(binding=1) uniform sampler oflowPreviousFlowSmp;

layout(binding=0) uniform optical_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 current = texture(sampler2D(oflowCurrentFlowTex, oflowCurrentFlowSmp), uv).xy;
    vec2 previous = texture(sampler2D(oflowPreviousFlowTex, oflowPreviousFlowSmp), uv).xy;
    vec2 flow = mix(current, previous, clamp(options.y, 0.0, 1.0)) * options.z;
    frag_color = vec4(flow, 0.0, 1.0);
}
@end

@fs fs_optical_visualize
layout(binding=0) uniform texture2D oflowCurrentFlowTex;
layout(binding=0) uniform sampler oflowCurrentFlowSmp;

layout(binding=0) uniform optical_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 flow = texture(sampler2D(oflowCurrentFlowTex, oflowCurrentFlowSmp), uv).xy * options.x;
    float mag = clamp(length(flow), 0.0, 1.0);
    vec3 color = vec3(max(flow.x, 0.0), max(flow.y, 0.0), max(-flow.x - flow.y, 0.0));
    frag_color = vec4(color + mag * 0.15, 1.0);
}
@end

@program tcx_flow_optical_luminance optical_vs fs_optical_luminance
@program tcx_flow_optical_difference optical_vs fs_optical_difference
@program tcx_flow_optical_gradient optical_vs fs_optical_gradient
@program tcx_flow_optical_flow optical_vs fs_optical_flow
@program tcx_flow_optical_temporal_smooth optical_vs fs_optical_temporal_smooth
@program tcx_flow_optical_visualize optical_vs fs_optical_visualize
