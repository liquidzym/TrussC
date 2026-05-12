#pragma once

#include "BridgeFlow.h"

namespace tcx::flow {

class TemperatureBridge : public BridgeFlow {
public:
    void applyTo(Fluid2D& fluid) override;
};

} // namespace tcx::flow
