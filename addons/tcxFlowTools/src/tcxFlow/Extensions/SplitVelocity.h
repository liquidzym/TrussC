#pragma once

#include "../Core/FlowPass.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

struct SplitVelocityResult {
    tc::Vec2 positive = tc::Vec2(0, 0);
    tc::Vec2 negative = tc::Vec2(0, 0);
    float horizontalEnergy = 0.0f;
    float verticalEnergy = 0.0f;
};

class SplitVelocity {
public:
    void update(const Fluid2D& fluid, int samplesX = 24, int samplesY = 16);
    bool updateTexture(const Fluid2D& fluid, int width = 0, int height = 0, float gain = 0.06f, int mode = 0);
    void reset();
    void draw(float x, float y, float w, float h) const;

    const SplitVelocityResult& result() const { return result_; }
    const tc::Texture* outputTexture() const;
    bool lastUpdateUsedGpu() const { return lastUpdateUsedGpu_; }

private:
    SplitVelocityResult result_;
    FlowPass splitPass_;
    tc::Fbo outputFbo_;
    bool splitPassReady_ = false;
    bool lastUpdateUsedGpu_ = false;
};

} // namespace tcx::flow
