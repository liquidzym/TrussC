#include "VelocityBridge.h"
#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

void VelocityBridge::applyTo(Fluid2D& fluid) {
    fluid.addVelocity(tc::Vec2(width_ * 0.5f, height_ * 0.5f), width_ * 0.08f,
                      tc::Vec2(std::sin(age_) * 30.0f, std::cos(age_) * 30.0f) * settings_.velocityScale);
}

} // namespace tcx::flow
