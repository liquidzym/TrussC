// =============================================================================
// physics_test.cpp - Standalone verification against the real addon headers
// =============================================================================
// Compile:
//   clang++ -std=c++17 -Itests/stubs -Isrc -o /tmp/tcxTraerPhysics_test tests/physics_test.cpp

#include "tcxTraerPhysics.h"

#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>

using namespace tcx;

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); failures++; } \
    else { std::printf("  PASS: %s\n", msg); } \
} while (0)

#define CHECK_NEAR(a, b, eps, msg) do { \
    float _a = (a), _b = (b); \
    if (std::fabs(_a - _b) > (eps)) { \
        std::printf("  FAIL: %s (got %.4f, expected %.4f, diff %.4f)\n", msg, _a, _b, std::fabs(_a - _b)); \
        failures++; \
    } else { std::printf("  PASS: %s\n", msg); } \
} while (0)

void test_gravity_force_semantics() {
    std::printf("\n--- Gravity Force Semantics ---\n");
    ParticleSystem ps(10.0f, 0.0f);
    ps.setIntegrator(std::make_unique<EulerIntegrator>(&ps));
    auto p = ps.makeParticle(2.0f, 0, 0, 0);
    ps.tick(1.0f);

    CHECK_NEAR(p->velocity.y, 5.0f, 0.001f, "Traer gravity is a force vector divided by mass");
    CHECK_NEAR(p->position.y, 0.0f, 0.001f, "Forward Euler updates position before velocity");

    auto fixed = ps.makeParticle(1.0f, 0, 0, 0);
    fixed->makeFixed();
    ps.clearForces();
    ps.applyForces();
    CHECK_NEAR(fixed->force.y, 0.0f, 0.001f, "Fixed particles skip global force fast path");
}

void test_fixed_and_locked_particles() {
    std::printf("\n--- Fixed/Locked Particles ---\n");
    ParticleSystem ps(100.0f, 0.0f);
    auto fixed = ps.makeParticle(1.0f, 10, 20, 0);
    auto locked = ps.makeParticle(1.0f, 30, 40, 0);

    fixed->velocity = tc::Vec3(5, 6, 0);
    fixed->makeFixed();
    locked->lock();

    CHECK_NEAR(fixed->velocity.x, 0.0f, 0.001f, "makeFixed clears velocity like original Traer");

    ps.tick(1.0f);
    CHECK_NEAR(fixed->position.y, 20.0f, 0.001f, "Fixed particle y unchanged under default RK4");
    CHECK_NEAR(locked->position.y, 40.0f, 0.001f, "Locked particle y unchanged under default RK4");
    CHECK_NEAR(fixed->getAge(), 1.0f, 0.001f, "Fixed particle still ages");
    CHECK_NEAR(locked->getAge(), 1.0f, 0.001f, "Locked particle still ages");

    locked->makeFree();
    CHECK(locked->isFixed(), "makeFree does not unlock locked particles");
    locked->unlock();
    CHECK(locked->isFree(), "unlock releases locked particle");
}

void test_spring_force() {
    std::printf("\n--- Spring Force ---\n");
    ParticleSystem ps(0.0f, 0.0f);
    auto a = ps.makeParticle(1.0f, 0, 0, 0);
    auto b = ps.makeParticle(1.0f, 15.0f, 0, 0);
    auto s = ps.makeSpring(a, b, 2.0f, 0.0f, 10.0f);

    ps.clearForces();
    s->apply();
    CHECK_NEAR(a->force.x, 10.0f, 0.001f, "Stretched spring pulls a toward b");
    CHECK_NEAR(b->force.x, -10.0f, 0.001f, "Stretched spring pulls b toward a");

    a->lock();
    ps.clearForces();
    s->apply();
    CHECK_NEAR(a->force.x, 0.0f, 0.001f, "Locked spring endpoint receives no force");
    CHECK_NEAR(b->force.x, -10.0f, 0.001f, "Free spring endpoint still receives force");
}

