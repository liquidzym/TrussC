#pragma once

#include "../Core/FlowPass.h"
#include "../Core/FlowTypes.h"
#include "../Core/PingPongBuffer.h"
#include "OpticalFlowSettings.h"

namespace tcx::flow {

class OpticalFlow {
public:
    void setup(int width, int height, const OpticalFlowSettings& settings = {});
    void resize(int width, int height);
    void update(const tc::Texture& inputTexture, float dt);
    void updateProcedural(float time, float dt);
    void reset();

    void drawFlow(float x, float y, float w, float h) const;
    void drawDebug(float x, float y, float w, float h) const;

    OpticalFlowSettings& settings() { return settings_; }
    const OpticalFlowSettings& settings() const { return settings_; }
    const std::vector<tc::Vec2>& cpuFlow() const { return flow_; }
    const std::vector<float>& currentFrame() const { return currentFrame_; }
    const std::vector<float>& previousFrame() const { return previousFrame_; }
    const tc::Texture* getFlowTexture() const;
    const tc::Texture* getCurrentTexture() const;
    const tc::Texture* getPreviousTexture() const;
    int width() const { return width_; }
    int height() const { return height_; }
    bool isAllocated() const { return width_ > 0 && height_ > 0; }
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }
    float flowEnergy() const;
    float currentFrameEnergy() const;

private:
    int index(int x, int y) const { return y * width_ + x; }
    void generateProceduralFrame(float time);
    void blurFrame(std::vector<float>& frame, float radius) const;
    float sampleFrame(const std::vector<float>& frame, int x, int y) const;
    bool canUseGpu() const;
    bool ensureGpu();
    bool updateGpu(const tc::Texture& inputTexture, float dt);
    void setupGpuPasses();

    int width_ = 0;
    int height_ = 0;
    OpticalFlowSettings settings_;
    std::vector<tc::Vec2> flow_;
    std::vector<float> currentFrame_;
    std::vector<float> previousFrame_;
    tc::Fbo currentFbo_;
    tc::Fbo previousFbo_;
    tc::Fbo blurTempFbo_;
    tc::Fbo rawFlowFbo_;
    mutable tc::Fbo debugFbo_;
    PingPongBuffer flowBuffer_;
    FlowPass passCopy_;
    FlowPass passLuminance_;
    FlowPass passBlurHorizontal_;
    FlowPass passBlurVertical_;
    FlowPass passFlow_;
    FlowPass passTemporalSmooth_;
    mutable FlowPass passVisualize_;
    bool gpuPassesReady_ = false;
    bool firstGpuFrame_ = true;
    bool lastUpdateUsedGpu_ = false;
};

} // namespace tcx::flow
