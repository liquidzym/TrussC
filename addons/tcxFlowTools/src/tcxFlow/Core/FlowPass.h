#pragma once

#include "FlowTypes.h"
#include <memory>
#include <unordered_map>

namespace tcx::flow {

enum class FlowPassKind {
    Unknown,
    Copy,
    Clear,
    Multiply,
    Threshold,
    Luminance,
    Difference,
    BlurHorizontal,
    BlurVertical,
    FluidAdvect,
    FluidSplat,
    FluidAddVelocity,
    FluidAddDensityTexture,
    FluidAddTemperatureTexture,
    FluidDivergence,
    FluidJacobiPressure,
    FluidGradientSubtract,
    FluidVorticityCurl,
    FluidVorticityForce,
    FluidBuoyancy,
    FluidDiffuse,
    FluidObstacleSplat,
    FluidObstacleOffset,
    FluidApplyObstacle,
    OpticalLuminance,
    OpticalDifference,
    OpticalGradient,
    OpticalFlow,
    OpticalTemporalSmooth,
    OpticalVisualize,
    BridgeLuminanceMask,
    BridgeVelocity,
    BridgeDensity,
    BridgeTemperature,
    VisualizeScalar,
    VisualizeDensity,
    VisualizeVelocityColor,
    VisualizePressure,
    VisualizeTemperature,
    ParticlesSpawn,
    ParticlesUpdate,
    ParticlesRender
};

struct FlowPassParams {
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float resolution[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    float texel[4] = {1.0f, 1.0f, 0.0f, 0.0f};
    float options[4] = {1.0f, 0.0f, 1.0f, 0.0f}; // gain, threshold, blur radius, spare
};

class FlowPass {
public:
    FlowPass();
    ~FlowPass();

    FlowPass(const FlowPass&) = delete;
    FlowPass& operator=(const FlowPass&) = delete;
    FlowPass(FlowPass&&) noexcept;
    FlowPass& operator=(FlowPass&&) noexcept;

    void setup(const std::string& shaderPath, const std::string& label = {});
    void setup(FlowPassKind kind, const std::string& label = {});
    bool isReady() const { return ready_; }
    const std::string& shaderPath() const { return shaderPath_; }
    const std::string& label() const { return label_; }
    const std::string& lastError() const { return lastError_; }
    FlowPassKind kind() const { return kind_; }

    void setTexture(const std::string& name, const tc::Texture* texture);
    void setTexture(const std::string& name, const tc::Texture& texture) { setTexture(name, &texture); }
    void setColor(const tc::Color& color);
    void setGain(float gain);
    void setThreshold(float threshold);
    void setBlurRadius(float radius);
    void setOptions(float x, float y, float z, float w);
    void setTexelExtra(float z, float w);
    void setResolution(int width, int height);
    const FlowPassParams& params() const { return params_; }
    FlowPassParams& mutableParams() { return params_; }

    template <typename T>
    void setUniform(const std::string& name, const T& value) {
        uniformSizes_[name] = sizeof(T);
        (void)value;
    }

    void render(tc::Fbo& target);

private:
    struct Impl;

    bool ensureShader();
    void bindTextures();

    std::string shaderPath_;
    std::string label_;
    std::string lastError_;
    FlowPassKind kind_ = FlowPassKind::Unknown;
    bool ready_ = false;
    bool shaderLoaded_ = false;
    std::unordered_map<std::string, const tc::Texture*> textures_;
    std::unordered_map<std::string, std::size_t> uniformSizes_;
    FlowPassParams params_;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcx::flow
