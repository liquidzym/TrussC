#include "TemperatureBridge.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

void TemperatureBridge::applyTo(Fluid2D& fluid) {
    fluid.addTemperature(tc::Vec2(width_ * 0.5f, height_ * 0.5f), width_ * 0.08f, settings_.temperatureScale);
}

} // namespace tcx::flow
