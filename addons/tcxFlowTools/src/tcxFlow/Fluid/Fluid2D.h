#pragma once

#include "../Core/FlowTypes.h"
#include "../Core/FlowPass.h"
#include "FluidBuffers.h"
#include "FluidSettings.h"

namespace tcx::flow {

class Fluid2D {
public:
    void setup(int width, int height, const FluidSettings& settings = {});
    void resize(int width, int height);
    void update(float dt);
    void reset();
    void clear();

    void addVelocity(const tc::Vec2& position, float radius, const tc::Vec2& velocity);
    void addDensity(const tc::Vec2& position, float radius, const tc::Color& color);
    void addTemperature(const tc::Vec2& position, float radius, float temperature);
    void addObstacle(const tc::Vec2& position, float radius);
    void clearObstacles();

    void applyVelocityTexture(const tc::Texture& velocityTexture, float scale = 1.0f);
    void applyDensityTexture(const tc::Texture& densityTexture, float scale = 1.0f);
    void applyVelocityDensityTexture(const tc::Texture& velocityTexture, float scale = 1.0f, const tc::Color& color = tc::Color(0.08f, 0.42f, 1.0f, 1.0f));
    void applyTemperatureTexture(const tc::Texture& temperatureTexture, float scale = 1.0f);
    void applyVelocityTemperatureTexture(const tc::Texture& velocityTexture, float scale = 1.0f);
    void applyVelocityField(const std::vector<tc::Vec2>& field, int fieldWidth, int fieldHeight, float scale = 1.0f);

    void drawDensity(float x, float y, float w, float h) const;
    void drawVelocity(float x, float y, float w, float h) const;
    void drawPressure(float x, float y, float w, float h) const;
    void drawTemperature(float x, float y, float w, float h) const;
    void drawCombined(float x, float y, float w, float h) const;
    void drawLic(float x, float y, float w, float h) const;

    FluidSettings& settings() { return settings_; }
    const FluidSettings& settings() const { return settings_; }
    int simWidth() const { return simWidth_; }
    int simHeight() const { return simHeight_; }
    int outputWidth() const { return outputWidth_; }
    int outputHeight() const { return outputHeight_; }
    float resolutionScale() const { return settings_.resolutionScale; }
    float outputResolutionScale() const {
        return settings_.outputResolutionScale > 0.0f ? settings_.outputResolutionScale : settings_.resolutionScale;
    }
    bool isAllocated() const { return simWidth_ > 0 && simHeight_ > 0; }
    bool isGpuReady() const;
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }
    float densityEnergy() const;
    float velocityEnergy() const;
    float pressureEnergy() const;
    float temperatureEnergy() const;
    tc::Vec2 sampleVelocityAtPosition(const tc::Vec2& position) const;
    const tc::Texture* getVelocityTexture() const;
    const tc::Texture* getDensityTexture() const;
    const tc::Texture* getPressureTexture() const;
    const tc::Texture* getTemperatureTexture() const;

private:
    int index(int x, int y) const { return y * simWidth_ + x; }
    void splatDensity(const tc::Vec2& position, float radius, const tc::Color& color);
    void splatVelocity(const tc::Vec2& position, float radius, const tc::Vec2& velocity);
    tc::Vec2 sampleVelocity(const std::vector<tc::Vec2>& field, float x, float y) const;
    tc::Vec2 sampleExternalVelocity(const std::vector<tc::Vec2>& field, int fieldWidth, int fieldHeight, float x, float y) const;
    tc::Color sampleDensity(const std::vector<tc::Color>& field, float x, float y) const;
    float sampleScalar(const std::vector<float>& field, float x, float y) const;
    void diffuseVelocity(float amount);
    void diffuseDensity(float amount);
    void diffuseScalar(std::vector<float>& field, float amount);
    void applyVorticity(float dt);
    bool canUseGpu() const;
    bool ensureGpu();
    bool updateGpu(float dt);
    void setupGpuPasses();
    void clearPendingInputs();
    void renderSplat(PingPongBuffer& target, const tc::Vec2& position, float radius, const tc::Color& color, float gain, float blendMode = 0.0f, bool clampOutput = false);
    void renderObstacleSplat(const tc::Vec2& position, float radius);
    void renderObstacleOffset();
    void applyObstacle(PingPongBuffer& target, float threshold = 0.5f);
    void renderAdvect(PingPongBuffer& target, const tc::Texture& velocityTexture, float step, float decay, bool clampOutput = false);
    void renderDiffuse(PingPongBuffer& target, float amount, bool clampOutput = false);
    void renderVelocityTexture(const tc::Texture& texture, float scale, bool mixMode);
    void renderDensityTexture(const tc::Texture& texture, float scale, const tc::Color& color, bool velocityMagnitude);
    void renderTemperatureTexture(const tc::Texture& texture, float scale, bool velocityMagnitude);
    bool ensureDebugFbo() const;
    void uploadDensityImage() const;

