#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void windowResized(int width, int height) override;

private:
    void resizeSystems();
    void configureParticles();
    void injectFluid(float time);
    std::string modeName() const;

    tcx::flow::Fluid2D fluid_;
    tcx::flow::ParticleFlow particles_;
    tcx::flow::ParticleFlowVariant variant_ = tcx::flow::ParticleFlowVariant::Flow;
    bool showFluid_ = true;
};
