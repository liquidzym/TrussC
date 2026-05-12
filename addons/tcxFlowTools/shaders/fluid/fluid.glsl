// =============================================================================
// tcxFlowTools fluid fullscreen passes
// =============================================================================
// These passes follow the same stable-fluid graph used by ofxFlowTools and
// PixelFlow: advect, splat, divergence, Jacobi pressure solve, gradient
// subtraction, vorticity, and buoyancy.

@vs fluid_vs
in vec2 position;
in vec2 texcoord0;

out vec2 uv;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    uv = texcoord0;
}
@end

@fs fs_advect
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;
layout(binding=1) uniform texture2D fluidSourceTex;
layout(binding=1) uniform sampler fluidSourceSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 velocity = texture(sampler2D(velocityTex, velocitySmp), uv).xy;
    vec2 previousUv = uv - velocity * texel.xy * options.x;
    vec4 value = texture(sampler2D(fluidSourceTex, fluidSourceSmp), previousUv) * options.z;
    frag_color = options.w > 0.5 ? clamp(value, 0.0, 1.0) : value;
}
@end

@fs fs_splat
layout(binding=1) uniform texture2D fluidSourceTex;
layout(binding=1) uniform sampler fluidSourceSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 base = texture(sampler2D(fluidSourceTex, fluidSourceSmp), uv);
    vec2 p = uv * resolution.xy;
    vec2 center = texel.zw;
    float radius = max(options.z, 1.0);
    float d = length(p - center) / radius;
    float distNorm = max(0.0, 1.0 - d);
    vec4 value = base;
    if (options.y < 0.5) {
        value = base + color * distNorm * options.x;
    } else if (options.y < 1.5) {
        float falloff = sqrt(sqrt(distNorm));
        value = max(base, color * falloff * options.x);
    } else {
        vec2 oldVelocity = base.xy;
        vec2 newVelocity = color.xy * distNorm * options.x;
        vec2 mixedVelocity = length(oldVelocity) > length(newVelocity)
            ? oldVelocity
            : mix(oldVelocity, newVelocity, clamp(color.b, 0.0, 1.0));
        value = vec4(mixedVelocity, 0.0, 1.0);
    }
    frag_color = options.w > 0.5 ? clamp(value, 0.0, 1.0) : value;
}
@end

@fs fs_diffuse
layout(binding=0) uniform texture2D diffuseSourceTex;
layout(binding=0) uniform sampler diffuseSourceSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    vec4 c = texture(sampler2D(diffuseSourceTex, diffuseSourceSmp), uv);
    vec4 l = texture(sampler2D(diffuseSourceTex, diffuseSourceSmp), uv - vec2(px.x, 0.0));
    vec4 r = texture(sampler2D(diffuseSourceTex, diffuseSourceSmp), uv + vec2(px.x, 0.0));
    vec4 u = texture(sampler2D(diffuseSourceTex, diffuseSourceSmp), uv - vec2(0.0, px.y));
    vec4 d = texture(sampler2D(diffuseSourceTex, diffuseSourceSmp), uv + vec2(0.0, px.y));
    vec4 avg = (l + r + u + d) * 0.25;
    vec4 value = mix(c, avg, clamp(options.x, 0.0, 1.0));
    frag_color = options.w > 0.5 ? clamp(value, 0.0, 1.0) : value;
}
@end

@fs fs_add_velocity
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;
layout(binding=1) uniform texture2D externalVelocityTex;
layout(binding=1) uniform sampler externalVelocitySmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 velocity = texture(sampler2D(velocityTex, velocitySmp), uv).xy;
    vec2 externalVelocity = texture(sampler2D(externalVelocityTex, externalVelocitySmp), uv).xy;
    if (options.y > 0.5) {
        float hasExternal = step(0.000001, dot(externalVelocity, externalVelocity));
        velocity = mix(velocity, externalVelocity, hasExternal * clamp(options.x, 0.0, 1.0));
    } else {
        velocity += externalVelocity * options.x;
    }
    frag_color = vec4(velocity, 0.0, 1.0);
}
@end

