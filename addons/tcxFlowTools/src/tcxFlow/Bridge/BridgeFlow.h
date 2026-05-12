#pragma once

#include "../Core/FlowPass.h"
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
    bool hasTextureOutput() const { return textureOutputValid_; }
    const tc::Texture* outputTexture() const;

protected:
    bool renderTextureOutput(FlowPassKind kind, const tc::Texture& input,
                             const tc::Color& color, float gain, float threshold,
                             float radius = 1.0f, float extra = 0.0f);
    void clearTextureOutput() { textureOutputValid_ = false; }

    int width_ = 0;
    int height_ = 0;
    float age_ = 0.0f;
    BridgeSettings settings_;
    tc::Fbo textureOutput_;
    FlowPass texturePass_;
    FlowPassKind texturePassKind_ = FlowPassKind::Unknown;
    bool textureOutputValid_ = false;
};

} // namespace tcx::flow
