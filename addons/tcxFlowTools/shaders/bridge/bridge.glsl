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

float bridge_flag(float packed, float flag) {
    return floor(mod(floor(packed / flag), 2.0));
}

vec2 bridge_sample_uv(vec2 p) {
    float mirror_x = bridge_flag(options.w, 4.0);
    float mirror_y = bridge_flag(options.w, 8.0);
    return vec2(mix(p.x, 1.0 - p.x, mirror_x),
                mix(p.y, 1.0 - p.y, mirror_y));
}

float bridge_luminance(vec4 src) {
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    return mix(y, 1.0 - y, bridge_flag(options.w, 1.0));
}

float bridge_mask_source_mode() {
    return floor(options.w / 16.0);
}

float bridge_saturation(vec3 rgb) {
    float hi = max(max(rgb.r, rgb.g), rgb.b);
    float lo = min(min(rgb.r, rgb.g), rgb.b);
    return hi <= 0.000001 ? 0.0 : (hi - lo) / hi;
}

float bridge_mask_gamma() {
    return max(texel.w, 0.0001);
}

float bridge_mask_value(vec4 src, float intensity) {
    float use_alpha = bridge_flag(options.w, 2.0);
    float mode = bridge_mask_source_mode();
    if (use_alpha > 0.5) mode = 1.0;
    if (mode > 0.5 && mode < 1.5) return src.a;
    if (mode > 1.5 && mode < 2.5) return src.r;
    if (mode > 2.5 && mode < 3.5) return src.g;
    if (mode > 3.5 && mode < 4.5) return src.b;
    if (mode > 4.5 && mode < 5.5) return max(max(src.r, src.g), src.b);
    if (mode > 5.5) return bridge_saturation(src.rgb);
    return intensity;
}

float bridge_soft_mask(float value) {
    float softness = max(texel.z, 0.0);
    float mask = softness <= 0.00001
        ? step(options.y, value)
        : smoothstep(options.y - softness * 0.5, options.y + softness * 0.5, value);
    return pow(clamp(mask, 0.0, 1.0), 1.0 / bridge_mask_gamma());
}

void main() {
    vec4 src = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv));
    float y = bridge_luminance(src);
    float mask = bridge_soft_mask(bridge_mask_value(src, y));
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

float bridge_flag(float packed, float flag) {
    return floor(mod(floor(packed / flag), 2.0));
}

vec2 bridge_sample_uv(vec2 p) {
    float mirror_x = bridge_flag(options.w, 4.0);
    float mirror_y = bridge_flag(options.w, 8.0);
    return vec2(mix(p.x, 1.0 - p.x, mirror_x),
                mix(p.y, 1.0 - p.y, mirror_y));
}

float bridge_luminance(vec4 src) {
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    return mix(y, 1.0 - y, bridge_flag(options.w, 1.0));
}

float bridge_mask_source_mode() {
    return floor(options.w / 16.0);
}

float bridge_saturation(vec3 rgb) {
    float hi = max(max(rgb.r, rgb.g), rgb.b);
    float lo = min(min(rgb.r, rgb.g), rgb.b);
    return hi <= 0.000001 ? 0.0 : (hi - lo) / hi;
}

float bridge_mask_gamma() {
    return max(texel.w, 0.0001);
}

float bridge_mask_value(vec4 src, float intensity) {
    float use_alpha = bridge_flag(options.w, 2.0);
    float mode = bridge_mask_source_mode();
    if (use_alpha > 0.5) mode = 1.0;
    if (mode > 0.5 && mode < 1.5) return src.a;
    if (mode > 1.5 && mode < 2.5) return src.r;
    if (mode > 2.5 && mode < 3.5) return src.g;
    if (mode > 3.5 && mode < 4.5) return src.b;
    if (mode > 4.5 && mode < 5.5) return max(max(src.r, src.g), src.b);
    if (mode > 5.5) return bridge_saturation(src.rgb);
    return intensity;
}

float bridge_soft_mask(float value) {
    float softness = max(texel.z, 0.0);
    float mask = softness <= 0.00001
        ? step(options.y, value)
        : smoothstep(options.y - softness * 0.5, options.y + softness * 0.5, value);
    return pow(clamp(mask, 0.0, 1.0), 1.0 / bridge_mask_gamma());
}

void main() {
    vec2 px = texel.xy * max(options.z, 1.0);
    vec4 src_c = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv));
    vec4 src_l = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv - vec2(px.x, 0.0)));
    vec4 src_r = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv + vec2(px.x, 0.0)));
    vec4 src_u = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv - vec2(0.0, px.y)));
    vec4 src_d = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv + vec2(0.0, px.y)));
    float c = bridge_luminance(src_c);
    float l = bridge_luminance(src_l);
    float r = bridge_luminance(src_r);
    float u = bridge_luminance(src_u);
    float d = bridge_luminance(src_d);
    float mask = bridge_soft_mask(max(max(max(max(bridge_mask_value(src_c, c),
                                                 bridge_mask_value(src_l, l)),
                                             bridge_mask_value(src_r, r)),
                                         bridge_mask_value(src_u, u)),
                                     bridge_mask_value(src_d, d)));
    vec2 v = vec2(r - l, d - u) * options.x * mask * color.xy;
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

