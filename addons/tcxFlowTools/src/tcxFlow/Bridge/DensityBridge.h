#pragma once

#include "BridgeFlow.h"

namespace tcx::flow {

class DensityBridge : public BridgeFlow {
public:
    using BridgeFlow::update;
    void update(const tc::Texture& input, float dt) override;
    void applyTo(Fluid2D& fluid) override;
    tc::Color color = tc::Color(0.2f, 0.7f, 1.0f, 1.0f);
};

} // namespace tcx::flow
