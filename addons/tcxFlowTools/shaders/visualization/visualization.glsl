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
    float display_gain = max(options.x, 0.0);
    c.rgb *= display_gain;
    c.a *= min(display_gain, 2.0);
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

@fs fs_visualize_combined
layout(binding=0) uniform texture2D combinedDensityTex;
layout(binding=0) uniform sampler combinedDensitySmp;
layout(binding=1) uniform texture2D combinedVelocityTex;
layout(binding=1) uniform sampler combinedVelocitySmp;
layout(binding=2) uniform texture2D combinedTemperatureTex;
layout(binding=2) uniform sampler combinedTemperatureSmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 density = max(texture(sampler2D(combinedDensityTex, combinedDensitySmp), uv), vec4(0.0));
    vec2 velocity = texture(sampler2D(combinedVelocityTex, combinedVelocitySmp), uv).xy * options.x;
    float temperature = clamp(texture(sampler2D(combinedTemperatureTex, combinedTemperatureSmp), uv).r, 0.0, 1.0);

    float densityEnergy = clamp((density.r + density.g + density.b) * 0.38 + density.a * 0.55, 0.0, 1.0);
    float velocityEnergy = clamp(length(velocity), 0.0, 1.0);
    float tempEnergy = smoothstep(0.015, 0.75, temperature);

    vec3 densityRgb = density.rgb * (1.08 + densityEnergy * 0.35);
    vec3 velocityRgb = vec3(
        max(velocity.x, 0.0) + velocityEnergy * 0.12,
        max(velocity.y, 0.0) + velocityEnergy * 0.12,
        max(-velocity.x - velocity.y, 0.0)
    ) * (0.25 + densityEnergy * 0.75);
    vec3 temperatureRgb = mix(vec3(0.08, 0.05, 0.35), vec3(1.0, 0.30, 0.06), tempEnergy) * tempEnergy;

    vec3 rgb = clamp(densityRgb + velocityRgb * 0.65 + temperatureRgb * 0.85, 0.0, 1.0);
    frag_color = vec4(rgb * color.rgb, color.a);
}
@end

@fs fs_visualize_lic
layout(binding=0) uniform texture2D licVelocityTex;
layout(binding=0) uniform sampler licVelocitySmp;

layout(binding=0) uniform visual_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(127.1, 311.7, 74.7));
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    vec2 velocity = texture(sampler2D(licVelocityTex, licVelocitySmp), uv).xy;
    float speed = length(velocity);
    vec2 dir = speed > 0.00001 ? normalize(velocity) : vec2(1.0, 0.0);
    vec2 stepUv = dir * texel.xy * max(options.y, 1.0);
    int samples = int(clamp(floor(options.z + 0.5), 4.0, 32.0));

    float accum = noise(uv * resolution.xy * max(options.w, 0.25));
    float weight = 1.0;
    for (int i = 1; i <= 32; ++i) {
        if (i > samples) break;
        float w = 1.0 - float(i) / (float(samples) + 1.0);
        vec2 offset = stepUv * float(i);
        accum += (noise((uv + offset) * resolution.xy * max(options.w, 0.25)) +
                  noise((uv - offset) * resolution.xy * max(options.w, 0.25))) * w;
        weight += 2.0 * w;
    }

    float lic = accum / max(weight, 0.0001);
    float contrast = pow(clamp(lic, 0.0, 1.0), 1.35);
    float motion = smoothstep(0.001, 0.14, speed * options.x);
    vec3 tint = mix(vec3(0.12, 0.18, 0.22), vec3(0.18, 0.84, 1.0), motion);
    vec3 rgb = tint * (contrast * 1.35) + vec3(motion * 0.04);
    frag_color = vec4(clamp(rgb * color.rgb, 0.0, 1.0), color.a);
}
@end

@program tcx_flow_visualize_scalar visualization_vs fs_visualize_scalar
@program tcx_flow_visualize_density visualization_vs fs_visualize_density
@program tcx_flow_visualize_velocity_color visualization_vs fs_visualize_velocity_color
@program tcx_flow_visualize_pressure visualization_vs fs_visualize_pressure
@program tcx_flow_visualize_temperature visualization_vs fs_visualize_temperature
@program tcx_flow_visualize_combined visualization_vs fs_visualize_combined
@program tcx_flow_visualize_lic visualization_vs fs_visualize_lic
