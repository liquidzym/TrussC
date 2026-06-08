#pragma once

#include <TrussC.h>
#include <tcxFlowTools.h>

#include <string>
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
    enum class FluidView {
        Density,
        Temperature,
        Pressure,
        Velocity,
        Combined
    };

    struct Obstacle {
        tc::Vec2 position;
        float radius = 1.0f;
    };

    void resizeSystems();
    void configureObstacles();
    void injectReferenceSource(float time);
    void handleMouseInput();
    void drawFluidView() const;
    void drawObstacles() const;
    void cycleGridScale(int direction);
    std::string viewName() const;

    tcx::flow::Fluid2D fluid_;
    tcx::flow::ParticleFlow particles_;
    std::vector<Obstacle> obstacles_;
    tc::Vec2 previousMouse_;
    int activeMouseButton_ = -1;
    int gridPreset_ = 1;
    FluidView fluidView_ = FluidView::Density;
    bool paused_ = false;
    bool showFluid_ = false;
    bool wasMousePressed_ = false;
};
