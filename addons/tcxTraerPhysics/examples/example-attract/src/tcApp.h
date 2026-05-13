#pragma once

#include <TrussC.h>
#include <tcxTraerPhysics.h>
#include <memory>
#include <vector>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mousePressed(tc::Vec2 pos, int button) override;

private:
    tcx::ParticleSystem::Ptr ps_;
    std::shared_ptr<tcx::Particle> sun_;
    std::vector<std::shared_ptr<tcx::Particle>> planets_;
    static constexpr int MAX_PLANETS = 12;
    static constexpr float SUN_MASS = 500.0f;
    static constexpr float ATTRACTION_STRENGTH = 5.0f;
    static constexpr float MIN_ORBIT_RADIUS = 40.0f;
};
