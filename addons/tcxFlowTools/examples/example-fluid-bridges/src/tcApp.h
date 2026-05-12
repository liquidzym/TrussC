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

    tcx::flow::Fluid2D fluid_;
    tcx::flow::VelocityBridge velocityBridge_;
    tcx::flow::DensityBridge densityBridge_;
    tcx::flow::TemperatureBridge temperatureBridge_;
    tcx::flow::CombinedBridge combinedBridge_;
    int mode_ = 4;
};
