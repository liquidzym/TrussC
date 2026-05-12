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
    void allocateBuffer();
    void paintTargets();
    void runCommonPassPreview();

    tcx::flow::PingPongBuffer buffer_;
    tc::Fbo copyPreview_;
    tc::Fbo clearPreview_;
    tcx::flow::FlowPass copyPass_;
    tcx::flow::FlowPass clearPass_;
    int bufferWidth_ = 320;
    int bufferHeight_ = 180;
    int frameCounter_ = 0;
};
