// =============================================================================
// tcxFlowTools GPU physarum-style trail passes
// =============================================================================

@vs physarum_fullscreen_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_physarum_spawn
layout(binding=0) uniform physarum_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float physarum_hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 physarum_hash2(vec2 p) {
    float a = physarum_hash(p);
    float b = physarum_hash(p.yx + vec2(a, 1.0 - a) + 19.19);
    return vec2(a, b);
}

void main() {
    vec2 canvas = max(options.xy, vec2(1.0));
    vec2 seed = physarum_hash2(uv + options.zw);
    vec2 pos = fract(seed + physarum_hash2(seed + uv * 17.0) * 0.6180339887);
    frag_color = vec4(pos * canvas, 0.0, 0.0);
}
@end

@fs fs_physarum_age_spawn
layout(binding=0) uniform physarum_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float physarum_age_hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    float age = physarum_age_hash(uv + options.zw) * max(options.x, 1.0);
    frag_color = vec4(age, 0.0, 0.0, 1.0);
}
@end

@fs fs_physarum_age_update
layout(binding=0) uniform texture2D physarumAgeUpdateTex;
layout(binding=0) uniform sampler physarumAgeUpdateSmp;

layout(binding=0) uniform physarum_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    float lifetime = max(options.x, 1.0);
    float age = texture(sampler2D(physarumAgeUpdateTex, physarumAgeUpdateSmp), uv).x + max(options.y, 0.0);
    frag_color = vec4(age >= lifetime ? 0.0 : age, 0.0, 0.0, 1.0);
}
@end

@fs fs_physarum_update
layout(binding=0) uniform texture2D physarumStateTex;
layout(binding=0) uniform sampler physarumStateSmp;
layout(binding=1) uniform texture2D physarumVelocityTex;
layout(binding=1) uniform sampler physarumVelocitySmp;
layout(binding=2) uniform texture2D physarumAgeTex;
layout(binding=2) uniform sampler physarumAgeSmp;
layout(binding=3) uniform texture2D physarumInitialTex;
layout(binding=3) uniform sampler physarumInitialSmp;

layout(binding=0) uniform physarum_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float physarum_update_hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 physarum_update_hash2(vec2 p) {
    float a = physarum_update_hash(p);
    float b = physarum_update_hash(p.yx + vec2(a, 1.0 - a) + 23.17);
    return vec2(a, b);
}

void main() {
    vec2 canvas = max(options.yz, vec2(1.0));
    float stepScale = max(options.x, 0.0);
    float maxStep = max(options.w, 0.25);
    vec4 state = texture(sampler2D(physarumStateTex, physarumStateSmp), uv);
    vec2 absolute = state.xy;
    vec2 displacement = state.zw;
    vec2 position = absolute + displacement;

    vec2 velocity1 = texture(sampler2D(physarumVelocityTex, physarumVelocitySmp), fract(position / canvas)).xy;
    vec2 halfStep = velocity1 * stepScale * 0.5;
    float halfLen = length(halfStep);
    if (halfLen > maxStep * 0.5) {
        halfStep *= (maxStep * 0.5) / halfLen;
    }
    vec2 velocity2 = texture(sampler2D(physarumVelocityTex, physarumVelocitySmp), fract((position + halfStep) / canvas)).xy;
    vec2 stepPx = velocity2 * stepScale;
    float stepLen = length(stepPx);
    if (stepLen > maxStep) {
        stepPx *= maxStep / stepLen;
    }

    displacement += stepPx;
    float shouldMerge = step(20.0, dot(displacement, displacement));
    absolute = mod(absolute + shouldMerge * displacement + canvas, canvas);
    displacement *= (1.0 - shouldMerge);

    vec4 advected = vec4(absolute, displacement);
    float age = texture(sampler2D(physarumAgeTex, physarumAgeSmp), uv).x;
    float shouldReset = 1.0 - step(0.5, age);
    frag_color = mix(advected, texture(sampler2D(physarumInitialTex, physarumInitialSmp), uv), shouldReset);
}
@end

@vs physarum_deposit_vs
in vec2 lookup;
in vec2 corner;

layout(binding=0) uniform texture2D physarumPointStateTex;
layout(binding=0) uniform sampler physarumPointStateSmp;
layout(binding=1) uniform texture2D physarumPointVelocityTex;
layout(binding=1) uniform sampler physarumPointVelocitySmp;
layout(binding=2) uniform texture2D physarumPointAgeTex;
layout(binding=2) uniform sampler physarumPointAgeSmp;

layout(binding=0) uniform physarum_point_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

out vec3 v_rgb;
out float v_alpha;

float physarum_point_hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec4 state = texture(sampler2D(physarumPointStateTex, physarumPointStateSmp), lookup);
    vec2 canvas = max(resolution.xy, vec2(1.0));
    vec2 pos = state.xy + state.zw;
    vec2 uvPos = fract(pos / canvas);
    vec2 velocity = texture(sampler2D(physarumPointVelocityTex, physarumPointVelocitySmp), uvPos).xy;
    float age = texture(sampler2D(physarumPointAgeTex, physarumPointAgeSmp), lookup).x;
    float seed = physarum_point_hash(lookup + vec2(0.13, 0.71));

    float inkStrength = clamp(options.y, 0.0, 3.0);
    float ageFraction = clamp(age / max(options.z, 1.0), 0.0, 1.0);
    float opacity = min(ageFraction * 10.0, 1.0);
    float shade = mix(0.74, 1.24, physarum_point_hash(lookup + vec2(seed, 0.41)));
    float multiplier = clamp(dot(velocity, velocity) * 0.05 + 0.70, 0.0, 1.0);
    float alpha = clamp((0.064 + 0.190 * multiplier) * opacity * shade * inkStrength, 0.0, 0.44);
    float pointSize = max(options.x, 0.75);
    vec2 pixel = uvPos * canvas + corner * pointSize * 0.5;

    vec2 ndc = pixel / canvas * 2.0 - 1.0;
    ndc.y = -ndc.y;
    v_rgb = color.rgb;
    v_alpha = color.a * alpha;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
@end

@fs fs_physarum_deposit
in vec3 v_rgb;
in float v_alpha;
out vec4 frag_color;

void main() {
    frag_color = vec4(v_rgb, v_alpha);
}
@end

@program tcx_flow_physarum_spawn physarum_fullscreen_vs fs_physarum_spawn
@program tcx_flow_physarum_age_spawn physarum_fullscreen_vs fs_physarum_age_spawn
@program tcx_flow_physarum_age_update physarum_fullscreen_vs fs_physarum_age_update
@program tcx_flow_physarum_update physarum_fullscreen_vs fs_physarum_update
@program tcx_flow_physarum_deposit physarum_deposit_vs fs_physarum_deposit
