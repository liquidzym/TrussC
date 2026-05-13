#include "tcApp.h"

using namespace trussc;

void tcApp::setup() {
    setWindowTitle("TraerPhysics — Cloth");
    setIndependentFps(60.0f, 0.0f);

    // Parameters in original TraerPhysics range (gravity 0-5, stiffness 0.1-0.5)
    ps_ = tcx::ParticleSystem::create(0.2f, 0.001f);
    ps_->setIntegrator(tcx::ParticleSystem::IntegratorType::ModifiedEuler);

    constexpr int particleCount = GRID_W * GRID_H;
    constexpr int horizontalSprings = (GRID_W - 1) * GRID_H;
    constexpr int verticalSprings = GRID_W * (GRID_H - 1);
    constexpr int diagonalSprings = (GRID_W - 1) * (GRID_H - 1) * 2;
    ps_->reserve(particleCount, horizontalSprings + verticalSprings + diagonalSprings);

    float ox = (getWindowWidth()  - GRID_W * REST) / 2.0f;
    float oy = 60.0f;

    // Create grid of particles
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            auto p = ps_->makeParticle(1.0f, ox + x * REST, oy + y * REST, 0);
            grid_[x][y] = p;
        }
    }

    // Pin entire top row
    for (int x = 0; x < GRID_W; x++) {
        grid_[x][0]->makeFixed();
    }

    // Connect horizontal springs
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W - 1; x++) {
            ps_->makeSpring(grid_[x][y], grid_[x + 1][y], 0.3f, 0.05f, REST);
        }
    }

    // Connect vertical springs
    for (int y = 0; y < GRID_H - 1; y++) {
        for (int x = 0; x < GRID_W; x++) {
            ps_->makeSpring(grid_[x][y], grid_[x][y + 1], 0.3f, 0.05f, REST);
        }
    }

    // Connect diagonal springs (for shear resistance)
    float diagRest = REST * 1.414f;
    for (int y = 0; y < GRID_H - 1; y++) {
        for (int x = 0; x < GRID_W - 1; x++) {
            ps_->makeSpring(grid_[x][y], grid_[x + 1][y + 1], 0.15f, 0.03f, diagRest);
            ps_->makeSpring(grid_[x + 1][y], grid_[x][y + 1], 0.15f, 0.03f, diagRest);
        }
    }
}

void tcApp::update() {
    ps_->tick();
    redraw();
}

void tcApp::draw() {
    clear(0.12f);

    // Draw springs as lines
    setColor(0.4f, 0.6f, 0.9f, 0.6f);
    for (const auto& s : ps_->getSprings()) {
        auto a = s->getOneEnd();
        auto b = s->getTheOtherEnd();
        drawLine(a->position.x, a->position.y,
                 b->position.x, b->position.y);
    }

    // Draw particles
    setColor(1.0f);
    fill();
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            drawCircle(grid_[x][y]->position.x, grid_[x][y]->position.y, 3);
        }
    }

    // HUD
    setColor(0.7f);
    drawBitmapString("Drag to tear cloth | [R] reset | [Space] wind | [1] RK4 [2] ModEuler", 12, 16);
}

void tcApp::keyPressed(int key) {
    if (key == KEY_SPACE) {
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                grid_[x][y]->force.x += (rand() % 2000 - 1000) * 0.01f;
            }
        }
    }
    if (key == 'R') {
        setup();
    }
    if (key == '1') ps_->setIntegrator(tcx::ParticleSystem::IntegratorType::RungeKutta);
    if (key == '2') ps_->setIntegrator(tcx::ParticleSystem::IntegratorType::ModifiedEuler);
}

void tcApp::mouseDragged(tc::Vec2 pos, int button) {
    (void)button;
    for (int i = ps_->numSprings() - 1; i >= 0; i--) {
        auto s = ps_->getSpring(i);
        auto a = s->getOneEnd();
        auto b = s->getTheOtherEnd();
        float midX = (a->position.x + b->position.x) * 0.5f;
        float midY = (a->position.y + b->position.y) * 0.5f;
        float dx = pos.x - midX;
        float dy = pos.y - midY;
        if (dx * dx + dy * dy < 400.0f) {
            ps_->removeSpring(i);
        }
    }
}
