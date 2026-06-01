// =============================================================================
// tcxCloth TexturePingPong shader passes
// =============================================================================

@vs cloth_fullscreen_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_cloth_step
layout(binding=0) uniform texture2D positionTex;
layout(binding=0) uniform sampler positionSmp;
layout(binding=1) uniform texture2D previousTex;
layout(binding=1) uniform sampler previousSmp;
layout(binding=2) uniform texture2D pinTex;
layout(binding=2) uniform sampler pinSmp;

layout(binding=0) uniform cloth_step_params {
    vec4 simSize;      // columns, rows, inv columns, inv rows
    vec4 timing;       // dt, damping, elapsed time, mode
    vec4 wind;         // normalized direction xyz, strength
    vec4 clothLayout;  // origin x/y, width, height
    vec4 forces;       // gravity + global force xyz
    vec4 stiffness;    // structural, shear, bend
    vec4 collider;     // sphere center xyz, radius
    vec4 options;      // enable shear, enable bend, enable wind, max grid step
};

in vec2 uv;
out vec4 frag_color;

vec2 gridCoord() {
    return floor(uv * simSize.xy);
}

bool validCoord(vec2 coord) {
    return coord.x >= 0.0 && coord.y >= 0.0 && coord.x < simSize.x && coord.y < simSize.y;
}

vec2 coordUv(vec2 coord) {
    return (coord + vec2(0.5)) / simSize.xy;
}

vec3 samplePosition(vec2 tc) {
    return texture(sampler2D(positionTex, positionSmp), clamp(tc, vec2(0.0), vec2(1.0))).xyz;
}

vec3 samplePrevious(vec2 tc) {
    return texture(sampler2D(previousTex, previousSmp), clamp(tc, vec2(0.0), vec2(1.0))).xyz;
}

vec4 samplePin(vec2 tc) {
    return texture(sampler2D(pinTex, pinSmp), clamp(tc, vec2(0.0), vec2(1.0)));
}

vec3 restPosition(vec2 tc) {
    vec2 normalized = clamp((tc * simSize.xy - vec2(0.5)) / max(simSize.xy - vec2(1.0), vec2(1.0)),
                            vec2(0.0), vec2(1.0));
    return vec3(clothLayout.x + normalized.x * clothLayout.z,
                clothLayout.y + normalized.y * clothLayout.w,
                0.0);
}

vec3 pinnedOrRest(vec2 tc, vec4 pin) {
    if (pin.r > 0.5) {
        return vec3(pin.g, pin.b, pin.a);
    }
    return restPosition(tc);
}

vec3 clothNormal() {
    vec2 texel = simSize.zw;
    vec3 c = samplePosition(uv);
    vec3 r = samplePosition(uv + vec2(texel.x, 0.0));
    vec3 l = samplePosition(uv - vec2(texel.x, 0.0));
    vec3 u = samplePosition(uv - vec2(0.0, texel.y));
    vec3 d = samplePosition(uv + vec2(0.0, texel.y));
    vec3 n = cross(r - c, u - c) +
             cross(u - c, l - c) +
             cross(l - c, d - c) +
             cross(d - c, r - c);
    float len = length(n);
    if (len < 0.000001) {
        return vec3(0.0, 0.0, 1.0);
    }
    return n / len;
}

void applyNeighbor(inout vec3 correction, vec3 pos, vec2 coord, vec2 offset, float restLen, float k) {
    vec2 ncoord = coord + offset;
    if (!validCoord(ncoord) || k <= 0.0) {
        return;
    }
    vec3 neighbor = samplePosition(coordUv(ncoord));
    vec3 delta = neighbor - pos;
    float len = length(delta);
    if (len <= 0.000001) {
        return;
    }
    correction += delta * ((len - restLen) / len) * (0.5 * k);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    int mode = int(timing.w + 0.5);
    vec4 pin = samplePin(uv);
    if (mode == 0) {
        frag_color = vec4(pinnedOrRest(uv, pin), pin.r);
        return;
    }

    vec2 texel = simSize.zw;
    vec3 pos = samplePosition(uv);
    if (pin.r > 0.5) {
        frag_color = vec4(vec3(pin.g, pin.b, pin.a), pin.r);
        return;
    }

    if (mode == 4) {
        frag_color = vec4(pos, pin.r);
        return;
    }

    if (mode == 1) {
        vec3 prev = samplePrevious(uv);
        vec3 accel = forces.xyz;
        if (options.z > 0.5 && wind.w > 0.0) {
            vec3 n = clothNormal();
            float projection = dot(n, normalize(wind.xyz));
            float freeEdge = smoothstep(0.05, 0.88, uv.y);
            float flutter = sin(timing.z * (1.8 + wind.w * 0.04) + uv.x * 10.0 + uv.y * 6.0) * 0.35;
            accel += n * projection * wind.w * (85.0 + flutter * 35.0) * freeEdge;
        }
        vec3 velocity = (pos - prev) * (1.0 - timing.y);
        vec3 next = pos + velocity + accel * timing.x * timing.x;
        frag_color = vec4(next, pin.r);
        return;
    }

    if (mode == 2) {
        vec2 coord = gridCoord();
        float dx = clothLayout.z / max(simSize.x - 1.0, 1.0);
        float dy = clothLayout.w / max(simSize.y - 1.0, 1.0);
        float diag = length(vec2(dx, dy));
        vec3 correction = vec3(0.0);

        applyNeighbor(correction, pos, coord, vec2(-1.0, 0.0), dx, stiffness.x);
        applyNeighbor(correction, pos, coord, vec2(1.0, 0.0), dx, stiffness.x);
        applyNeighbor(correction, pos, coord, vec2(0.0, -1.0), dy, stiffness.x);
        applyNeighbor(correction, pos, coord, vec2(0.0, 1.0), dy, stiffness.x);

        if (options.x > 0.5) {
            applyNeighbor(correction, pos, coord, vec2(-1.0, -1.0), diag, stiffness.y);
            applyNeighbor(correction, pos, coord, vec2(1.0, -1.0), diag, stiffness.y);
            applyNeighbor(correction, pos, coord, vec2(-1.0, 1.0), diag, stiffness.y);
            applyNeighbor(correction, pos, coord, vec2(1.0, 1.0), diag, stiffness.y);
        }

        if (options.y > 0.5) {
            applyNeighbor(correction, pos, coord, vec2(-2.0, 0.0), dx * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(2.0, 0.0), dx * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(0.0, -2.0), dy * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(0.0, 2.0), dy * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(-2.0, -2.0), diag * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(2.0, -2.0), diag * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(-2.0, 2.0), diag * 2.0, stiffness.z);
            applyNeighbor(correction, pos, coord, vec2(2.0, 2.0), diag * 2.0, stiffness.z);
        }

        vec3 corrected = pos + correction * options.w;
        frag_color = vec4(corrected, pin.r);
        return;
    }

    if (mode == 3 && collider.w > 0.0) {
        vec3 delta = pos - collider.xyz;
        float dist = length(delta);
        if (dist < collider.w) {
            vec3 dir = dist > 0.000001 ? delta / dist : vec3(0.0, 1.0, 0.0);
            pos = collider.xyz + dir * collider.w;
        }
        frag_color = vec4(pos, pin.r);
        return;
    }

    frag_color = vec4(pos, pin.r);
}
@end

@program tcx_cloth_step cloth_fullscreen_vs fs_cloth_step