void test_attraction_force() {
    std::printf("\n--- Attraction Force ---\n");
    ParticleSystem ps(0.0f, 0.0f);
    auto a = ps.makeParticle(2.0f, 0, 0, 0);
    auto b = ps.makeParticle(3.0f, 10.0f, 0, 0);
    auto attraction = ps.makeAttraction(a, b, 1.0f, 1.0f);
    float expected = 1.0f * 2.0f * 3.0f / (10.0f * 10.0f);

    ps.clearForces();
    attraction->apply();
    CHECK_NEAR(a->force.x, expected, 0.001f, "Positive attraction pulls a toward b");
    CHECK_NEAR(b->force.x, -expected, 0.001f, "Positive attraction pulls b toward a");

    attraction->setMinimumDistance(5.0f);
    b->position = tc::Vec3(2.0f, 0, 0);
    ps.clearForces();
    attraction->apply();
    CHECK_NEAR(a->force.x, 6.0f / 25.0f, 0.001f, "Minimum distance clamps magnitude only");
}

void test_removal_semantics() {
    std::printf("\n--- Removal Semantics ---\n");
    ParticleSystem ps(0.0f, 0.0f);
    auto a = ps.makeParticle(1.0f, 0, 0, 0);
    auto b = ps.makeParticle(1.0f, 10, 0, 0);
    ps.makeSpring(a, b, 1.0f, 0.0f, 5.0f);
    ps.makeAttraction(a, b, 1.0f, 1.0f);

    CHECK(ps.removeParticle(a), "removeParticle reports existing particle removal");
    CHECK(ps.numParticles() == 1, "removeParticle removes particle from particle list");
    CHECK(ps.numSprings() == 1 && ps.numAttractions() == 1, "removeParticle preserves Traer 3.0 force-removal semantics");

    CHECK(ps.removeParticleAndForces(b), "removeParticleAndForces reports existing particle removal");
    CHECK(ps.numSprings() == 0 && ps.numAttractions() == 0, "removeParticleAndForces removes connected built-in forces");
}

void test_safe_api() {
    std::printf("\n--- Safe API ---\n");
    ParticleSystem ps;
    ps.reserve(4, 3, 2, 1);

    auto a = ps.makeParticle();
    auto b = ps.makeParticle();
    ps.makeSpring(a, b, 1.0f, 0.0f, 10.0f);

    CHECK(ps.getParticles().size() == 2, "getParticles exposes read-only particle view");
    CHECK(ps.getSprings().size() == 1, "getSprings exposes read-only spring view");

    ps.clear();
    bool threw = false;
    try {
        ps.getParticle(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    CHECK(threw, "getParticle throws on invalid index");

    threw = false;
    try {
        ps.setIntegrator(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw, "setIntegrator rejects null unique_ptr");

    ps.setIntegrator(ParticleSystem::MODIFIED_EULER);
    CHECK(ps.getIntegrator() != nullptr, "Traer-style integer integrator selector works");
}

void test_spring_period_rk4() {
    std::printf("\n--- Spring Period RK4 ---\n");
    ParticleSystem ps(0.0f, 0.0f);
    auto anchor = ps.makeParticle(1.0f, 0, 0, 0);
    anchor->makeFixed();
    auto bob = ps.makeParticle(1.0f, 10.0f, 0, 0);
    ps.makeSpring(anchor, bob, 1.0f, 0.0f, 0.0f);

    int crossings = 0;
    float lastX = bob->position.x;
    float crossTimes[6] = {};
    float dt = 0.005f;
    for (int i = 0; i < 5000; i++) {
        ps.tick(dt);
        float x = bob->position.x;
        if (lastX * x < 0) {
            crossTimes[crossings++] = i * dt;
            if (crossings >= 6) break;
        }
        lastX = x;
    }

    CHECK(crossings >= 6, "Oscillator crosses zero multiple times");
    if (crossings >= 6) {
        float expected = 2.0f * 3.14159265f;
        CHECK_NEAR(crossTimes[2] - crossTimes[0], expected, 0.05f, "RK4 spring period remains stable");
    }
}

int main() {
    std::printf("=== tcxTraerPhysics Verification Suite ===\n");
    test_gravity_force_semantics();
    test_fixed_and_locked_particles();
    test_spring_force();
    test_attraction_force();
    test_removal_semantics();
    test_safe_api();
    test_spring_period_rk4();
    std::printf("\n=== %d failures ===\n", failures);
    return failures ? 1 : 0;
}