    struct DensitySplat {
        tc::Vec2 position;
        float radius = 1.0f;
        tc::Color color;
    };

    struct VelocitySplat {
        tc::Vec2 position;
        float radius = 1.0f;
        tc::Vec2 velocity;
    };

    struct TemperatureSplat {
        tc::Vec2 position;
        float radius = 1.0f;
        float temperature = 0.0f;
    };

    struct ObstacleSplat {
        tc::Vec2 position;
        float radius = 1.0f;
    };

    int inputWidth_ = 0;
    int inputHeight_ = 0;
    int simWidth_ = 0;
    int simHeight_ = 0;
    int outputWidth_ = 0;
    int outputHeight_ = 0;
    FluidSettings settings_;
    std::vector<tc::Color> density_;
    std::vector<tc::Vec2> velocity_;
    std::vector<float> pressure_;
    std::vector<float> temperature_;
    std::vector<float> divergence_;
    mutable tc::Image densityImage_;
    FluidBuffers gpuBuffers_;
    bool gpuPassesReady_ = false;
    bool lastUpdateUsedGpu_ = false;
    FlowPass passAdvect_;
    FlowPass passSplat_;
    FlowPass passAddVelocity_;
    FlowPass passAddDensityTexture_;
    FlowPass passAddTemperatureTexture_;
    FlowPass passDivergence_;
    FlowPass passJacobiPressure_;
    FlowPass passGradientSubtract_;
    FlowPass passVorticityCurl_;
    FlowPass passVorticityForce_;
    FlowPass passBuoyancy_;
    FlowPass passDiffuse_;
    FlowPass passObstacleSplat_;
    FlowPass passObstacleOffset_;
    FlowPass passApplyObstacle_;
    mutable FlowPass passVisualizeDensity_;
    mutable FlowPass passVisualizeVelocity_;
    mutable FlowPass passVisualizePressure_;
    mutable FlowPass passVisualizeTemperature_;
    mutable FlowPass passVisualizeCombined_;
    mutable FlowPass passVisualizeLic_;
    mutable tc::Fbo debugFbo_;
    tc::Texture externalVelocityTexture_;
    std::vector<float> externalVelocityPixels_;
    const tc::Texture* pendingVelocityTexture_ = nullptr;
    float pendingVelocityTextureScale_ = 1.0f;
    bool pendingVelocityTextureMixMode_ = false;
    const tc::Texture* pendingDensityTexture_ = nullptr;
    float pendingDensityTextureScale_ = 1.0f;
    tc::Color pendingDensityTextureColor_ = tc::Color(1.0f);
    bool pendingDensityTextureUsesVelocity_ = false;
    const tc::Texture* pendingTemperatureTexture_ = nullptr;
    float pendingTemperatureTextureScale_ = 1.0f;
    bool pendingTemperatureTextureUsesVelocity_ = false;
    std::vector<DensitySplat> pendingDensitySplats_;
    std::vector<VelocitySplat> pendingVelocitySplats_;
    std::vector<TemperatureSplat> pendingTemperatureSplats_;
    std::vector<ObstacleSplat> obstacleSplats_;
    bool obstaclesDirty_ = false;
};

} // namespace tcx::flow
