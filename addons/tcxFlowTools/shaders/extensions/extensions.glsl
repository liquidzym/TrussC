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

@fs fs_split_velocity_raw
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 velocity = texture(sampler2D(extensionTex0, extensionSmp0), uv).xy * options.x;
    vec2 positiveVelocity = max(velocity, vec2(0.0));
    vec2 negativeVelocity = abs(min(velocity, vec2(0.0)));
    frag_color = vec4(positiveVelocity, negativeVelocity);
}
@end

@fs fs_normalize_vector
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 value = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    float minMagnitude = max(options.x, 0.0);
    float range = max(options.y, 0.0001);
    float magnitude = max(length(value) - minMagnitude, 0.0);
    vec4 direction = normalize(value + vec4(0.000001));
    frag_color = direction * (magnitude / range) * color;
}
@end

@fs fs_decay
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;
layout(binding=1) uniform texture2D extensionTex1;
layout(binding=1) uniform sampler extensionSmp1;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 previous = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    vec4 source = texture(sampler2D(extensionTex1, extensionSmp1), uv);
    float decay = clamp(options.x, 0.0, 1.0);
    float sourceGain = max(options.y, 0.0);
    frag_color = previous * (1.0 - decay) + source * sourceGain * color;
}
@end

@fs fs_split_velocity_visual
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;
layout(binding=1) uniform texture2D extensionTex1;
layout(binding=1) uniform sampler extensionSmp1;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 rawSplit = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    vec4 trailSplit = texture(sampler2D(extensionTex1, extensionSmp1), uv);
    float trailBlend = clamp(options.z, 0.0, 1.0);
    vec4 split = mix(rawSplit, trailSplit, trailBlend) * max(options.x, 0.0);
    float energy = clamp(split.x + split.y + split.z + split.w, 0.0, 1.0);

    vec3 combined = vec3(
        split.x + split.z * 0.25,
        split.y + split.w * 0.35,
        split.z + split.w * 0.75
    );
    vec3 positive = vec3(split.x, split.y, energy * 0.08);
    vec3 negative = vec3(split.z * 0.45, split.w * 0.25, split.z + split.w);
    vec3 trail = vec3(energy, max(split.x, split.z), max(split.y, split.w));
    vec3 rgb = combined;
    if (options.y > 0.5 && options.y < 1.5) {
        rgb = positive;
    } else if (options.y > 1.5 && options.y < 2.5) {
        rgb = negative;
    } else if (options.y > 2.5) {
        rgb = trail;
    }
    frag_color = vec4(clamp(rgb * color.rgb, 0.0, 1.0), color.a);
}
@end

@fs fs_colorize_luminance
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float helper_luma(vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec4 src = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    float luma = clamp((helper_luma(src.rgb) - options.y) * max(options.x, 0.0), 0.0, 1.0);
    vec3 cold = vec3(0.06, 0.16, 0.48);
    vec3 mid = vec3(0.12, 0.72, 0.82);
    vec3 hot = vec3(1.0, 0.82, 0.18);
    vec3 ramp = mix(cold, mid, smoothstep(0.0, 0.55, luma));
    ramp = mix(ramp, hot, smoothstep(0.45, 1.0, luma));
    frag_color = vec4(ramp * color.rgb, src.a * color.a);
}
@end

@fs fs_colorize_velocity
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 velocity = texture(sampler2D(extensionTex0, extensionSmp0), uv).xy * options.x;
    float speed = clamp(length(velocity), 0.0, 1.0);
    vec2 dir = normalize(velocity + vec2(0.000001));
    vec3 hue = vec3(dir.x * 0.5 + 0.5, dir.y * 0.5 + 0.5, 1.0 - abs(dir.x * dir.y));
    vec3 rgb = mix(vec3(0.02), hue, pow(speed, 0.35));
    frag_color = vec4(clamp(rgb * color.rgb, 0.0, 1.0), color.a * speed);
}
@end

@fs fs_colorize_gradient
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;
layout(binding=1) uniform texture2D extensionTex1;
layout(binding=1) uniform sampler extensionSmp1;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    float scalar = clamp(length(src.rgb) * max(options.x, 0.0) + options.y, 0.0, 1.0);
    vec4 ramp = texture(sampler2D(extensionTex1, extensionSmp1), vec2(scalar, 0.5));
    frag_color = ramp * color;
}
@end

