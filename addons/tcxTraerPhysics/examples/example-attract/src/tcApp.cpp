#include "tcApp.h"
#include <cmath>

using namespace trussc;

namespace {
tc::Vec3 circularOrbitVelocity(const tc::Vec3& center,
                               const tc::Vec3& position,
                               float strength,
                               float centerMass)
{
    const float dx = position.x - center.x;
    const float dy = position.y - center.y;
    const float radius = std::sqrt(dx * dx + dy * dy);
    if (radius <= 0.0001f) return tc::Vec3(0, 0, 0);

    const float speed = std::sqrt(strength * centerMass / radius);
    const float invRadius = 1.0f / radius;
    return tc::Vec3(-dy * invRadius * speed, dx * invRadius * speed, 0);
}

tc::Vec3 orbitPositionOrFallback(const tc::Vec3& center, tc::Vec2 requested, float minRadius)
{
    const float dx = requested.x - center.x;
    const float dy = requested.y - center.y;
    const float radius = std::sqrt(dx * dx + dy * dy);
    if (radius >= minRadius) return tc::Vec3(requested.x, requested.y, 0);
    return tc::Vec3(center.x + minRadius, center.y, 0);
}
} // namespace

void tcApp::setup() {
    setWindowTitle("TraerPhysics — N-body Attraction");
    setIndependentFps(60.0f, 0.0f);

    planets_.clear();

    // No gravity, no drag — pure attraction
    ps_ = tcx::ParticleSystem::create(0.0f, 0.0f);
    ps_->reserve(1 + MAX_PLANETS, 0, MAX_PLANETS);
    planets_.reserve(MAX_PLANETS);

    // Central sun (fixed, large mass)
    float cx = getWindowWidth()  / 2.0f;
    float cy = getWindowHeight() / 2.0f;
    sun_ = ps_->makeParticle(SUN_MASS, cx, cy, 0);
    sun_->makeFixed();

    // Create orbiting planets
    for (int i = 0; i < 5; i++) {
        float angle = (float)i / 5.0f * TAU;
        float radius = 100.0f + i * 40.0f;
        float px = cx + cosf(angle) * radius;
        float py = cy + sinf(angle) * radius;
        auto p = ps_->makeParticle(1.0f, px, py, 0);

        // For Traer attraction: a = strength * sunMass / r^2, so v = sqrt(strength * sunMass / r).
        p->velocity = circularOrbitVelocity(sun_->position, p->position, ATTRACTION_STRENGTH, SUN_MASS);

        ps_->makeAttraction(sun_, p, ATTRACTION_STRENGTH, 10.0f);
        planets_.push_back(p);
    }
}

void tcApp::update() {
    ps_->tick();  // default dt=1.0, RK4 default
    redraw();
}

void tcApp::draw() {
    clear(0.05f);

    // Attraction lines
    setColor(0.15f, 0.2f, 0.3f);
    for (auto& p : planets_) {
        drawLine(sun_->position.x, sun_->position.y,
                 p->position.x, p->position.y);
    }

    // Planets
    for (auto& p : planets_) {
        setColor(0.5f, 0.7f, 1.0f);
        fill();
        drawCircle(p->position.x, p->position.y, 5);
    }

    // Sun
    setColor(1.0f, 0.8f, 0.2f);
    fill();
    drawCircle(sun_->position.x, sun_->position.y, 14);

    // HUD
    setColor(0.7f);
    std::string info = "Planets: " + std::to_string(planets_.size());
    info += " | Click to add | [R] reset | [C] clear";
    drawBitmapString(info, 12, 16);
}

void tcApp::keyPressed(int key) {
    if (key == 'R') {
        setup();
    }
    if (key == 'C') {
        for (auto& p : planets_) {
            ps_->removeParticleAndForces(p);
        }
        planets_.clear();
    }
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    (void)button;
    if ((int)planets_.size() >= MAX_PLANETS) return;

    const tc::Vec3 position = orbitPositionOrFallback(sun_->position, pos, MIN_ORBIT_RADIUS);
    auto p = ps_->makeParticle(1.0f, position);
    p->velocity = circularOrbitVelocity(sun_->position, p->position, ATTRACTION_STRENGTH, SUN_MASS);

    ps_->makeAttraction(sun_, p, ATTRACTION_STRENGTH, 10.0f);
    planets_.push_back(p);
}
