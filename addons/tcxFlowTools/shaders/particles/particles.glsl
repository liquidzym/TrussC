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

float particle_point_hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float particle_mass_scale(float mass, float sizeSpread, vec2 seed) {
    float spread = max(sizeSpread, 0.0);
    float sizeJitter = 1.0 + (particle_point_hash(seed) * 2.0 - 1.0) * spread;
    return sqrt(max(mass, 0.05)) * max(sizeJitter, 0.05);
}

float particle_lifespan_scale(float mass) {
    return clamp(max(mass, 0.05), 0.25, 4.0);
}

vec4 particle_age_lifespan_mass_size(vec4 state, float lifetime, float baseSize, float sizeSpread, vec2 seed) {
    float mass = max(state.w, 0.05);
    float lifespan = max(lifetime * particle_lifespan_scale(mass), 0.0001);
    float size = max(baseSize, 0.05) * particle_mass_scale(mass, sizeSpread, seed);
    return vec4(state.z, lifespan, mass, size);
}

void main() {
    vec4 state = texture(sampler2D(particlePointStateTex, particlePointStateSmp), lookup);
    vec4 ageLifespanMassSize = particle_age_lifespan_mass_size(state, options.z, options.w, options.x, lookup);
    vec2 pixel = texel.xy + state.xy * resolution.zw;
    pixel += corner * ageLifespanMassSize.w;
    vec2 ndc = pixel / max(resolution.xy, vec2(1.0)) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    float fade = pow(1.0 - clamp(ageLifespanMassSize.x / ageLifespanMassSize.y, 0.0, 1.0), max(options.y, 0.0001));
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

vec2 particle_spawn_seed(vec2 p) {
    float a = hash(p + options.xy);
    float b = hash(p.yx + options.zw + vec2(a, 1.0 - a));
    return vec2(a, b);
}

float particle_seed_mass(vec2 seed) {
    float spread = max(texel.z, 0.0);
    float mass = max(color.a, 0.05);
    return max(0.05, mass * (1.0 + (hash(seed) * 2.0 - 1.0) * spread));
}

void main() {
    vec2 seed = particle_spawn_seed(uv);
    vec2 jitter = particle_spawn_seed(seed + uv * 17.0);
    vec2 pos = fract(seed + jitter * 0.6180339887);
    frag_color = vec4(pos, 0.0, particle_seed_mass(uv + seed));
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

float update_hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float particle_mass_scale(float mass) {
    return max(mass, 0.05);
}

float particle_lifespan_scale(float mass) {
    return clamp(max(mass, 0.05), 0.25, 4.0);
}

vec2 particle_birth_velocity(vec2 pos) {
    if (color.w <= 0.0) return vec2(0.0);
    return texture(sampler2D(particleVelocityTex, particleVelocitySmp), pos).xy * options.x;
}

void main() {
    vec4 state = texture(sampler2D(particleStateTex, particleStateSmp), uv);
    vec2 pos = state.xy;
    float mass = particle_mass_scale(state.w);
    vec2 flow = texture(sampler2D(particleVelocityTex, particleVelocitySmp), pos).xy;
    pos += flow * options.x / mass;
    if (options.w > 0.5 && options.w < 1.5) {
        vec2 toward = color.xy - pos;
        float dist = max(length(toward), 0.0005);
        pos += toward / dist * color.z * options.y / mass;
    } else if (options.w > 1.5) {
        vec2 away = pos - color.xy;
        float dist = max(length(away), 0.0005);
        pos += away / dist * color.z * options.y / mass;
    }
    pos = fract(pos);
    float age = state.z + options.y;
    float lifespan = options.z * particle_lifespan_scale(mass);
    if (age > lifespan) {
        pos = fract(uv + particle_birth_velocity(uv));
        age = 0.0;
    }
    float spawn_choice = update_hash(uv + vec2(state.z, options.y));
    if (texel.w > 0.0 && spawn_choice < texel.w) {
        float angle = update_hash(uv + vec2(13.7, 41.3)) * 6.2831853;
        float radius = sqrt(update_hash(uv.yx + vec2(5.1, 19.9))) * texel.z;
        pos = clamp(color.xy + vec2(cos(angle), sin(angle)) * radius, vec2(0.0), vec2(1.0));
        pos = clamp(pos + particle_birth_velocity(pos), vec2(0.0), vec2(1.0));
        age = 0.0;
    }
    frag_color = vec4(pos, age, mass);
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

float particle_lifespan_scale(float mass) {
    return clamp(max(mass, 0.05), 0.25, 4.0);
}

void main() {
    vec4 state = texture(sampler2D(particleStateTex, particleStateSmp), uv);
    float fade = 1.0 - clamp(state.z / max(options.z * particle_lifespan_scale(state.w), 0.0001), 0.0, 1.0);
    frag_color = vec4(color.rgb, color.a * fade);
}
@end

@program tcx_flow_particles_spawn particles_vs fs_particles_spawn
@program tcx_flow_particles_update particles_vs fs_particles_update
@program tcx_flow_particles_render particles_vs fs_particles_render
@program tcx_flow_particles_points particle_points_vs fs_particle_points
