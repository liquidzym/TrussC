#include "tcApp.h"

#include "../../common/ExampleControls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

constexpr int kParticleCount = 1000;
constexpr float kBoundsPad = 14.0f;
constexpr float kRestitution = 0.10f;
constexpr float kParticleFillFactor = 0.60f;

float hash01(int value) {
    unsigned int x = static_cast<unsigned int>(value) * 747796405u + 2891336453u;
    x = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    x = (x >> 22u) ^ x;
    return static_cast<float>(x & 0xffffu) / 65535.0f;
}

float clampf(float value, float lo, float hi) {
    return std::max(lo, std::min(value, hi));
}

tc::Vec2 safeNormal(const tc::Vec2& v, const tc::Vec2& fallback = tc::Vec2(1, 0)) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) return fallback;
    return v * (1.0f / len);
}

} // namespace

void tcApp::setup() {
    resizeSystems();
    previousMouse_ = tc::getMousePos();
}

void tcApp::update() {
    const float dt = std::min(1.0f / 30.0f, static_cast<float>(tc::getDeltaTime()));
    const float time = tc::getElapsedTimef();

    injectFluid(time);

    const tc::Vec2 mouse = tc::getMousePos();
    if (tc::isMousePressed() && activeMouseButton_ == 0) {
        if (!wasMousePressed_) {
            previousMouse_ = mouse;
        }
        const tc::Vec2 delta = mouse - previousMouse_;
        if (delta.length() > 0.0f) {
            const int steps = std::max(1, std::min(96, static_cast<int>(std::ceil(delta.length() / 2.0f))));
            const tc::Vec2 segmentVelocity = delta * (20.0f / static_cast<float>(steps));
            for (int i = 1; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const tc::Vec2 p = previousMouse_ + delta * t;
                fluid_.addVelocity(p, 24.0f, segmentVelocity);
            }
            fluid_.addDensity(mouse, 13.0f, tc::Color(0.78f, 0.84f, 0.90f, 0.10f));
            for (auto& particle : particles_) {
                const tc::Vec2 toParticle = particle.position - mouse;
                const float d = toParticle.length();
                const float k = std::max(0.0f, 1.0f - d / 150.0f);
                particle.previous -= delta * (k * k * 1.35f);
                particle.acceleration += delta * (k * k * 24.0f);
            }
        }
        previousMouse_ = mouse;
        wasMousePressed_ = true;
    } else {
        previousMouse_ = mouse;
        wasMousePressed_ = false;
    }

    fluid_.update(dt);
    fluid_.refreshVelocityReadback();
    updateParticles(dt, time);
    ++frame_;
}

void tcApp::draw() {
    tc::clear(0.0f, 0.0f, 0.0f);
    if (showFluid_) {
        fluid_.drawCombined(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    }
    drawObstacles();
    drawParticles();

    tc::setColor(1.0f);
    tc::drawBitmapString("fluid-verlet-collision | drag inject | middle grab | c collision | f fluid | r reset",
                         18, 28, tcx::flow::example::kHudScale);
    tc::drawBitmapString("PixelFlow Fluid_VerletParticleCollisionSystem parity target | " +
                             tc::toString(static_cast<int>(particles_.size())) + " particles | " +
                             (collisionsEnabled_ ? "collision on" : "collision off"),
                         18, 28 + tcx::flow::example::kHudLine,
                         tcx::flow::example::kHudScale);
}

void tcApp::keyPressed(int key) {
    using tcx::flow::example::keyIs;
    if (keyIs(key, 'r')) {
        fluid_.reset();
        resetParticles();
        frame_ = 0;
    }
    if (keyIs(key, 'f')) showFluid_ = !showFluid_;
    if (keyIs(key, 'c')) collisionsEnabled_ = !collisionsEnabled_;
}

void tcApp::mousePressed(tc::Vec2 pos, int button) {
    previousMouse_ = pos;
    activeMouseButton_ = button;
    wasMousePressed_ = true;
    grabbedParticle_ = -1;
    if (button != 2) return;
    float best = 34.0f;
    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        const float d = (particles_[i].position - pos).length();
        if (d < best) {
            best = d;
            grabbedParticle_ = i;
        }
    }
    if (grabbedParticle_ >= 0) {
        particles_[grabbedParticle_].grabbed = true;
        particles_[grabbedParticle_].position = pos;
        particles_[grabbedParticle_].previous = pos;
    }
}

