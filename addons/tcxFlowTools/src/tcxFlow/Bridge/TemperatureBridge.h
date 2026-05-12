#pragma once

#include "BridgeFlow.h"

namespace tcx::flow {

class TemperatureBridge : public BridgeFlow {
public:
    using BridgeFlow::update;
    void update(const tc::Texture& input, float dt) override;
    void applyTo(Fluid2D& fluid) override;
};

} // namespace tcx::flow
