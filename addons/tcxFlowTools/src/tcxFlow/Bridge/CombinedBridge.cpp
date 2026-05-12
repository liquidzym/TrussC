#include "CombinedBridge.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

void CombinedBridge::setup(int width, int height) {
    resize(width, height);
}

void CombinedBridge::resize(int width, int height) {
    BridgeFlow::resize(width, height);
    velocity_.resize(width, height);
    density_.resize(width, height);
    temperature_.resize(width, height);
}

void CombinedBridge::update(const tc::Texture& input, float dt) {
    BridgeFlow::update(input, dt);
    velocity_.settings() = settings_;
    density_.settings() = settings_;
    temperature_.settings() = settings_;
    velocity_.update(input, dt);
    density_.update(input, dt);
    temperature_.update(input, dt);
}

void CombinedBridge::update(float dt) {
    BridgeFlow::update(dt);
    velocity_.settings() = settings_;
    density_.settings() = settings_;
    temperature_.settings() = settings_;
    velocity_.update(dt);
    density_.update(dt);
    temperature_.update(dt);
}

void CombinedBridge::applyTo(Fluid2D& fluid) {
    if (enableVelocity) velocity_.applyTo(fluid);
    if (enableDensity) density_.applyTo(fluid);
    if (enableTemperature) temperature_.applyTo(fluid);
}

} // namespace tcx::flow