@fs fs_add_density_texture
layout(binding=0) uniform texture2D densityAddBaseTex;
layout(binding=0) uniform sampler densityAddBaseSmp;
layout(binding=1) uniform texture2D externalDensityTex;
layout(binding=1) uniform sampler externalDensitySmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 density = texture(sampler2D(densityAddBaseTex, densityAddBaseSmp), uv);
    vec4 externalDensity = texture(sampler2D(externalDensityTex, externalDensitySmp), uv);
    float energy = options.w > 0.5
        ? length(externalDensity.xy)
        : externalDensity.a;
    float mask = smoothstep(options.y, options.z, energy);
    vec3 injectedColor = options.w > 0.5 ? color.rgb : externalDensity.rgb * color.rgb;
    vec4 injected = vec4(injectedColor, energy);
    if (options.w > 0.5) {
        frag_color = clamp(density + injected * (options.x * mask), 0.0, 1.0);
    } else {
        vec4 added = clamp(injected * (options.x * mask), 0.0, 1.0);
        frag_color = max(density, added);
    }
}
@end

@fs fs_add_temperature_texture
layout(binding=0) uniform texture2D temperatureAddBaseTex;
layout(binding=0) uniform sampler temperatureAddBaseSmp;
layout(binding=1) uniform texture2D externalTemperatureTex;
layout(binding=1) uniform sampler externalTemperatureSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 temperature = texture(sampler2D(temperatureAddBaseTex, temperatureAddBaseSmp), uv);
    vec4 externalTemperature = texture(sampler2D(externalTemperatureTex, externalTemperatureSmp), uv);
    float energy = options.w > 0.5
        ? length(externalTemperature.xy)
        : dot(externalTemperature.rgb, vec3(0.2126, 0.7152, 0.0722));
    float mask = smoothstep(options.y, options.z, energy);
    float injected = energy * options.x * mask;
    frag_color = clamp(temperature + vec4(injected, injected, injected, 1.0), 0.0, 1.0);
}
@end

@fs fs_divergence
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;
layout(binding=1) uniform texture2D obstacleTex;
layout(binding=1) uniform sampler obstacleSmp;
layout(binding=2) uniform texture2D obstacleOffsetTex;
layout(binding=2) uniform sampler obstacleOffsetSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    float oC = texture(sampler2D(obstacleTex, obstacleSmp), uv).r;
    if (oC >= 0.5) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec2 vL = texture(sampler2D(velocityTex, velocitySmp), uv - vec2(px.x, 0.0)).xy;
    vec2 vR = texture(sampler2D(velocityTex, velocitySmp), uv + vec2(px.x, 0.0)).xy;
    vec2 vU = texture(sampler2D(velocityTex, velocitySmp), uv - vec2(0.0, px.y)).xy;
    vec2 vD = texture(sampler2D(velocityTex, velocitySmp), uv + vec2(0.0, px.y)).xy;
    vec2 vC = texture(sampler2D(velocityTex, velocitySmp), uv).xy;
    vec4 oN = texture(sampler2D(obstacleOffsetTex, obstacleOffsetSmp), uv);
    vU = mix(vU, -vC, oN.x);
    vD = mix(vD, -vC, oN.y);
    vR = mix(vR, -vC, oN.z);
    vL = mix(vL, -vC, oN.w);
    float div = options.w * ((vR.x - vL.x) + (vD.y - vU.y));
    frag_color = vec4(div, div, div, 1.0);
}
@end

@fs fs_jacobi_pressure
layout(binding=0) uniform texture2D pressureSrcTex;
layout(binding=0) uniform sampler pressureSrcSmp;
layout(binding=1) uniform texture2D divergenceTex;
layout(binding=1) uniform sampler divergenceSmp;
layout(binding=2) uniform texture2D pressureObstacleTex;
layout(binding=2) uniform sampler pressureObstacleSmp;
layout(binding=3) uniform texture2D pressureObstacleOffsetTex;
layout(binding=3) uniform sampler pressureObstacleOffsetSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    float oC = texture(sampler2D(pressureObstacleTex, pressureObstacleSmp), uv).r;
    if (oC >= 0.5) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float pL = texture(sampler2D(pressureSrcTex, pressureSrcSmp), uv - vec2(px.x, 0.0)).x;
    float pR = texture(sampler2D(pressureSrcTex, pressureSrcSmp), uv + vec2(px.x, 0.0)).x;
    float pU = texture(sampler2D(pressureSrcTex, pressureSrcSmp), uv - vec2(0.0, px.y)).x;
    float pD = texture(sampler2D(pressureSrcTex, pressureSrcSmp), uv + vec2(0.0, px.y)).x;
    float pC = texture(sampler2D(pressureSrcTex, pressureSrcSmp), uv).x;
    vec4 oN = texture(sampler2D(pressureObstacleOffsetTex, pressureObstacleOffsetSmp), uv);
    pU = mix(pU, pC, oN.x);
    pD = mix(pD, pC, oN.y);
    pR = mix(pR, pC, oN.z);
    pL = mix(pL, pC, oN.w);
    float div = texture(sampler2D(divergenceTex, divergenceSmp), uv).x;
    float pressure = (pL + pR + pU + pD - div) * 0.25;
    frag_color = vec4(pressure, pressure, pressure, 1.0);
}
@end

