#pragma once

#include "../Core/FlowPass.h"
#include "../Core/PingPongBuffer.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

struct SplitVelocityResult {
    tc::Vec2 positive = tc::Vec2(0, 0);
    tc::Vec2 negative = tc::Vec2(0, 0);
    float horizontalEnergy = 0.0f;
    float verticalEnergy = 0.0f;
};

struct SplitVelocityFieldStyle {
    int columns = 36;
    int rows = 20;
    float scale = 0.10f;
    float alpha = 0.82f;
    float meshAlpha = 0.12f;
    bool drawMesh = true;
};

class SplitVelocity {
public:
    void update(const Fluid2D& fluid, int samplesX = 24, int samplesY = 16);
    bool updateTexture(const Fluid2D& fluid, int width = 0, int height = 0, float gain = 0.06f, int mode = 0);
    void reset();
    void draw(float x, float y, float w, float h) const;
    void drawField(const Fluid2D& fluid, float x, float y, float w, float h) const;
    void drawField(const Fluid2D& fluid, float x, float y, float w, float h,
                   const SplitVelocityFieldStyle& style) const;

    void setForce(float value);
    void setNormalizeMin(float value);
    void setNormalizeRange(float value);
    void setDecay(float value);
    void setTrailBlend(float value);
    void setFieldStyle(const SplitVelocityFieldStyle& style);

    float force() const { return force_; }
    float normalizeMin() const { return normalizeMin_; }
    float normalizeRange() const { return normalizeRange_; }
    float decay() const { return decay_; }
    float trailBlend() const { return trailBlend_; }
    const SplitVelocityFieldStyle& fieldStyle() const { return fieldStyle_; }

    const SplitVelocityResult& result() const { return result_; }
    const tc::Texture* outputTexture() const;
    const tc::Texture* splitTexture() const;
    const tc::Texture* normalizedTexture() const;
    const tc::Texture* trailTexture() const;
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }

private:
    bool ensureGpuResources(int width, int height);
    void ensurePasses();

    SplitVelocityResult result_;
    FlowPass splitPass_;
    FlowPass normalizePass_;
    FlowPass decayPass_;
    FlowPass visualPass_;
    tc::Fbo splitFbo_;
    tc::Fbo normalizedFbo_;
    PingPongBuffer trail_;
    tc::Fbo outputFbo_;
    bool splitPassReady_ = false;
    bool normalizePassReady_ = false;
    bool decayPassReady_ = false;
    bool visualPassReady_ = false;
    bool lastUpdateUsedGpu_ = false;
    float force_ = 1.0f;
    float normalizeMin_ = 0.0f;
    float normalizeRange_ = 1.0f;
    float decay_ = 0.12f;
    float trailBlend_ = 1.0f;
    SplitVelocityFieldStyle fieldStyle_;
};

} // namespace tcx::flow
