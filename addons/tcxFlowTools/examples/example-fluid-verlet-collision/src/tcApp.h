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
    struct Particle {
        tc::Vec2 position;
        tc::Vec2 previous;
        tc::Vec2 acceleration;
        float radius = 6.0f;
        float mass = 1.0f;
        tc::Color color;
        bool grabbed = false;
    };

    struct Obstacle {
        tc::Vec2 position;
        float radius = 24.0f;
        bool column = false;
    };

    void resizeSystems();
    void resetParticles();
    void configureObstacles();
    void injectFluid(float time);
    void updateParticles(float dt, float time);
    void applyParticleForces(float time);
    void satisfyBounds(Particle& particle);
    void collideParticles();
    void collideParticlePair(Particle& a, Particle& b);
    void collideObstacles(Particle& particle);
    void drawObstacles() const;
    void drawParticles() const;
    tc::Vec2 proceduralFluidVelocity(const tc::Vec2& position, float time) const;
    tc::Color particleColorFor(float u, float radius) const;

    tcx::flow::Fluid2D fluid_;
    std::vector<Particle> particles_;
    std::vector<Obstacle> obstacles_;
    tc::Vec2 previousMouse_;
    int grabbedParticle_ = -1;
    bool showFluid_ = true;
    bool collisionsEnabled_ = true;
    bool wasMousePressed_ = false;
    int frame_ = 0;
};
