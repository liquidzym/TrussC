#pragma once

#include "../Core/FlowTypes.h"

namespace tcx::flow {

class Fluid2D;

class BridgeFlow {
public:
    virtual ~BridgeFlow() = default;
    virtual void setup(int width, int height);
    virtual void resize(int width, int height);
    virtual void update(const tc::Texture& input, float dt);
    virtual void update(float dt);
    virtual void applyTo(Fluid2D& fluid);

    BridgeSettings& settings() { return settings_; }
    const BridgeSettings& settings() const { return settings_; }
    int width() const { return width_; }
    int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    float age_ = 0.0f;
    BridgeSettings settings_;
};

} // namespace tcx::flow
