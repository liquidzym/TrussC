#include "tcApp.h"

using namespace trussc;

void tcApp::setup() {
    setWindowTitle("TraerPhysics — Spring Pendulum");
    setIndependentFps(60.0f, 0.0f);

    // Parameters in original TraerPhysics range
    ps_ = tcx::ParticleSystem::create(0.5f, 0.001f);
    ps_->reserve(2, 1);

    // Anchor (fixed)
    anchor_ = ps_->makeParticle(1.0f, getWindowWidth() / 2.0f, 100, 0);
    anchor_->makeFixed();

    // Bob (hanging)
    bob_ = ps_->makeParticle(3.0f, getWindowWidth() / 2.0f, 300, 0);

    // Spring: stiffness 0.2, damping 0.01, rest 150
    spring_ = ps_->makeSpring(anchor_, bob_, 0.2f, 0.01f, 150.0f);
}

void tcApp::update() {
    ps_->tick();  // default dt=1.0
    redraw();
}

void tcApp::draw() {
    clear(0.12f);

    setColor(1.0f, 1.0f, 0.3f);
    drawLine(anchor_->position.x, anchor_->position.y,
             bob_->position.x, bob_->position.y);

    setColor(1.0f);
    fill();
    drawCircle(anchor_->position.x, anchor_->position.y, 8);

    setColor(1.0f, 0.5f, 0.2f);
    fill();
    drawCircle(bob_->position.x, bob_->position.y, 16);

    setColor(0.7f);
    drawBitmapString("Click to grab bob | [R] reset | [1]RK4 [2]Euler [3]ModEuler", 12, 16);
}

void tcApp::keyPressed(int key) {
    if (key == 'R') {
        bob_->position = tc::Vec3(getWindowWidth() / 2.0f, 300, 0);
        bob_->velocity = tc::Vec3(0, 0, 0);
    }
    if (key == '1') ps_->setIntegrator(tcx::ParticleSystem::IntegratorType::RungeKutta);
    if (key == '2') ps_->setIntegrator(tcx::ParticleSystem::IntegratorType::Euler);
    if (key == '3') ps_->setIntegrator(tcx::ParticleSystem::IntegratorType::ModifiedEuler);
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    (void)button;
    bob_->position = tc::Vec3(pos.x, pos.y, 0);
    bob_->velocity = tc::Vec3(0, 0, 0);
}
