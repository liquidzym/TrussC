#include "BridgeFlow.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

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
    texturePass_.setOptions(gain, threshold, radius, extra);
    texturePass_.render(textureOutput_);
    textureOutputValid_ = true;
    return true;
}

} // namespace tcx::flow
