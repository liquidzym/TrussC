#include "AverageFlow.h"

#include <cmath>

namespace tcx::flow {

void AverageFlow::update(const Fluid2D& fluid, int samplesX, int samplesY) {
    reset();
    if (!fluid.isAllocated()) return;
    samplesX = std::max(1, samplesX);
    samplesY = std::max(1, samplesY);

    tc::Vec2 sum(0, 0);
    float speedSum = 0.0f;
    for (int y = 0; y < samplesY; ++y) {
        for (int x = 0; x < samplesX; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) / samplesX;
            const float py = (static_cast<float>(y) + 0.5f) / samplesY;
            const tc::Vec2 v = fluid.sampleVelocityAtPosition(tc::Vec2(px * fluid.simWidth() / fluid.resolutionScale(),
                                                                       py * fluid.simHeight() / fluid.resolutionScale()));
            sum += v;
            speedSum += std::sqrt(v.x * v.x + v.y * v.y);
            ++sampleCount_;
        }
    }

    if (sampleCount_ > 0) {
        averageVelocity_ = sum / static_cast<float>(sampleCount_);
        averageSpeed_ = speedSum / static_cast<float>(sampleCount_);
    }
}

void AverageFlow::reset() {
    averageVelocity_ = tc::Vec2(0, 0);
    averageSpeed_ = 0.0f;
    sampleCount_ = 0;
}

} // namespace tcx::flow
