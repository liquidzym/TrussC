#include "TemperatureBridge.h"
#include "../Fluid/Fluid2D.h"

#include <algorithm>

namespace tcx::flow {

void TemperatureBridge::update(const tc::Texture& input, float dt) {
    BridgeFlow::update(input, dt);
    renderTextureOutput(FlowPassKind::BridgeTemperature, input, tc::Color(1.0f),
                        settings_.temperatureScale, settings_.threshold,
                        std::max(1.0f, settings_.blurRadius), 0.0f);
}

void TemperatureBridge::applyTo(Fluid2D& fluid) {
    if (const tc::Texture* texture = outputTexture()) {
        fluid.applyTemperatureTexture(*texture, 1.0f);
        return;
    }
    fluid.addTemperature(tc::Vec2(width_ * 0.5f, height_ * 0.5f), width_ * 0.08f, settings_.temperatureScale);
}

} // namespace tcx::flow