void tcApp::mouseReleased(tc::Vec2 pos, int button) {
    (void)pos;
    (void)button;
    if (grabbedParticle_ >= 0 && grabbedParticle_ < static_cast<int>(particles_.size())) {
        particles_[grabbedParticle_].grabbed = false;
    }
    grabbedParticle_ = -1;
    activeMouseButton_ = -1;
}

void tcApp::mouseDragged(tc::Vec2 pos, int button) {
    if (button == 2 && grabbedParticle_ >= 0 && grabbedParticle_ < static_cast<int>(particles_.size())) {
        particles_[grabbedParticle_].position = pos;
        particles_[grabbedParticle_].previous = pos;
    }
}

void tcApp::windowResized(int width, int height) {
    (void)width;
    (void)height;
    resizeSystems();
}

void tcApp::resizeSystems() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.5f;
    settings.outputResolutionScale = 1.0f;
    settings.timestep = 0.125f;
    settings.solverIterations = 40;
    settings.enableVorticity = true;
    settings.vorticity = 0.42f;
    settings.enableTemperature = true;
    settings.enableBuoyancy = true;
    settings.buoyancy = 0.32f;
    settings.densityWeight = 0.015f;
    settings.velocityDissipation = 0.996f;
    settings.densityDissipation = 0.990f;
    settings.temperatureDissipation = 0.52f;
    settings.viscosity = 0.0030f;
    fluid_.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
    configureObstacles();
    resetParticles();
}

void tcApp::resetParticles() {
    particles_.clear();
    particles_.reserve(kParticleCount);
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float baseRadius = std::max(1.0f, std::sqrt((w * h * kParticleFillFactor) /
                                                      static_cast<float>(kParticleCount)) * 0.5f);
    const float rMin = baseRadius * 0.5f;
    const float rMax = baseRadius * 1.5f;
    for (int i = 0; i < kParticleCount; ++i) {
        Particle p;
        p.radius = rMin + hash01(i * 7 + 3) * (rMax - rMin);
        if (i == 0) p.radius = rMax * 1.5f;
        p.mass = std::max(0.25f, (rMax * rMax) / std::max(1.0f, p.radius * p.radius));
        p.position = tc::Vec2(kBoundsPad + p.radius + hash01(i * 5 + 1) * std::max(1.0f, w - (kBoundsPad + p.radius) * 2.0f),
                              kBoundsPad + p.radius + hash01(i * 5 + 2) * std::max(1.0f, h - (kBoundsPad + p.radius) * 2.0f));
        p.position.x = clampf(p.position.x, kBoundsPad + p.radius, w - kBoundsPad - p.radius);
        p.position.y = clampf(p.position.y, kBoundsPad + p.radius, h - kBoundsPad - p.radius);
        const tc::Vec2 kick((hash01(i * 9 + 6) - 0.5f) * 1.8f,
                            (hash01(i * 9 + 7) - 0.5f) * 1.4f);
        p.previous = p.position - kick;
        p.color = particleColorFor(hash01(i * 13 + 9), p.radius);
        particles_.push_back(p);
    }
    grabbedParticle_ = -1;
}

