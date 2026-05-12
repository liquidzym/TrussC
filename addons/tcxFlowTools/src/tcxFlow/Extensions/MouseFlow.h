#pragma once

#include "../Fluid/Fluid2D.h"

namespace tcx::flow {

class MouseFlow {
public:
    void reset();
    void addDrag(Fluid2D& fluid, const tc::Vec2& position, const tc::Vec2& previousPosition, int button);

    float velocityScale = 15.0f;
    float velocityRadius = 14.0f;
    float densityRadius = 8.0f;
    float densityOuterRadius = 20.0f;
    tc::Color primaryColor = tc::Color(0.85f, 0.98f, 1.0f, 0.9f);
    tc::Color secondaryColor = tc::Color(0.0f, 0.4f, 1.0f, 1.0f);
};

} // namespace tcx::flow