@fs fs_gradient_subtract
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;
layout(binding=1) uniform texture2D pressureTex;
layout(binding=1) uniform sampler pressureSmp;
layout(binding=2) uniform texture2D gradientObstacleTex;
layout(binding=2) uniform sampler gradientObstacleSmp;
layout(binding=3) uniform texture2D gradientObstacleOffsetTex;
layout(binding=3) uniform sampler gradientObstacleOffsetSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    float oC = texture(sampler2D(gradientObstacleTex, gradientObstacleSmp), uv).r;
    if (oC >= 0.5) {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float pL = texture(sampler2D(pressureTex, pressureSmp), uv - vec2(px.x, 0.0)).x;
    float pR = texture(sampler2D(pressureTex, pressureSmp), uv + vec2(px.x, 0.0)).x;
    float pU = texture(sampler2D(pressureTex, pressureSmp), uv - vec2(0.0, px.y)).x;
    float pD = texture(sampler2D(pressureTex, pressureSmp), uv + vec2(0.0, px.y)).x;
    float pC = texture(sampler2D(pressureTex, pressureSmp), uv).x;
    vec4 oN = texture(sampler2D(gradientObstacleOffsetTex, gradientObstacleOffsetSmp), uv);
    pU = mix(pU, pC, oN.x);
    pD = mix(pD, pC, oN.y);
    pR = mix(pR, pC, oN.z);
    pL = mix(pL, pC, oN.w);
    vec2 velocity = texture(sampler2D(velocityTex, velocitySmp), uv).xy;
    velocity -= options.w * vec2(pR - pL, pD - pU);
    frag_color = vec4(velocity, 0.0, 1.0);
}
@end

@fs fs_vorticity_curl
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    float vL = texture(sampler2D(velocityTex, velocitySmp), uv - vec2(px.x, 0.0)).y;
    float vR = texture(sampler2D(velocityTex, velocitySmp), uv + vec2(px.x, 0.0)).y;
    float vU = texture(sampler2D(velocityTex, velocitySmp), uv - vec2(0.0, px.y)).x;
    float vD = texture(sampler2D(velocityTex, velocitySmp), uv + vec2(0.0, px.y)).x;
    float curl = options.w * ((vD - vU) - (vR - vL));
    frag_color = vec4(curl, curl, curl, 1.0);
}
@end

@fs fs_vorticity_force
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;
layout(binding=1) uniform texture2D curlTex;
layout(binding=1) uniform sampler curlSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    float cL = abs(texture(sampler2D(curlTex, curlSmp), uv - vec2(px.x, 0.0)).x);
    float cR = abs(texture(sampler2D(curlTex, curlSmp), uv + vec2(px.x, 0.0)).x);
    float cU = abs(texture(sampler2D(curlTex, curlSmp), uv - vec2(0.0, px.y)).x);
    float cD = abs(texture(sampler2D(curlTex, curlSmp), uv + vec2(0.0, px.y)).x);
    float cC = texture(sampler2D(curlTex, curlSmp), uv).x;
    vec2 direction = normalize(vec2(cD - cU, cR - cL) + vec2(0.000001)) * vec2(-1.0, 1.0);
    vec2 velocity = texture(sampler2D(velocityTex, velocitySmp), uv).xy;
    velocity += direction * cC * options.x * options.y;
    frag_color = vec4(velocity, 0.0, 1.0);
}
@end