void tcApp::configureObstacles() {
    obstacles_.clear();
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float r = std::min(w, h) * 0.030f;
    obstacles_.push_back({tc::Vec2(w * 0.43f, h * 0.53f), r * 1.05f, true});
    obstacles_.push_back({tc::Vec2(w * 0.77f, h * 0.42f), r * 1.15f, true});
    obstacles_.push_back({tc::Vec2(w * 0.25f, h * 0.72f), r * 1.65f, false});

    fluid_.clearObstacles();
    fluid_.addObstacle(tc::Vec2(w * 0.43f, h * 0.53f), r * 1.35f);
    fluid_.addObstacle(tc::Vec2(w * 0.77f, h * 0.42f), r * 1.45f);
    fluid_.addObstacle(tc::Vec2(w * 0.25f, h * 0.72f), r * 1.90f);
}

void tcApp::injectFluid(float time) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const tc::Vec2 topRight(w - 84.0f, 34.0f);
    const tc::Vec2 center(w * 0.50f, h * 0.50f);
    const tc::Vec2 upper(w * 0.54f + std::sin(time * 0.7f) * 36.0f, h * 0.11f);

    fluid_.addDensity(topRight, 50.0f, tc::Color(0.56f, 0.58f, 0.62f, 0.26f));
    fluid_.addTemperature(topRight, 66.0f, 4.4f);
    fluid_.addVelocity(topRight, 58.0f, tc::Vec2(-32.0f, 90.0f));

    fluid_.addDensity(center, 18.0f, tc::Color(0.66f, 0.68f, 0.72f, 0.25f));
    fluid_.addVelocity(center, 45.0f, tc::Vec2(std::sin(time * 1.1f) * 60.0f, -96.0f));

    fluid_.addDensity(upper, 22.0f, tc::Color(0.95f, 0.52f, 0.18f, 0.24f));
    fluid_.addVelocity(upper, 52.0f, tc::Vec2(-105.0f, 22.0f));

}

void tcApp::updateParticles(float dt, float time) {
    const float step = std::max(0.008f, std::min(0.022f, dt));
    applyParticleForces(time);

    for (auto& p : particles_) {
        if (p.grabbed) continue;
        const tc::Vec2 velocity = (p.position - p.previous) * 0.976f;
        const tc::Vec2 next = p.position + velocity + p.acceleration * (step * step * 260.0f);
        p.previous = p.position;
        p.position = next;
        p.acceleration = tc::Vec2(0, 0);
    }

    const int iterations = collisionsEnabled_ ? 4 : 1;
    for (int i = 0; i < iterations; ++i) {
        for (auto& p : particles_) {
            collideObstacles(p);
            satisfyBounds(p);
        }
        if (collisionsEnabled_) {
            collideParticles();
        }
    }
}

void tcApp::applyParticleForces(float time) {
    (void)time;
    for (auto& p : particles_) {
        if (p.grabbed) continue;
        const tc::Vec2 field = fluid_.sampleVelocityAtPosition(p.position);
        p.acceleration += field * (0.16f / std::max(0.25f, p.mass));
        p.acceleration += tc::Vec2(0.0f, 0.65f);
    }
}

void tcApp::satisfyBounds(Particle& particle) {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    const float left = kBoundsPad + particle.radius;
    const float right = w - kBoundsPad - particle.radius;
    const float top = kBoundsPad + particle.radius;
    const float bottom = h - kBoundsPad - particle.radius;
    if (particle.position.x < left || particle.position.x > right) {
        const float clamped = clampf(particle.position.x, left, right);
        particle.previous.x = clamped + (particle.position.x - particle.previous.x) * kRestitution;
        particle.position.x = clamped;
    }
    if (particle.position.y < top || particle.position.y > bottom) {
        const float clamped = clampf(particle.position.y, top, bottom);
        particle.previous.y = clamped + (particle.position.y - particle.previous.y) * kRestitution;
        particle.position.y = clamped;
    }
}

void tcApp::collideParticles() {
    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(particles_.size()); ++j) {
            const tc::Vec2 delta = particles_[j].position - particles_[i].position;
            const float minDist = particles_[i].radius + particles_[j].radius;
            if (std::abs(delta.x) > minDist || std::abs(delta.y) > minDist) continue;
            const float distSq = delta.x * delta.x + delta.y * delta.y;
            if (distSq < minDist * minDist) {
                collideParticlePair(particles_[i], particles_[j]);
            }
        }
    }
}

