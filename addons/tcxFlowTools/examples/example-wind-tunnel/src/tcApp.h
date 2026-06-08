#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

#include <vector>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void windowResized(int width, int height) override;

private:
    void resizeSystems();
    void rebuildObstacles();
    void injectWind(float time);
    void updateWindField(float time);
    void updateDensitySource(float time);
    void drawObstacles() const;

    tcx::flow::Fluid2D fluid_;
    tcx::flow::FlowVisualizer visualizer_;
    std::vector<tc::Vec2> windField_;
    std::vector<float> densitySourcePixels_;
    tc::Texture densitySourceTexture_;
    int windFieldWidth_ = 128;
    int windFieldHeight_ = 72;
    int sourceWidth_ = 192;
    int sourceHeight_ = 108;
    tcx::flow::FlowVisualizer::Mode mode_ = tcx::flow::FlowVisualizer::Mode::Density;
    bool showVelocity_ = false;
    bool mouseObstacle_ = false;
    float windStrength_ = 1.0f;
};
