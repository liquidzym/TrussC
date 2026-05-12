#include "FlowPass.h"
#include "common/common.glsl.h"
#include "fluid/fluid.glsl.h"
#include "opticalflow/opticalflow.glsl.h"
#include "bridge/bridge.glsl.h"
#include "visualization/visualization.glsl.h"
#include "particles/particles.glsl.h"

#include <algorithm>
#include <array>

namespace tcx::flow {

namespace {

using ShaderDescFn = const sg_shader_desc* (*)(sg_backend);

struct PassInfo {
    FlowPassKind kind = FlowPassKind::Unknown;
    const char* label = "";
    const char* path = "";
    ShaderDescFn desc = nullptr;
};

constexpr std::array<PassInfo, 43> kCommonPasses = {{
    {FlowPassKind::Copy, "copy", "shaders/common/copy.glsl", tcx_flow_copy_shader_desc},
    {FlowPassKind::Clear, "clear", "shaders/common/clear.glsl", tcx_flow_clear_shader_desc},
    {FlowPassKind::Multiply, "multiply", "shaders/common/multiply.glsl", tcx_flow_multiply_shader_desc},
    {FlowPassKind::Threshold, "threshold", "shaders/common/threshold.glsl", tcx_flow_threshold_shader_desc},
    {FlowPassKind::Luminance, "luminance", "shaders/common/luminance.glsl", tcx_flow_luminance_shader_desc},
    {FlowPassKind::Difference, "difference", "shaders/common/difference.glsl", tcx_flow_difference_shader_desc},
    {FlowPassKind::BlurHorizontal, "blur_horizontal", "shaders/common/blur_horizontal.glsl", tcx_flow_blur_horizontal_shader_desc},
    {FlowPassKind::BlurVertical, "blur_vertical", "shaders/common/blur_vertical.glsl", tcx_flow_blur_vertical_shader_desc},
    {FlowPassKind::FluidAdvect, "fluid_advect", "shaders/fluid/advect.glsl", tcx_flow_fluid_advect_shader_desc},
    {FlowPassKind::FluidSplat, "fluid_splat", "shaders/fluid/splat.glsl", tcx_flow_fluid_splat_shader_desc},
    {FlowPassKind::FluidAddVelocity, "fluid_add_velocity", "shaders/fluid/add_velocity.glsl", tcx_flow_fluid_add_velocity_shader_desc},
    {FlowPassKind::FluidAddDensityTexture, "fluid_add_density_texture", "shaders/fluid/add_density_texture.glsl", tcx_flow_fluid_add_density_texture_shader_desc},
    {FlowPassKind::FluidAddTemperatureTexture, "fluid_add_temperature_texture", "shaders/fluid/add_temperature_texture.glsl", tcx_flow_fluid_add_temperature_texture_shader_desc},
    {FlowPassKind::FluidDivergence, "fluid_divergence", "shaders/fluid/divergence.glsl", tcx_flow_fluid_divergence_shader_desc},
    {FlowPassKind::FluidJacobiPressure, "fluid_jacobi_pressure", "shaders/fluid/jacobi_pressure.glsl", tcx_flow_fluid_jacobi_pressure_shader_desc},
    {FlowPassKind::FluidGradientSubtract, "fluid_gradient_subtract", "shaders/fluid/gradient_subtract.glsl", tcx_flow_fluid_gradient_subtract_shader_desc},
    {FlowPassKind::FluidVorticityCurl, "fluid_vorticity_curl", "shaders/fluid/vorticity_curl.glsl", tcx_flow_fluid_vorticity_curl_shader_desc},
    {FlowPassKind::FluidVorticityForce, "fluid_vorticity_force", "shaders/fluid/vorticity_force.glsl", tcx_flow_fluid_vorticity_force_shader_desc},
    {FlowPassKind::FluidBuoyancy, "fluid_buoyancy", "shaders/fluid/buoyancy.glsl", tcx_flow_fluid_buoyancy_shader_desc},
    {FlowPassKind::FluidDiffuse, "fluid_diffuse", "shaders/fluid/diffuse.glsl", tcx_flow_fluid_diffuse_shader_desc},
    {FlowPassKind::FluidObstacleSplat, "fluid_obstacle_splat", "shaders/fluid/obstacle_splat.glsl", tcx_flow_fluid_obstacle_splat_shader_desc},
    {FlowPassKind::FluidObstacleOffset, "fluid_obstacle_offset", "shaders/fluid/obstacle_offset.glsl", tcx_flow_fluid_obstacle_offset_shader_desc},
    {FlowPassKind::FluidApplyObstacle, "fluid_apply_obstacle", "shaders/fluid/apply_obstacle.glsl", tcx_flow_fluid_apply_obstacle_shader_desc},
    {FlowPassKind::OpticalLuminance, "optical_luminance", "shaders/opticalflow/luminance.glsl", tcx_flow_optical_luminance_shader_desc},
    {FlowPassKind::OpticalDifference, "optical_difference", "shaders/opticalflow/difference.glsl", tcx_flow_optical_difference_shader_desc},
    {FlowPassKind::OpticalGradient, "optical_gradient", "shaders/opticalflow/gradient.glsl", tcx_flow_optical_gradient_shader_desc},
    {FlowPassKind::OpticalFlow, "optical_flow", "shaders/opticalflow/optical_flow.glsl", tcx_flow_optical_flow_shader_desc},
    {FlowPassKind::OpticalTemporalSmooth, "optical_temporal_smooth", "shaders/opticalflow/temporal_smooth.glsl", tcx_flow_optical_temporal_smooth_shader_desc},
    {FlowPassKind::OpticalVisualize, "optical_visualize", "shaders/opticalflow/visualize.glsl", tcx_flow_optical_visualize_shader_desc},
    {FlowPassKind::BridgeLuminanceMask, "bridge_luminance_mask", "shaders/bridge/luminance_mask.glsl", tcx_flow_bridge_luminance_mask_shader_desc},
    {FlowPassKind::BridgeVelocity, "bridge_velocity", "shaders/bridge/velocity.glsl", tcx_flow_bridge_velocity_shader_desc},
    {FlowPassKind::BridgeDensity, "bridge_density", "shaders/bridge/density.glsl", tcx_flow_bridge_density_shader_desc},
    {FlowPassKind::BridgeTemperature, "bridge_temperature", "shaders/bridge/temperature.glsl", tcx_flow_bridge_temperature_shader_desc},
    {FlowPassKind::VisualizeScalar, "visualize_scalar", "shaders/visualization/scalar.glsl", tcx_flow_visualize_scalar_shader_desc},
    {FlowPassKind::VisualizeDensity, "visualize_density", "shaders/visualization/density.glsl", tcx_flow_visualize_density_shader_desc},
    {FlowPassKind::VisualizeVelocityColor, "visualize_velocity_color", "shaders/visualization/velocity_color.glsl", tcx_flow_visualize_velocity_color_shader_desc},
    {FlowPassKind::VisualizePressure, "visualize_pressure", "shaders/visualization/pressure.glsl", tcx_flow_visualize_pressure_shader_desc},
    {FlowPassKind::VisualizeTemperature, "visualize_temperature", "shaders/visualization/temperature.glsl", tcx_flow_visualize_temperature_shader_desc},
    {FlowPassKind::VisualizeCombined, "visualize_combined", "shaders/visualization/combined.glsl", tcx_flow_visualize_combined_shader_desc},
    {FlowPassKind::VisualizeLic, "visualize_lic", "shaders/visualization/lic.glsl", tcx_flow_visualize_lic_shader_desc},
    {FlowPassKind::ParticlesSpawn, "particles_spawn", "shaders/particles/spawn.glsl", tcx_flow_particles_spawn_shader_desc},
    {FlowPassKind::ParticlesUpdate, "particles_update", "shaders/particles/update.glsl", tcx_flow_particles_update_shader_desc},
    {FlowPassKind::ParticlesRender, "particles_render", "shaders/particles/render.glsl", tcx_flow_particles_render_shader_desc},
}};

const PassInfo* findPass(FlowPassKind kind) {
    for (const auto& pass : kCommonPasses) {
        if (pass.kind == kind) return &pass;
    }
    return nullptr;
}

const PassInfo* findPass(const std::string& shaderPath, const std::string& label) {
    for (const auto& pass : kCommonPasses) {
        if (!label.empty() && label == pass.label) return &pass;
        if (!shaderPath.empty()) {
            const std::string path(pass.path);
            if (shaderPath == path) return &pass;
            const auto slash = path.find_last_of('/');
            if (slash != std::string::npos && shaderPath == path.substr(slash + 1)) return &pass;
        }
    }
    return nullptr;
}

int textureSlotForName(const std::string& name) {
    if (name == "tex3" || name == "obstacleOffset" || name == "obstacleOffsetTex") return 3;
    if (name == "tex2" || name == "density" || name == "densityTex") return 2;
    if (name == "tex1" || name == "previous" || name == "prev" || name == "b" ||
        name == "fluidSourceTex" || name == "pressureTex" ||
        name == "divergence" || name == "divergenceTex" || name == "curl" || name == "curlTex" ||
        name == "obstacle" || name == "obstacleTex" ||
        name == "temperature" || name == "temperatureTex" || name == "oflowPreviousTex" ||
        name == "oflowPreviousFlowTex" || name == "previousFlow" || name == "particleVelocityTex") return 1;
    return 0;
}

} // namespace

struct FlowPass::Impl {
    class Shader : public tc::FullscreenShader {
    public:
        void setInput(int slot, const tc::Texture* texture) {
            if (slot < 0 || slot >= static_cast<int>(views_.size())) return;
            if (!texture) {
                views_[slot] = {};
                samplers_[slot] = {};
                return;
            }
            views_[slot] = texture->getView();
            samplers_[slot] = texture->getSampler();
        }

