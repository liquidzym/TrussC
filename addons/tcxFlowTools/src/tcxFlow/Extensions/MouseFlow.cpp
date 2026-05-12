#include "MouseFlow.h"

#include <cmath>

namespace tcx::flow {

void MouseFlow::reset() {
}

void MouseFlow::addDrag(Fluid2D& fluid, const tc::Vec2& position, const tc::Vec2& previousPosition, int button) {
    const tc::Vec2 delta = position - previousPosition;
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    const tc::Color coreColor = button == 0 ? primaryColor : tc::Color(1.0f, 0.35f, 0.15f, 1.0f);
    const tc::Color outerColor = button == 0 ? secondaryColor : tc::Color(0.8f, 0.08f, 0.02f, 1.0f);
    if (distance > 0.0f) {
        fluid.addVelocity(position, velocityRadius, delta * velocityScale);
    }
    fluid.addDensity(position, densityOuterRadius, outerColor);
    fluid.addDensity(position, densityRadius, coreColor);
}

} // namespace tcx::flow