float bridge_flag(float packed, float flag) {
    return floor(mod(floor(packed / flag), 2.0));
}

vec2 bridge_sample_uv(vec2 p) {
    float mirror_x = bridge_flag(options.w, 4.0);
    float mirror_y = bridge_flag(options.w, 8.0);
    return vec2(mix(p.x, 1.0 - p.x, mirror_x),
                mix(p.y, 1.0 - p.y, mirror_y));
}

float bridge_luminance(vec4 src) {
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    return mix(y, 1.0 - y, bridge_flag(options.w, 1.0));
}

float bridge_mask_source_mode() {
    return floor(options.w / 16.0);
}

float bridge_saturation(vec3 rgb) {
    float hi = max(max(rgb.r, rgb.g), rgb.b);
    float lo = min(min(rgb.r, rgb.g), rgb.b);
    return hi <= 0.000001 ? 0.0 : (hi - lo) / hi;
}

float bridge_mask_gamma() {
    return max(texel.w, 0.0001);
}

float bridge_mask_value(vec4 src, float intensity) {
    float use_alpha = bridge_flag(options.w, 2.0);
    float mode = bridge_mask_source_mode();
    if (use_alpha > 0.5) mode = 1.0;
    if (mode > 0.5 && mode < 1.5) return src.a;
    if (mode > 1.5 && mode < 2.5) return src.r;
    if (mode > 2.5 && mode < 3.5) return src.g;
    if (mode > 3.5 && mode < 4.5) return src.b;
    if (mode > 4.5 && mode < 5.5) return max(max(src.r, src.g), src.b);
    if (mode > 5.5) return bridge_saturation(src.rgb);
    return intensity;
}

float bridge_soft_mask(float value) {
    float softness = max(texel.z, 0.0);
    float mask = softness <= 0.00001
        ? step(options.y, value)
        : smoothstep(options.y - softness * 0.5, options.y + softness * 0.5, value);
    return pow(clamp(mask, 0.0, 1.0), 1.0 / bridge_mask_gamma());
}

void main() {
    vec4 src = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv));
    float y = bridge_luminance(src);
    float mask = bridge_soft_mask(bridge_mask_value(src, y));
    float density = y * options.x * mask;
    frag_color = vec4(color.rgb * density, density);
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

float bridge_flag(float packed, float flag) {
    return floor(mod(floor(packed / flag), 2.0));
}

vec2 bridge_sample_uv(vec2 p) {
    float mirror_x = bridge_flag(options.w, 4.0);
    float mirror_y = bridge_flag(options.w, 8.0);
    return vec2(mix(p.x, 1.0 - p.x, mirror_x),
                mix(p.y, 1.0 - p.y, mirror_y));
}

float bridge_luminance(vec4 src) {
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    return mix(y, 1.0 - y, bridge_flag(options.w, 1.0));
}

float bridge_mask_source_mode() {
    return floor(options.w / 16.0);
}

float bridge_saturation(vec3 rgb) {
    float hi = max(max(rgb.r, rgb.g), rgb.b);
    float lo = min(min(rgb.r, rgb.g), rgb.b);
    return hi <= 0.000001 ? 0.0 : (hi - lo) / hi;
}

float bridge_mask_gamma() {
    return max(texel.w, 0.0001);
}

float bridge_mask_value(vec4 src, float intensity) {
    float use_alpha = bridge_flag(options.w, 2.0);
    float mode = bridge_mask_source_mode();
    if (use_alpha > 0.5) mode = 1.0;
    if (mode > 0.5 && mode < 1.5) return src.a;
    if (mode > 1.5 && mode < 2.5) return src.r;
    if (mode > 2.5 && mode < 3.5) return src.g;
    if (mode > 3.5 && mode < 4.5) return src.b;
    if (mode > 4.5 && mode < 5.5) return max(max(src.r, src.g), src.b);
    if (mode > 5.5) return bridge_saturation(src.rgb);
    return intensity;
}

float bridge_soft_mask(float value) {
    float softness = max(texel.z, 0.0);
    float mask = softness <= 0.00001
        ? step(options.y, value)
        : smoothstep(options.y - softness * 0.5, options.y + softness * 0.5, value);
    return pow(clamp(mask, 0.0, 1.0), 1.0 / bridge_mask_gamma());
}

void main() {
    vec4 src = texture(sampler2D(bridgeInputTex, bridgeInputSmp), bridge_sample_uv(uv));
    float y = bridge_luminance(src);
    float mask = bridge_soft_mask(bridge_mask_value(src, y));
    float t = max(y - options.y, 0.0) * options.x * mask;
    frag_color = vec4(vec3(t), t) * color;
}
@end

@program tcx_flow_bridge_luminance_mask bridge_vs fs_bridge_luminance_mask
@program tcx_flow_bridge_velocity bridge_vs fs_bridge_velocity
@program tcx_flow_bridge_density bridge_vs fs_bridge_density
@program tcx_flow_bridge_temperature bridge_vs fs_bridge_temperature
