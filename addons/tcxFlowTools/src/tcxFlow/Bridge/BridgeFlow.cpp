#include "BridgeFlow.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

void BridgeFlow::setup(int width, int height) {
    resize(width, height);
}

void BridgeFlow::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

void BridgeFlow::update(const tc::Texture& input, float dt) {
    (void)input;
    age_ += std::max(0.0f, dt);
}

void BridgeFlow::update(float dt) {
    age_ += std::max(0.0f, dt);
}

void BridgeFlow::applyTo(Fluid2D& fluid) {
    (void)fluid;
}

} // namespace tcx::flow
