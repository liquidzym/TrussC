#include "SplitVelocity.h"

#include <cmath>

namespace tcx::flow {

void SplitVelocity::update(const Fluid2D& fluid, int samplesX, int samplesY) {
    reset();
    if (!fluid.isAllocated()) return;
    samplesX = std::max(1, samplesX);
    samplesY = std::max(1, samplesY);

    int count = 0;
    for (int y = 0; y < samplesY; ++y) {
        for (int x = 0; x < samplesX; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) / samplesX;
            const float py = (static_cast<float>(y) + 0.5f) / samplesY;
            const tc::Vec2 v = fluid.sampleVelocityAtPosition(tc::Vec2(px * fluid.simWidth() / fluid.resolutionScale(),
                                                                       py * fluid.simHeight() / fluid.resolutionScale()));
            result_.positive.x += std::max(v.x, 0.0f);
            result_.positive.y += std::max(v.y, 0.0f);
            result_.negative.x += std::min(v.x, 0.0f);
            result_.negative.y += std::min(v.y, 0.0f);
            result_.horizontalEnergy += std::abs(v.x);
            result_.verticalEnergy += std::abs(v.y);
            ++count;
        }
    }

    if (count > 0) {
        const float inv = 1.0f / static_cast<float>(count);
        result_.positive *= inv;
        result_.negative *= inv;
        result_.horizontalEnergy *= inv;
        result_.verticalEnergy *= inv;
    }
}

void SplitVelocity::reset() {
    result_ = {};
}

} // namespace tcx::flow
