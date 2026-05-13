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
    void mousePressed(tc::Vec2 pos, int button) override;

private:
    tcx::ParticleSystem::Ptr ps_;
    std::shared_ptr<tcx::Particle> anchor_;
    std::shared_ptr<tcx::Particle> bob_;
    std::shared_ptr<tcx::Spring> spring_;
};