@fs fs_dilate
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float helper_weight(vec4 value) {
    return dot(value.rgb, vec3(0.2126, 0.7152, 0.0722)) + value.a * 0.001;
}

void main() {
    vec2 radius = texel.xy * max(options.z, 1.0);
    vec4 best = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    float bestWeight = helper_weight(best);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec4 candidate = texture(sampler2D(extensionTex0, extensionSmp0), uv + vec2(float(x), float(y)) * radius);
            float weight = helper_weight(candidate);
            if (weight > bestWeight) {
                best = candidate;
                bestWeight = weight;
            }
        }
    }
    frag_color = best * color;
}
@end

@fs fs_erode
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float helper_erode_weight(vec4 value) {
    return dot(value.rgb, vec3(0.2126, 0.7152, 0.0722)) + value.a * 0.001;
}

void main() {
    vec2 radius = texel.xy * max(options.z, 1.0);
    vec4 best = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    float bestWeight = helper_erode_weight(best);
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec4 candidate = texture(sampler2D(extensionTex0, extensionSmp0), uv + vec2(float(x), float(y)) * radius);
            float weight = helper_erode_weight(candidate);
            if (weight < bestWeight) {
                best = candidate;
                bestWeight = weight;
            }
        }
    }
    frag_color = best * color;
}
@end

@fs fs_inverse_warp
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;
layout(binding=1) uniform texture2D extensionTex1;
layout(binding=1) uniform sampler extensionSmp1;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 velocity = texture(sampler2D(extensionTex1, extensionSmp1), uv).xy;
    vec2 warpedUv = uv - velocity * texel.xy * options.x;
    frag_color = texture(sampler2D(extensionTex0, extensionSmp0), warpedUv) * color;
}
@end

@fs fs_ease
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;
layout(binding=1) uniform texture2D extensionTex1;
layout(binding=1) uniform sampler extensionSmp1;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 previous = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    vec4 current = texture(sampler2D(extensionTex1, extensionSmp1), uv);
    float amount = clamp(options.x, 0.0, 1.0);
    float curved = amount * amount * (3.0 - 2.0 * amount);
    frag_color = mix(previous, current, curved) * color;
}
@end

@fs fs_time_blur
layout(binding=0) uniform texture2D extensionTex0;
layout(binding=0) uniform sampler extensionSmp0;
layout(binding=1) uniform texture2D extensionTex1;
layout(binding=1) uniform sampler extensionSmp1;

layout(binding=0) uniform extension_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 radius = texel.xy * max(options.z, 1.0);
    vec4 previous = texture(sampler2D(extensionTex0, extensionSmp0), uv);
    vec4 source = texture(sampler2D(extensionTex1, extensionSmp1), uv) * 0.227027;
    source += texture(sampler2D(extensionTex1, extensionSmp1), uv + vec2(radius.x, 0.0) * 1.384615) * 0.158108;
    source += texture(sampler2D(extensionTex1, extensionSmp1), uv - vec2(radius.x, 0.0) * 1.384615) * 0.158108;
    source += texture(sampler2D(extensionTex1, extensionSmp1), uv + vec2(0.0, radius.y) * 1.384615) * 0.158108;
    source += texture(sampler2D(extensionTex1, extensionSmp1), uv - vec2(0.0, radius.y) * 1.384615) * 0.158108;
    float decay = clamp(options.x, 0.0, 1.0);
    float sourceGain = max(options.y, 0.0);
    frag_color = previous * (1.0 - decay) + source * sourceGain * color;
}
@end

@program tcx_flow_extensions_split_velocity_raw extensions_vs fs_split_velocity_raw
@program tcx_flow_extensions_normalize_vector extensions_vs fs_normalize_vector
@program tcx_flow_extensions_decay extensions_vs fs_decay
@program tcx_flow_extensions_split_velocity_visual extensions_vs fs_split_velocity_visual
@program tcx_flow_extensions_colorize_luminance extensions_vs fs_colorize_luminance
@program tcx_flow_extensions_colorize_velocity extensions_vs fs_colorize_velocity
@program tcx_flow_extensions_colorize_gradient extensions_vs fs_colorize_gradient
@program tcx_flow_extensions_dilate extensions_vs fs_dilate
@program tcx_flow_extensions_erode extensions_vs fs_erode
@program tcx_flow_extensions_inverse_warp extensions_vs fs_inverse_warp
@program tcx_flow_extensions_ease extensions_vs fs_ease
@program tcx_flow_extensions_time_blur extensions_vs fs_time_blur
