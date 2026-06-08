#include "BridgeFlow.h"
#include "../Fluid/Fluid2D.h"

#include <algorithm>

namespace tcx::flow {

namespace {

float bridgeOptionFlags(const BridgeSettings& settings) {
    float flags = 0.0f;
    if (settings.invert) flags += 1.0f;
    if (settings.useAlphaAsMask) flags += 2.0f;
    if (settings.mirrorX) flags += 4.0f;
    if (settings.mirrorY) flags += 8.0f;
    const int maskSource = settings.useAlphaAsMask
        ? static_cast<int>(BridgeMaskSource::Alpha)
        : static_cast<int>(settings.maskSource);
    flags += static_cast<float>(std::clamp(maskSource, 0, 6)) * 16.0f;
    return flags;
}

} // namespace

void BridgeFlow::setup(int width, int height) {
    resize(width, height);
}

void BridgeFlow::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    if (textureOutput_.isAllocated() &&
        (textureOutput_.getWidth() != width_ || textureOutput_.getHeight() != height_)) {
        textureOutput_.clear();
    }
    textureOutputValid_ = false;
}

void BridgeFlow::update(const tc::Texture& input, float dt) {
    (void)input;
    age_ += std::max(0.0f, dt);
    textureOutputValid_ = false;
}

void BridgeFlow::update(float dt) {
    age_ += std::max(0.0f, dt);
}

void BridgeFlow::applyTo(Fluid2D& fluid) {
    (void)fluid;
}

const tc::Texture* BridgeFlow::outputTexture() const {
    return textureOutputValid_ && textureOutput_.isAllocated() ? &textureOutput_.getTexture() : nullptr;
}

bool BridgeFlow::renderTextureOutput(FlowPassKind kind, const tc::Texture& input,
                                     const tc::Color& color, float gain, float threshold,
                                     float radius, float extra) {
    textureOutputValid_ = false;
    if (!input.isAllocated() || !sg_isvalid()) {
        return false;
    }

    const int outW = width_ > 0 ? width_ : input.getWidth();
    const int outH = height_ > 0 ? height_ : input.getHeight();
    if (!textureOutput_.isAllocated() ||
        textureOutput_.getWidth() != outW ||
        textureOutput_.getHeight() != outH) {
        textureOutput_.allocate(outW, outH, 1, tc::TextureFormat::RGBA32F);
        textureOutput_.getTexture().setFilter(tc::TextureFilter::Linear);
    }

    if (!texturePass_.isReady() || texturePassKind_ != kind) {
        texturePass_.setup(kind);
        texturePassKind_ = kind;
    }
    if (!texturePass_.isReady()) {
        return false;
    }

    texturePass_.setTexture("tex0", input);
    texturePass_.setColor(color);
    texturePass_.setOptions(gain, threshold, radius, bridgeOptionFlags(settings_) + extra);
    texturePass_.setTexelExtra(std::max(0.0f, settings_.maskSoftness),
                               std::max(0.0001f, settings_.maskGamma));
    texturePass_.render(textureOutput_);
    textureOutputValid_ = true;
    return true;
}

} // namespace tcx::flow