void tcApp::collideParticlePair(Particle& a, Particle& b) {
    const tc::Vec2 delta = b.position - a.position;
    const float minDist = a.radius + b.radius;
    const float dist = std::max(0.001f, std::sqrt(delta.x * delta.x + delta.y * delta.y));
    const tc::Vec2 normal = delta * (1.0f / dist);
    const float overlap = (minDist - dist) * 0.52f;
    const float totalMass = a.mass + b.mass;
    const float aw = b.mass / totalMass;
    const float bw = a.mass / totalMass;
    if (!a.grabbed) a.position -= normal * (overlap * aw);
    if (!b.grabbed) b.position += normal * (overlap * bw);
}

void tcApp::collideObstacles(Particle& particle) {
    for (const auto& obstacle : obstacles_) {
        const tc::Vec2 delta = particle.position - obstacle.position;
        const float minDist = particle.radius + obstacle.radius;
        const float distSq = delta.x * delta.x + delta.y * delta.y;
        if (distSq < minDist * minDist) {
            const tc::Vec2 normal = safeNormal(delta);
            particle.position = obstacle.position + normal * minDist;
            const tc::Vec2 velocity = particle.position - particle.previous;
            const float vn = velocity.x * normal.x + velocity.y * normal.y;
            if (vn < 0.0f) {
                particle.previous = particle.position - (velocity - normal * (1.65f * vn));
            }
        }
    }
}

void tcApp::drawObstacles() const {
    const float w = static_cast<float>(tc::getWindowWidth());
    const float h = static_cast<float>(tc::getWindowHeight());
    tc::setColor(0.78f, 0.80f, 0.78f, 0.68f);
    tc::drawRect(0, 0, w, kBoundsPad);
    tc::drawRect(0, h - kBoundsPad, w, kBoundsPad);
    tc::drawRect(0, 0, kBoundsPad, h);
    tc::drawRect(w - kBoundsPad, 0, kBoundsPad, h);
    for (const auto& obstacle : obstacles_) {
        tc::setColor(obstacle.column ? tc::Color(0.86f, 0.88f, 0.86f, 0.72f)
                                     : tc::Color(1.0f, 0.78f, 0.18f, 0.72f));
        tc::drawCircle(obstacle.position.x, obstacle.position.y, obstacle.radius);
        if (obstacle.column) {
            tc::drawRect(obstacle.position.x - obstacle.radius * 0.32f,
                         obstacle.position.y - obstacle.radius * 3.8f,
                         obstacle.radius * 0.64f,
                         obstacle.radius * 7.6f);
        }
    }
}

void tcApp::drawParticles() const {
    for (const auto& p : particles_) {
        const tc::Color shadow(0.0f, 0.0f, 0.0f, 0.30f);
        tc::setColor(shadow);
        tc::drawCircle(p.position.x + p.radius * 0.10f, p.position.y + p.radius * 0.18f, p.radius * 1.04f);
        tc::setColor(p.color);
        tc::drawCircle(p.position.x, p.position.y, p.radius);
        tc::setColor(1.0f, 1.0f, 1.0f, p.color.a * 0.18f);
        tc::drawCircle(p.position.x - p.radius * 0.25f, p.position.y - p.radius * 0.32f, p.radius * 0.34f);
    }
}

tc::Color tcApp::particleColorFor(float u, float radius) const {
    if (radius > 9.0f) return tc::Color(1.0f, 0.58f, 0.12f, 0.92f);
    if (u < 0.46f) return tc::Color(1.0f, 0.42f, 0.08f, 0.84f);
    if (u < 0.90f) return tc::Color(0.34f, 0.66f, 1.0f, 0.86f);
    return tc::Color(0.86f, 0.86f, 0.90f, 0.74f);
}
