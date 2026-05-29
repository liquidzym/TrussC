#include "tcx/assimp/GpuSkinnedRenderer.h"
#include "shaders/tcxAssimpSkinned.glsl.h"

#include <algorithm>
#include <cstring>

namespace tcx::assimp {

namespace {

struct SkinnedVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;
    float bi0, bi1, bi2, bi3;
    float bw0, bw1, bw2, bw3;
};

tc::Color materialBaseColor(const SceneData& scene, int materialIndex) {
    if (materialIndex < 0 || materialIndex >= (int)scene.materials.size()) {
        return tc::Color(0.7f, 0.7f, 0.7f, 1.0f);
    }
    const auto& mat = scene.materials[materialIndex];
    tc::Color base = mat.baseColor;
    if (base.a == 1.0f && mat.opacity != 1.0f) {
        base.a = mat.opacity;
    }
    if (base.a == 1.0f && mat.diffuseColor.a != 1.0f) {
        base.a = mat.diffuseColor.a;
    }
    return base;
}

tc::Vec3 currentLightDirection() {
    for (const tc::Light* light : tc::internal::activeLights) {
        if (light && light->getType() == tc::LightType::Directional) {
            const tc::Vec3& d = light->getDirection();
            if (d.length() > 0.0001f) return d.normalized();
        }
    }
    return tc::Vec3(-0.35f, -0.85f, -0.4f).normalized();
}

void writeMat(float dst[16], const tc::Mat4& m) {
    tc::Mat4 t = m.transposed();
    std::memcpy(dst, t.m, sizeof(float) * 16);
}

} // namespace

GpuSkinnedRenderer::~GpuSkinnedRenderer() {
    clear();
}

void GpuSkinnedRenderer::destroyBuffers() {
    bool gpuAlive = sg_isvalid();
    for (auto& item : drawItems_) {
        if (gpuAlive && item.vertexBuffer.id != 0) {
            sg_destroy_buffer(item.vertexBuffer);
        }
        if (gpuAlive && item.indexBuffer.id != 0) {
            sg_destroy_buffer(item.indexBuffer);
        }
        item.vertexBuffer = {};
        item.indexBuffer = {};
    }
    drawItems_.clear();
}

void GpuSkinnedRenderer::clear() {
    destroyBuffers();
    for (auto& entry : pipelineCache_) {
        if (sg_isvalid() && entry.second.id != 0) sg_destroy_pipeline(entry.second);
    }
    pipelineCache_.clear();
    if (sg_isvalid() && shader_.id != 0) {
        sg_destroy_shader(shader_);
    }
    shader_ = {};
    if (sg_isvalid() && fallbackWhiteView_.id != 0) sg_destroy_view(fallbackWhiteView_);
    if (sg_isvalid() && fallbackWhiteImage_.id != 0) sg_destroy_image(fallbackWhiteImage_);
    if (sg_isvalid() && fallbackSampler_.id != 0) sg_destroy_sampler(fallbackSampler_);
    fallbackWhiteView_ = {};
    fallbackWhiteImage_ = {};
    fallbackSampler_ = {};
    shaderInitialized_ = false;
    fallbackInitialized_ = false;
    ready_ = false;
}

bool GpuSkinnedRenderer::canDrawScene(const SceneData& scene) const {
    if (scene.skeleton.bones.empty()) return false;
    if (scene.skeleton.bones.size() > (size_t)MaxBones) return false;
    return ready_;
}

void GpuSkinnedRenderer::ensureShader() {
    if (shaderInitialized_) return;
    shader_ = sg_make_shader(tcx_assimp_skin_skinned_shader_desc(sg_query_backend()));
    shaderInitialized_ = true;
}

