#pragma once

#include "../Core/FlowPass.h"

namespace tcx::flow {

struct FlowHelperSettings {
    tc::Color color = tc::Color(1.0f);
    float gain = 1.0f;
    float threshold = 0.0f;
    float radius = 1.0f;
    float amount = 1.0f;
    float sourceGain = 1.0f;
};

class FlowHelperPipeline {
public:
    FlowHelperPipeline() = default;
    ~FlowHelperPipeline() = default;

    FlowHelperPipeline(const FlowHelperPipeline&) = delete;
    FlowHelperPipeline& operator=(const FlowHelperPipeline&) = delete;
    FlowHelperPipeline(FlowHelperPipeline&&) noexcept = default;
    FlowHelperPipeline& operator=(FlowHelperPipeline&&) noexcept = default;

    bool apply(FlowPassKind kind, const tc::Texture& source,
               int width = 0, int height = 0,
               const FlowHelperSettings& settings = {},
               const tc::Texture* secondary = nullptr);

    bool normalizeVector(const tc::Texture& source, float minMagnitude, float range,
                         int width = 0, int height = 0);
    bool decay(const tc::Texture& previous, const tc::Texture& current,
               float decayAmount, float sourceGain = 1.0f,
               int width = 0, int height = 0);
    bool colorizeLuminance(const tc::Texture& source, float gain = 1.0f,
                           float threshold = 0.0f,
                           const tc::Color& color = tc::Color(1.0f),
                           int width = 0, int height = 0);
    bool colorizeVelocity(const tc::Texture& source, float gain = 1.0f,
                          const tc::Color& color = tc::Color(1.0f),
                          int width = 0, int height = 0);
    bool colorizeGradient(const tc::Texture& source, const tc::Texture& gradient,
                          float gain = 1.0f, float bias = 0.0f,
                          const tc::Color& color = tc::Color(1.0f),
                          int width = 0, int height = 0);
    bool dilate(const tc::Texture& source, float radius = 1.0f,
                int width = 0, int height = 0);
    bool erode(const tc::Texture& source, float radius = 1.0f,
               int width = 0, int height = 0);
    bool inverseWarp(const tc::Texture& source, const tc::Texture& velocity,
                     float amount = 1.0f, int width = 0, int height = 0);
    bool ease(const tc::Texture& previous, const tc::Texture& current,
              float amount = 0.5f, int width = 0, int height = 0);
    bool timeBlur(const tc::Texture& previous, const tc::Texture& current,
                  float decayAmount = 0.15f, float sourceGain = 1.0f,
                  float radius = 1.0f, int width = 0, int height = 0);

    void reset();
    bool isAllocated() const { return output_.isAllocated(); }
    const tc::Texture* outputTexture() const;
    FlowPassKind lastKind() const { return passKind_; }

private:
    bool ensureOutput(int width, int height);

    FlowPass pass_;
    FlowPassKind passKind_ = FlowPassKind::Unknown;
    tc::Fbo output_;
};

} // namespace tcx::flow
