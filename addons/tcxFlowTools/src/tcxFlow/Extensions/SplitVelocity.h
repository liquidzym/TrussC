#pragma once

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
    void reset();

    const SplitVelocityResult& result() const { return result_; }

private:
    SplitVelocityResult result_;
};

} // namespace tcx::flow
