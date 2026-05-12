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

bool SplitVelocity::updateTexture(const Fluid2D& fluid, int width, int height, float gain, int mode) {
    lastUpdateUsedGpu_ = false;
    const tc::Texture* velocity = fluid.getVelocityTexture();
    if (!velocity || !velocity->isAllocated() || !sg_isvalid() || tc::headless::isActive()) return false;
    const int outWidth = std::max(1, width > 0 ? width : fluid.outputWidth());
    const int outHeight = std::max(1, height > 0 ? height : fluid.outputHeight());
    if (!outputFbo_.isAllocated() || outputFbo_.getWidth() != outWidth || outputFbo_.getHeight() != outHeight) {
        outputFbo_.allocate(outWidth, outHeight, 1, TextureFormat::RGBA8);
    }
    if (!splitPassReady_) {
        splitPass_.setup(FlowPassKind::ExtensionSplitVelocity);
        splitPassReady_ = true;
    }
    splitPass_.setTexture("tex0", *velocity);
    splitPass_.setColor(tc::Color(1.0f));
    splitPass_.setOptions(std::max(0.0f, gain), static_cast<float>(std::clamp(mode, 0, 2)), 1.0f, 0.0f);
    splitPass_.render(outputFbo_);
    lastUpdateUsedGpu_ = outputFbo_.isAllocated();
    return lastUpdateUsedGpu_;
}

void SplitVelocity::reset() {
    result_ = {};
    lastUpdateUsedGpu_ = false;
}

void SplitVelocity::draw(float x, float y, float w, float h) const {
    if (!outputFbo_.isAllocated()) return;
    tc::setColor(1.0f);
    outputFbo_.draw(x, y, w, h);
}

const tc::Texture* SplitVelocity::outputTexture() const {
    return outputFbo_.isAllocated() ? &outputFbo_.getTexture() : nullptr;
}

} // namespace tcx::flow
