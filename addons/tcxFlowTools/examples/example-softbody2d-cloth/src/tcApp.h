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
    void mousePressed(tc::Vec2 pos, int button) override;
    void mouseReleased(tc::Vec2 pos, int button) override;
    void mouseDragged(tc::Vec2 pos, int button) override;
    void windowResized(int width, int height) override;

private:
    struct ClothView {
        tcx::flow::SoftBody2DGrid grid;
        tc::Color materialColor;
        tc::Color particleColor;
    };

    void rebuild();
    void applyWind(float time);
    void drawClothFill(const ClothView& cloth);
    void drawConstraints();
    void drawParticles();

    tcx::flow::SoftBody2D softBody_;
    std::vector<ClothView> cloths_;
    int grabbedParticle_ = -1;
    bool paused_ = false;
    bool showParticles_ = true;
    bool showBend_ = false;
    float windStrength_ = 28.0f;
};
