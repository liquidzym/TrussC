#include "VelocityBridge.h"
#include "../Fluid/Fluid2D.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

void VelocityBridge::update(const tc::Texture& input, float dt) {
    BridgeFlow::update(input, dt);
    renderTextureOutput(FlowPassKind::BridgeVelocity, input, tc::Color(1.0f),
                        settings_.velocityScale, settings_.threshold,
                        std::max(1.0f, settings_.blurRadius), 0.0f);
}

void VelocityBridge::applyTo(Fluid2D& fluid) {
    if (const tc::Texture* texture = outputTexture()) {
        fluid.applyVelocityTexture(*texture, 1.0f);
        return;
    }
    fluid.addVelocity(tc::Vec2(width_ * 0.5f, height_ * 0.5f), width_ * 0.08f,
                      tc::Vec2(std::sin(age_) * 30.0f, std::cos(age_) * 30.0f) * settings_.velocityScale);
}

} // namespace tcx::flow
