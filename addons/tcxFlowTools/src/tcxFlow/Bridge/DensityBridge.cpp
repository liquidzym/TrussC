#include "DensityBridge.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

void DensityBridge::applyTo(Fluid2D& fluid) {
    fluid.addDensity(tc::Vec2(width_ * 0.5f, height_ * 0.5f), width_ * 0.08f, color * settings_.densityScale);
}

} // namespace tcx::flow
