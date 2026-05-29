@module tcx_assimp_skin

@vs vs_skinned
#define TCX_MAX_BONES 128

layout(binding=0) uniform vs_params {
    mat4 model;
    mat4 viewProj;
    mat4 normalMat;
    mat4 bones[TCX_MAX_BONES];
};

in vec3 position;
in vec3 normal;
in vec2 texcoord0;
in vec4 tangent;
in vec4 boneIndices;
in vec4 boneWeights;

out vec3 v_worldNormal;
out vec2 v_uv;

mat4 weightedBoneMatrix() {
    mat4 skin = mat4(0.0);
    skin += bones[int(boneIndices.x)] * boneWeights.x;
    skin += bones[int(boneIndices.y)] * boneWeights.y;
    skin += bones[int(boneIndices.z)] * boneWeights.z;
    skin += bones[int(boneIndices.w)] * boneWeights.w;
    return skin;
}

void main() {
    mat4 skin = weightedBoneMatrix();
    vec4 localPos = skin * vec4(position, 1.0);
    vec3 localNormal = normalize((skin * vec4(normal, 0.0)).xyz);
    vec4 worldPos = model * localPos;
    v_worldNormal = normalize((normalMat * vec4(localNormal, 0.0)).xyz);
    v_uv = texcoord0;
    gl_Position = viewProj * worldPos;
}
@end

@fs fs_skinned
layout(binding=1) uniform fs_params {
    vec4 baseColor;
    vec4 lightDirAmbient; // xyz=world light direction, w=ambient
    vec4 materialFlags;   // x=hasBaseColorTexture, y=alphaCutoff, zw=unused
};

layout(binding=0) uniform texture2D baseColorTex;
layout(binding=0) uniform sampler baseColorTexSmp;

in vec3 v_worldNormal;
in vec2 v_uv;

out vec4 frag_color;

void main() {
    vec3 n = normalize(v_worldNormal);
    vec3 l = normalize(-lightDirAmbient.xyz);
    float ndl = max(dot(n, l), 0.0);
    float lit = clamp(lightDirAmbient.w + ndl * (1.0 - lightDirAmbient.w), 0.0, 1.0);
    vec4 albedo = baseColor;
    if (materialFlags.x > 0.5) {
        albedo *= texture(sampler2D(baseColorTex, baseColorTexSmp), v_uv);
    }
    if (albedo.a < materialFlags.y) discard;
    frag_color = vec4(albedo.rgb * lit, albedo.a);
}
@end

@program skinned vs_skinned fs_skinned
