#pragma once

#include "VelocityBridge.h"
#include "DensityBridge.h"
#include "TemperatureBridge.h"

namespace tcx::flow {

class CombinedBridge : public BridgeFlow {
public:
    void setup(int width, int height) override;
    void resize(int width, int height) override;
    void update(const tc::Texture& input, float dt) override;
    void update(float dt) override;
    void applyTo(Fluid2D& fluid) override;

    bool enableVelocity = true;
    bool enableDensity = true;
    bool enableTemperature = true;

private:
    VelocityBridge velocity_;
    DensityBridge density_;
    TemperatureBridge temperature_;
};

} // namespace tcx::flow