void GpuSkinnedRenderer::ensureFallbacks() {
    if (fallbackInitialized_) return;
    uint8_t white[4] = {255, 255, 255, 255};
    sg_image_desc imageDesc = {};
    imageDesc.type = SG_IMAGETYPE_2D;
    imageDesc.width = 1;
    imageDesc.height = 1;
    imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
    imageDesc.data.mip_levels[0].ptr = white;
    imageDesc.data.mip_levels[0].size = sizeof(white);
    imageDesc.label = "tcx_assimp_gpu_skinned_white";
    fallbackWhiteImage_ = sg_make_image(&imageDesc);

    sg_view_desc viewDesc = {};
    viewDesc.texture.image = fallbackWhiteImage_;
    fallbackWhiteView_ = sg_make_view(&viewDesc);

    sg_sampler_desc samplerDesc = {};
    samplerDesc.min_filter = SG_FILTER_LINEAR;
    samplerDesc.mag_filter = SG_FILTER_LINEAR;
    samplerDesc.wrap_u = SG_WRAP_REPEAT;
    samplerDesc.wrap_v = SG_WRAP_REPEAT;
    samplerDesc.label = "tcx_assimp_gpu_skinned_sampler";
    fallbackSampler_ = sg_make_sampler(&samplerDesc);
    fallbackInitialized_ = true;
}

sg_pipeline GpuSkinnedRenderer::getPipeline(sg_pixel_format colorFormat, int sampleCount) {
    ensureShader();
    int key = static_cast<int>(colorFormat) | (sampleCount << 16);
    auto it = pipelineCache_.find(key);
    if (it != pipelineCache_.end()) return it->second;

    sg_pipeline_desc pd = {};
    pd.shader = shader_;
    pd.layout.attrs[ATTR_tcx_assimp_skin_skinned_position].format = SG_VERTEXFORMAT_FLOAT3;
    pd.layout.attrs[ATTR_tcx_assimp_skin_skinned_normal].format = SG_VERTEXFORMAT_FLOAT3;
    pd.layout.attrs[ATTR_tcx_assimp_skin_skinned_texcoord0].format = SG_VERTEXFORMAT_FLOAT2;
    pd.layout.attrs[ATTR_tcx_assimp_skin_skinned_tangent].format = SG_VERTEXFORMAT_FLOAT4;
    pd.layout.attrs[ATTR_tcx_assimp_skin_skinned_boneIndices].format = SG_VERTEXFORMAT_FLOAT4;
    pd.layout.attrs[ATTR_tcx_assimp_skin_skinned_boneWeights].format = SG_VERTEXFORMAT_FLOAT4;
    pd.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pd.depth.write_enabled = true;
    pd.depth.pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    pd.cull_mode = SG_CULLMODE_NONE;
    pd.colors[0].pixel_format = colorFormat;
    pd.colors[0].blend.enabled = true;
    pd.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
    pd.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pd.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
    pd.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    pd.sample_count = sampleCount;
    pd.index_type = SG_INDEXTYPE_UINT32;
    pd.label = "tcx_assimp_gpu_skinned_pipeline";

    sg_pipeline pipeline = sg_make_pipeline(&pd);
    pipelineCache_[key] = pipeline;
    return pipeline;
}