    protected:
        sg_pipeline_desc createPipelineDesc() override {
            sg_pipeline_desc desc = {};
            desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
            desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
            desc.depth.compare = SG_COMPAREFUNC_ALWAYS;
            desc.depth.write_enabled = false;
            desc.cull_mode = SG_CULLMODE_NONE;
            desc.colors[0].blend.enabled = false;
            desc.index_type = SG_INDEXTYPE_UINT16;
            desc.label = "tcx_flow_fullscreen_overwrite_pipeline";
            return desc;
        }

        void setupBindings(sg_bindings& bind) override {
            for (int i = 0; i < static_cast<int>(views_.size()); ++i) {
                if (views_[i].id) {
                    bind.views[i] = views_[i];
                    bind.samplers[i] = samplers_[i];
                }
            }
        }

    private:
        std::array<sg_view, 4> views_ = {};
        std::array<sg_sampler, 4> samplers_ = {};
    };

    Shader shader;
};

FlowPass::FlowPass() : impl_(std::make_unique<Impl>()) {}
FlowPass::~FlowPass() = default;
FlowPass::FlowPass(FlowPass&&) noexcept = default;
FlowPass& FlowPass::operator=(FlowPass&&) noexcept = default;

void FlowPass::setup(const std::string& shaderPath, const std::string& label) {
    shaderPath_ = shaderPath;
    label_ = label;
    lastError_.clear();
    shaderLoaded_ = false;
    kind_ = FlowPassKind::Unknown;

    const PassInfo* pass = findPass(shaderPath, label);
    if (pass) {
        setup(pass->kind, label.empty() ? pass->label : label);
        shaderPath_ = shaderPath.empty() ? pass->path : shaderPath;
        return;
    }

    ready_ = false;
    if (shaderPath.empty()) {
        lastError_ = "FlowPass setup requires a shader path or generated shader descriptor.";
    } else {
        lastError_ = "FlowPass shader path is not one of the built-in tcxFlowTools common passes.";
    }
}

void FlowPass::setup(FlowPassKind kind, const std::string& label) {
    const PassInfo* pass = findPass(kind);
    kind_ = kind;
    shaderLoaded_ = false;
    lastError_.clear();

    if (!pass || !pass->desc) {
        ready_ = false;
        shaderPath_.clear();
        label_ = label;
        lastError_ = "FlowPass setup received an unsupported pass kind.";
        return;
    }

    ready_ = true;
    shaderPath_ = pass->path;
    label_ = label.empty() ? pass->label : label;
}

void FlowPass::setTexture(const std::string& name, const tc::Texture* texture) {
    textures_[name] = texture;
}

void FlowPass::setColor(const tc::Color& color) {
    params_.color[0] = color.r;
    params_.color[1] = color.g;
    params_.color[2] = color.b;
    params_.color[3] = color.a;
}

void FlowPass::setGain(float gain) {
    params_.options[0] = gain;
}

void FlowPass::setThreshold(float threshold) {
    params_.options[1] = threshold;
}

void FlowPass::setBlurRadius(float radius) {
    params_.options[2] = std::max(0.0f, radius);
}

void FlowPass::setOptions(float x, float y, float z, float w) {
    params_.options[0] = x;
    params_.options[1] = y;
    params_.options[2] = z;
    params_.options[3] = w;
}

void FlowPass::setTexelExtra(float z, float w) {
    params_.texel[2] = z;
    params_.texel[3] = w;
}

void FlowPass::setResolution(int width, int height) {
    const float w = static_cast<float>(std::max(1, width));
    const float h = static_cast<float>(std::max(1, height));
    params_.resolution[0] = w;
    params_.resolution[1] = h;
    params_.resolution[2] = 1.0f / w;
    params_.resolution[3] = 1.0f / h;
    params_.texel[0] = 1.0f / w;
    params_.texel[1] = 1.0f / h;
}

void FlowPass::render(tc::Fbo& target) {
    if (!ready_) return;
    if (!ensureShader()) return;
    setResolution(target.getWidth(), target.getHeight());
    bindTextures();

    target.begin();
    impl_->shader.setParams(params_);
    impl_->shader.draw();
    target.end();
}

bool FlowPass::ensureShader() {
    if (shaderLoaded_) return true;
    const PassInfo* pass = findPass(kind_);
    if (!pass || !pass->desc) {
        lastError_ = "FlowPass render requires a supported generated shader descriptor.";
        return false;
    }
    if (!impl_->shader.load(pass->desc)) {
        lastError_ = "FlowPass failed to load generated shader descriptor.";
        return false;
    }
    shaderLoaded_ = true;
    lastError_.clear();
    return true;
}

void FlowPass::bindTextures() {
    for (const auto& [name, texture] : textures_) {
        impl_->shader.setInput(textureSlotForName(name), texture);
    }
}

} // namespace tcx::flow
