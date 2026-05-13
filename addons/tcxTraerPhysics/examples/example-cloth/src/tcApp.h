#pragma once

#include <TrussC.h>
#include <tcxTraerPhysics.h>
#include <memory>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mouseDragged(tc::Vec2 pos, int button) override;

private:
    tcx::ParticleSystem::Ptr ps_;

    static constexpr int GRID_W = 20;
    static constexpr int GRID_H = 15;
    static constexpr float REST = 20.0f;

    std::shared_ptr<tcx::Particle> grid_[GRID_W][GRID_H];
};