@fs fs_buoyancy
layout(binding=0) uniform texture2D velocityTex;
layout(binding=0) uniform sampler velocitySmp;
layout(binding=1) uniform texture2D temperatureTex;
layout(binding=1) uniform sampler temperatureSmp;
layout(binding=2) uniform texture2D densityTex;
layout(binding=2) uniform sampler densitySmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 velocity = texture(sampler2D(velocityTex, velocitySmp), uv).xy;
    float temperature = texture(sampler2D(temperatureTex, temperatureSmp), uv).x;
    float density = texture(sampler2D(densityTex, densitySmp), uv).a;
    float lift = temperature * options.y - density * options.z;
    velocity.y -= lift * options.x;
    frag_color = vec4(velocity, 0.0, 1.0);
}
@end

@fs fs_obstacle_splat
layout(binding=1) uniform texture2D fluidSourceTex;
layout(binding=1) uniform sampler fluidSourceSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 base = texture(sampler2D(fluidSourceTex, fluidSourceSmp), uv);
    vec2 p = uv * resolution.xy;
    float d = length(p - texel.zw) / max(options.z, 1.0);
    float mask = smoothstep(1.0, 0.0, d);
    float value = max(base.r, mask * options.x);
    frag_color = vec4(value, value, value, value);
}
@end

@fs fs_obstacle_offset
layout(binding=0) uniform texture2D obstacleOffsetSourceTex;
layout(binding=0) uniform sampler obstacleOffsetSourceSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec2 px = texel.xy;
    float oU = texture(sampler2D(obstacleOffsetSourceTex, obstacleOffsetSourceSmp), uv - vec2(0.0, px.y)).r;
    float oD = texture(sampler2D(obstacleOffsetSourceTex, obstacleOffsetSourceSmp), uv + vec2(0.0, px.y)).r;
    float oR = texture(sampler2D(obstacleOffsetSourceTex, obstacleOffsetSourceSmp), uv + vec2(px.x, 0.0)).r;
    float oL = texture(sampler2D(obstacleOffsetSourceTex, obstacleOffsetSourceSmp), uv - vec2(px.x, 0.0)).r;
    frag_color = step(vec4(options.y), vec4(oU, oD, oR, oL));
}
@end

@fs fs_apply_obstacle
layout(binding=0) uniform texture2D obstacleApplySourceTex;
layout(binding=0) uniform sampler obstacleApplySourceSmp;
layout(binding=1) uniform texture2D obstacleTex;
layout(binding=1) uniform sampler obstacleSmp;

layout(binding=0) uniform flow_params {
    vec4 color;
    vec4 resolution;
    vec4 texel;
    vec4 options;
};

in vec2 uv;
out vec4 frag_color;

void main() {
    vec4 value = texture(sampler2D(obstacleApplySourceTex, obstacleApplySourceSmp), uv);
    float obstacle = texture(sampler2D(obstacleTex, obstacleSmp), uv).r;
    float keep = 1.0 - step(options.y, obstacle);
    frag_color = value * keep;
}
@end

@program tcx_flow_fluid_advect fluid_vs fs_advect
@program tcx_flow_fluid_splat fluid_vs fs_splat
@program tcx_flow_fluid_diffuse fluid_vs fs_diffuse
@program tcx_flow_fluid_add_velocity fluid_vs fs_add_velocity
@program tcx_flow_fluid_add_density_texture fluid_vs fs_add_density_texture
@program tcx_flow_fluid_add_temperature_texture fluid_vs fs_add_temperature_texture
@program tcx_flow_fluid_divergence fluid_vs fs_divergence
@program tcx_flow_fluid_jacobi_pressure fluid_vs fs_jacobi_pressure
@program tcx_flow_fluid_gradient_subtract fluid_vs fs_gradient_subtract
@program tcx_flow_fluid_vorticity_curl fluid_vs fs_vorticity_curl
@program tcx_flow_fluid_vorticity_force fluid_vs fs_vorticity_force
@program tcx_flow_fluid_buoyancy fluid_vs fs_buoyancy
@program tcx_flow_fluid_obstacle_splat fluid_vs fs_obstacle_splat
@program tcx_flow_fluid_obstacle_offset fluid_vs fs_obstacle_offset
@program tcx_flow_fluid_apply_obstacle fluid_vs fs_apply_obstacle
