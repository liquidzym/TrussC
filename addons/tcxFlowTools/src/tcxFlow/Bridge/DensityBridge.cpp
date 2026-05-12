#include "DensityBridge.h"
#include "../Fluid/Fluid2D.h"

#include <algorithm>

namespace tcx::flow {

void DensityBridge::update(const tc::Texture& input, float dt) {
    BridgeFlow::update(input, dt);
    renderTextureOutput(FlowPassKind::BridgeDensity, input, color,
                        settings_.densityScale, settings_.threshold,
                        std::max(1.0f, settings_.blurRadius), 0.0f);
}

void DensityBridge::applyTo(Fluid2D& fluid) {
    if (const tc::Texture* texture = outputTexture()) {
        fluid.applyDensityTexture(*texture, 1.0f);
        return;
    }
    fluid.addDensity(tc::Vec2(width_ * 0.5f, height_ * 0.5f), width_ * 0.08f, color * settings_.densityScale);
}

} // namespace tcx::flow
