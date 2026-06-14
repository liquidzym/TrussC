// =============================================================================
// tcxFlowTools common fullscreen passes
// =============================================================================

@vs vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_copy
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(sourceTex, sourceSmp), uv) * color;
}
@end

@fs fs_clear
layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@fs fs_multiply
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(sourceTex, sourceSmp), uv) * color * options.x;
}
@end

@fs fs_threshold
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float luminance(vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec4 src = texture(sampler2D(sourceTex, sourceSmp), uv) * color;
    float mask = step(options.y, luminance(src.rgb));
    frag_color = src * mask;
}
@end

@fs fs_luminance
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(sourceTex, sourceSmp), uv) * color;
    float y = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    frag_color = vec4(y, y, y, src.a);
}
@end

@fs fs_difference
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;
layout(binding=1) uniform texture2D compareTex;
layout(binding=1) uniform sampler compareSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 a = texture(sampler2D(sourceTex, sourceSmp), uv);
    vec4 b = texture(sampler2D(compareTex, compareSmp), uv);
    frag_color = abs(a - b) * options.x;
}
@end

@fs fs_blur_horizontal
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 d = vec2(texel.x, 0.0) * max(options.z, 1.0);
    vec4 sum = texture(sampler2D(sourceTex, sourceSmp), uv) * 0.227027;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv + d * 1.384615) * 0.316216;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv - d * 1.384615) * 0.316216;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv + d * 3.230769) * 0.070270;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv - d * 3.230769) * 0.070270;
    frag_color = sum;
}
@end

@fs fs_blur_vertical
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 d = vec2(0.0, texel.y) * max(options.z, 1.0);
    vec4 sum = texture(sampler2D(sourceTex, sourceSmp), uv) * 0.227027;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv + d * 1.384615) * 0.316216;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv - d * 1.384615) * 0.316216;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv + d * 3.230769) * 0.070270;
    sum += texture(sampler2D(sourceTex, sourceSmp), uv - d * 3.230769) * 0.070270;
    frag_color = sum;
}
@end

@fs fs_bloom_prefilter
layout(binding=0) uniform texture2D sourceTex;
layout(binding=0) uniform sampler sourceSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec3 c = max(texture(sampler2D(sourceTex, sourceSmp), uv).rgb * color.rgb, vec3(0.0));
    float threshold = max(options.x, 0.0);
    float knee = max(threshold * max(options.y, 0.0), 0.0001);
    float curve0 = threshold - knee;
    float curve1 = knee * 2.0;
    float curve2 = 0.25 / knee;
    float br = max(c.r, max(c.g, c.b));
    float rq = clamp(br - curve0, 0.0, curve1);
    rq = curve2 * rq * rq;
    float weight = max(rq, br - threshold) / max(br, 0.0001);
    frag_color = vec4(c * weight * max(options.z, 0.0), 1.0);
}
@end

@fs fs_bloom_composite
layout(binding=0) uniform texture2D baseTex;
layout(binding=0) uniform sampler baseSmp;
layout(binding=1) uniform texture2D bloomTex;
layout(binding=1) uniform sampler bloomSmp;

layout(binding=0) uniform common_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 base = texture(sampler2D(baseTex, baseSmp), uv) * color;
    vec3 bloom = max(texture(sampler2D(bloomTex, bloomSmp), uv).rgb, vec3(0.0));
    float luma = dot(bloom, vec3(0.2126, 0.7152, 0.0722));
    bloom = mix(vec3(luma), bloom, max(options.z, 0.0));
    vec3 rgb = base.rgb * max(options.x, 0.0) + bloom * max(options.y, 0.0);
    float exposure = max(options.w, 0.0);
    if (exposure > 0.0) {
        rgb = vec3(1.0) - exp(-max(rgb, vec3(0.0)) * exposure);
    }
    frag_color = vec4(clamp(rgb, 0.0, 1.0), clamp(max(base.a, luma * options.y), 0.0, 1.0));
}
@end

@program tcx_flow_copy vs fs_copy
@program tcx_flow_clear vs fs_clear
@program tcx_flow_multiply vs fs_multiply
@program tcx_flow_threshold vs fs_threshold
@program tcx_flow_luminance vs fs_luminance
@program tcx_flow_difference vs fs_difference
@program tcx_flow_blur_horizontal vs fs_blur_horizontal
@program tcx_flow_blur_vertical vs fs_blur_vertical
@program tcx_flow_bloom_prefilter vs fs_bloom_prefilter
@program tcx_flow_bloom_composite vs fs_bloom_composite
