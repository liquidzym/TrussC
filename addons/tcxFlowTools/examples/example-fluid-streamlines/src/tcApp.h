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
    enum class BackgroundMode {
        Combined,
        Density,
        Temperature,
        Velocity,
        Lic,
        None
    };

    struct Obstacle {
        tc::Vec2 position;
        float radius = 1.0f;
    };

    void resizeSystems();
    void configureObstacles();
    void resetStreamSeeds();
    void injectReferenceSources(float time);
    void handleMouseInput();
    void setStreamPreset(int preset);
    void cycleBackgroundMode();
    int streamSpacing() const;
    int streamSamples() const;
    float streamStepScale() const;
    const char* backgroundModeName() const;
    void drawBackground() const;
    void drawObstacles() const;
    void drawVelocityVectors() const;
    void drawStreamParticles() const;
    void drawStreamlines() const;
    void drawStreamline(const tc::Vec2& seed, int direction, int seedIndex) const;
    tc::Color streamlineColor(float t, float speed, int seedIndex, int direction) const;

    tcx::flow::Fluid2D fluid_;
    std::vector<tc::Vec2> streamSeeds_;
    std::vector<Obstacle> obstacles_;
    tc::Vec2 previousMouse_;
    int activeMouseButton_ = -1;
    int streamPreset_ = 0;
    float lineLength_ = 150.0f;
    BackgroundMode backgroundMode_ = BackgroundMode::Lic;
    bool wasMousePressed_ = false;
    bool showStreamlines_ = true;
    bool showVelocityVectors_ = false;
    bool showStreamParticles_ = false;
    bool paused_ = false;
    bool velocityReadbackReady_ = false;
};
