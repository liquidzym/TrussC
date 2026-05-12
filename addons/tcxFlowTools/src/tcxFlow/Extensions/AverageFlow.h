#pragma once

#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

class AverageFlow {
public:
    void update(const Fluid2D& fluid, int samplesX = 24, int samplesY = 16);
    void reset();

    const tc::Vec2& averageVelocity() const { return averageVelocity_; }
    float averageSpeed() const { return averageSpeed_; }
    int sampleCount() const { return sampleCount_; }

private:
    tc::Vec2 averageVelocity_ = tc::Vec2(0, 0);
    float averageSpeed_ = 0.0f;
    int sampleCount_ = 0;
};

} // namespace tcx::flow
