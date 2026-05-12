// =============================================================================
// tcxFlowTools particle helper passes
// =============================================================================

@vs particles_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@vs particle_points_vs
in vec2 lookup;
in vec2 corner;

layout(binding=0) uniform texture2D particlePointStateTex;
layout(binding=0) uniform sampler particlePointStateSmp;

layout(binding=0) uniform particle_point_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

out vec4 v_color;

void main() {
    vec4 state = texture(sampler2D(particlePointStateTex, particlePointStateSmp), lookup);
    vec2 pixel = texel.xy + state.xy * resolution.zw;
    pixel += corner * options.w;
    vec2 ndc = pixel / max(resolution.xy, vec2(1.0)) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float fade = 1.0 - clamp(state.z / max(options.z, 0.0001), 0.0, 1.0);
    v_color = vec4(color.rgb, color.a * fade);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
@end

@fs fs_particle_points
in vec4 v_color;
out vec4 frag_color;

void main() {
    frag_color = v_color;
}
@end

@fs fs_particles_spawn
layout(binding=0) uniform texture2D particleSeedTex;
layout(binding=0) uniform sampler particleSeedSmp;

layout(binding=0) uniform particle_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec4 seed = texture(sampler2D(particleSeedTex, particleSeedSmp), uv);
    float a = hash(uv + options.xy);
    float b = hash(uv.yx + options.zw);
    frag_color = vec4(fract(a + seed.x), fract(b + seed.y), 0.0, 1.0) * color;
}
@end

@fs fs_particles_update
layout(binding=0) uniform texture2D particleStateTex;
layout(binding=0) uniform sampler particleStateSmp;
layout(binding=1) uniform texture2D particleVelocityTex;
layout(binding=1) uniform sampler particleVelocitySmp;

layout(binding=0) uniform particle_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 state = texture(sampler2D(particleStateTex, particleStateSmp), uv);
    vec2 pos = state.xy;
    vec2 flow = texture(sampler2D(particleVelocityTex, particleVelocitySmp), pos).xy;
    pos += flow * options.x;
    pos = fract(pos);
    float age = state.z + options.y;
    if (age > options.z) {
        pos = uv;
        age = 0.0;
    }
    frag_color = vec4(pos, age, 1.0) * color;
}
@end

@fs fs_particles_render
layout(binding=0) uniform texture2D particleStateTex;
layout(binding=0) uniform sampler particleStateSmp;

layout(binding=0) uniform particle_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 state = texture(sampler2D(particleStateTex, particleStateSmp), uv);
    float fade = 1.0 - clamp(state.z / max(options.z, 0.0001), 0.0, 1.0);
    frag_color = vec4(color.rgb, color.a * fade);
}
@end

@program tcx_flow_particles_spawn particles_vs fs_particles_spawn
@program tcx_flow_particles_update particles_vs fs_particles_update
@program tcx_flow_particles_render particles_vs fs_particles_render
@program tcx_flow_particles_points particle_points_vs fs_particle_points
