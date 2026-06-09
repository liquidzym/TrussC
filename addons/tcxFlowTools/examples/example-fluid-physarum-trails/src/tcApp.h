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
    enum class ViewMode {
        Fluid = 0,
        Pressure = 1,
        Velocity = 2
    };

    struct StrokeParticle {
        tc::Vec2 position;
        tc::Vec2 previous;
        float age = 0.0f;
        float lifetime = 1.0f;
        float seed = 0.0f;
        float shade = 1.0f;
    };

    void resizeSystems();
    void resetSystems();
    void resetStrokeParticles();
    void injectAutonomousFlow(float time);
    void handlePointerInput();
    void injectPointerSegment(const tc::Vec2& current, const tc::Vec2& previous, int button);
    void updateStrokeParticles(float dt);
    void drawStrokeParticles() const;
    void respawnStrokeParticle(StrokeParticle& particle, int index);
    void drawFluidView();
    void updateTrailBuffer();
    void clearTrailBuffer();
    void applyTrailPreset();
    bool gpuTrailActive() const;
    std::string viewName() const;

    tcx::flow::Fluid2D fluid_;
    tcx::flow::PhysarumTrailFlow gpuTrail_;
    tcx::flow::FlowVisualizer visualizer_;
    std::vector<StrokeParticle> strokeParticles_;
    tc::Fbo trailFbo_;
    tc::Vec2 previousMouse_;
    ViewMode viewMode_ = ViewMode::Fluid;
    int activeMouseButton_ = -1;
    int trailPreset_ = 1;
    int particleCount_ = 100000;
    float flowRangeScale_ = 0.62f;
    float flowStrengthScale_ = 0.86f;
    float fps_ = 0.0f;
    float frameMs_ = 0.0f;
    mutable int lastLineBatches_ = 0;
    mutable int lastPointBatches_ = 0;
    mutable int lastTrailVertices_ = 0;
    bool paused_ = false;
    bool autoFlow_ = false;
    bool useGpuTrail_ = true;
    bool showParticles_ = true;
    bool injectOnMove_ = true;
    bool showHud_ = true;
    bool trailNeedsClear_ = true;
};