bool GpuSkinnedRenderer::build(const SceneData& scene) {
    destroyBuffers();
    ready_ = false;
    if (!sg_isvalid()) return false;
    if (scene.meshes.empty() || scene.skeleton.bones.empty()) return false;
    if (scene.skeleton.bones.size() > (size_t)MaxBones) return false;

    auto addDrawItem = [&](int meshIndex, int nodeIndex) {
        if (meshIndex < 0 || meshIndex >= (int)scene.meshes.size()) return;
        const auto& mesh = scene.meshes[meshIndex];
        if (mesh.vertices.empty()) return;

        std::vector<SkinnedVertex> vertices(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& src = mesh.vertices[i];
            auto& dst = vertices[i];
            dst.px = src.position.x;
            dst.py = src.position.y;
            dst.pz = src.position.z;
            dst.nx = src.normal.x;
            dst.ny = src.normal.y;
            dst.nz = src.normal.z;
            dst.u = src.texCoord.x;
            dst.v = src.texCoord.y;
            dst.tx = src.tangent.x;
            dst.ty = src.tangent.y;
            dst.tz = src.tangent.z;
            dst.tw = src.tangent.w;

            const bool hasBoneData = i < mesh.boneData.size();
            const auto& bd = hasBoneData ? mesh.boneData[i] : VertexBoneData{};
            float totalWeight = 0.0f;
            for (int s = 0; s < 4; ++s) {
                totalWeight += bd.weights[s];
            }
            if (totalWeight <= 0.0001f) {
                dst.bi0 = 0.0f; dst.bi1 = 0.0f; dst.bi2 = 0.0f; dst.bi3 = 0.0f;
                dst.bw0 = 1.0f; dst.bw1 = 0.0f; dst.bw2 = 0.0f; dst.bw3 = 0.0f;
            } else {
                dst.bi0 = static_cast<float>(std::min<int>(bd.indices[0], MaxBones - 1));
                dst.bi1 = static_cast<float>(std::min<int>(bd.indices[1], MaxBones - 1));
                dst.bi2 = static_cast<float>(std::min<int>(bd.indices[2], MaxBones - 1));
                dst.bi3 = static_cast<float>(std::min<int>(bd.indices[3], MaxBones - 1));
                dst.bw0 = bd.weights[0] / totalWeight;
                dst.bw1 = bd.weights[1] / totalWeight;
                dst.bw2 = bd.weights[2] / totalWeight;
                dst.bw3 = bd.weights[3] / totalWeight;
            }
        }

        DrawItem item;
        item.sourceMeshIndex = meshIndex;
        item.nodeIndex = nodeIndex >= 0 ? nodeIndex : mesh.nodeIndex;
        item.materialIndex = mesh.materialIndex;
        item.vertexCount = static_cast<int>(vertices.size());
        item.indexCount = static_cast<int>(mesh.indices.size());
        item.boneCount = static_cast<int>(scene.skeleton.bones.size());

        sg_buffer_desc vbd = {};
        vbd.data.ptr = vertices.data();
        vbd.data.size = vertices.size() * sizeof(SkinnedVertex);
        vbd.label = "tcx_assimp_gpu_skinned_vbuf";
        item.vertexBuffer = sg_make_buffer(&vbd);

        if (!mesh.indices.empty()) {
            sg_buffer_desc ibd = {};
            ibd.usage.index_buffer = true;
            ibd.data.ptr = mesh.indices.data();
            ibd.data.size = mesh.indices.size() * sizeof(uint32_t);
            ibd.label = "tcx_assimp_gpu_skinned_ibuf";
            item.indexBuffer = sg_make_buffer(&ibd);
        }

        if (item.vertexBuffer.id != 0) {
            drawItems_.push_back(item);
        }
    };

    for (size_t ni = 0; ni < scene.nodes.size(); ++ni) {
        for (int meshIndex : scene.nodes[ni].meshIndices) {
            addDrawItem(meshIndex, static_cast<int>(ni));
        }
    }
    if (drawItems_.empty()) {
        for (size_t mi = 0; mi < scene.meshes.size(); ++mi) {
            addDrawItem(static_cast<int>(mi), scene.meshes[mi].nodeIndex);
        }
    }

    ready_ = !drawItems_.empty();
    return ready_;
}

void GpuSkinnedRenderer::draw(const SceneData& scene,
                              const std::vector<tc::Material>& materials,
                              const std::vector<tc::Mat4>& nodeGlobals) {
    if (!canDrawScene(scene)) return;
    if (!sg_isvalid()) return;
    ensureFallbacks();

    sg_pixel_format colorFmt;
    int sampleCount;
    if (tc::internal::inFboPass) {
        colorFmt = tc::internal::currentFboColorFormat;
        sampleCount = tc::internal::currentFboSampleCount;
    } else {
        colorFmt = _SG_PIXELFORMAT_DEFAULT;
        sampleCount = sapp_sample_count();
        tc::ensureSwapchainPass();
    }

    sgl_draw();
    sg_apply_pipeline(getPipeline(colorFmt, sampleCount));

    const tc::Mat4 baseModel = tc::getDefaultContext().getCurrentMatrix();
    const tc::Mat4 viewProj = tc::internal::currentProjectionMatrix * tc::internal::currentViewMatrix;
    const tc::Vec3 lightDir = currentLightDirection();

    for (const auto& item : drawItems_) {
        if (item.vertexBuffer.id == 0) continue;

        sg_bindings bindings = {};
        bindings.vertex_buffers[0] = item.vertexBuffer;
        if (item.indexBuffer.id != 0) {
            bindings.index_buffer = item.indexBuffer;
        }
        const tc::Material* runtimeMaterial = nullptr;
        if (item.materialIndex >= 0 && item.materialIndex < (int)materials.size()) {
            runtimeMaterial = &materials[item.materialIndex];
        }
        if (runtimeMaterial && runtimeMaterial->hasBaseColorTexture()) {
            bindings.views[VIEW_tcx_assimp_skin_baseColorTex] = runtimeMaterial->getBaseColorTexture()->getView();
            bindings.samplers[SMP_tcx_assimp_skin_baseColorTexSmp] = runtimeMaterial->getBaseColorTexture()->getSampler();
        } else {
            bindings.views[VIEW_tcx_assimp_skin_baseColorTex] = fallbackWhiteView_;
            bindings.samplers[SMP_tcx_assimp_skin_baseColorTexSmp] = fallbackSampler_;
        }
        sg_apply_bindings(&bindings);

        tcx_assimp_skin_vs_params_t vsp = {};
        tc::Mat4 nodeGlobal = tc::Mat4::identity();
        if (item.nodeIndex >= 0 && item.nodeIndex < (int)nodeGlobals.size()) {
            nodeGlobal = nodeGlobals[item.nodeIndex];
        }
        writeMat(vsp.model, baseModel * nodeGlobal);
        writeMat(vsp.viewProj, viewProj);
        writeMat(vsp.normalMat, baseModel * nodeGlobal);

        tc::Mat4 meshNodeInv = tc::Mat4::identity();
        if (item.nodeIndex >= 0 && item.nodeIndex < (int)nodeGlobals.size()) {
            meshNodeInv = nodeGlobals[item.nodeIndex].inverted();
        }
        for (int i = 0; i < MaxBones; ++i) {
            tc::Mat4 boneMat = tc::Mat4::identity();
            if (i < (int)scene.skeleton.bones.size()) {
                boneMat = meshNodeInv * scene.skeleton.bones[i].finalMatrix;
            }
            writeMat(vsp.bones[i], boneMat);
        }
        sg_range vsRange = { &vsp, sizeof(vsp) };
        sg_apply_uniforms(UB_tcx_assimp_skin_vs_params, &vsRange);

        tcx_assimp_skin_fs_params_t fsp = {};
        tc::Color base = materialBaseColor(scene, item.materialIndex);
        fsp.baseColor[0] = base.r;
        fsp.baseColor[1] = base.g;
        fsp.baseColor[2] = base.b;
        fsp.baseColor[3] = base.a;
        fsp.lightDirAmbient[0] = lightDir.x;
        fsp.lightDirAmbient[1] = lightDir.y;
        fsp.lightDirAmbient[2] = lightDir.z;
        fsp.lightDirAmbient[3] = 0.32f;
        fsp.materialFlags[0] = runtimeMaterial && runtimeMaterial->hasBaseColorTexture() ? 1.0f : 0.0f;
        if (item.materialIndex >= 0 && item.materialIndex < (int)scene.materials.size()) {
            const auto& mat = scene.materials[item.materialIndex];
            fsp.materialFlags[1] = mat.alphaMode == "MASK" ? mat.alphaCutoff : -1.0f;
        } else {
            fsp.materialFlags[1] = -1.0f;
        }
        sg_range fsRange = { &fsp, sizeof(fsp) };
        sg_apply_uniforms(UB_tcx_assimp_skin_fs_params, &fsRange);

        if (item.indexBuffer.id != 0 && item.indexCount > 0) {
            sg_draw(0, item.indexCount, 1);
        } else {
            sg_draw(0, item.vertexCount, 1);
        }
    }
}

} // namespace tcx::assimp
