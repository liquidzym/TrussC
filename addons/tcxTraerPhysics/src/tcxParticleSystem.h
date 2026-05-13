#pragma once
// =============================================================================
// tcxParticleSystem.h — Particle system with springs, attractions, integrators
// =============================================================================
// Owns all particles/forces via shared_ptr. Positions are trivially copyable
// for snapshotting to a render buffer — use mutex or ThreadChannel for
// cross-thread synchronization.
//
// Fixed bugs from original ofxTraerPhysics:
//   - Euler integrators: multiplied by dt instead of dividing
//   - Attraction::setStrength: self-assignment k=k fixed
//   - Removed fast-inverse-sqrt hack (modern CPUs do sqrt in hardware)

#include "tcxParticle.h"
#include "tcxSpring.h"
#include "tcxAttraction.h"
#include "tcxIntegrator.h"
#include "tcxEulerIntegrator.h"
#include "tcxModifiedEulerIntegrator.h"
#include "tcxRungeKuttaIntegrator.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <stdexcept>
#include <string>

namespace tcx {

// =========================================================================
// ParticleSystem
// =========================================================================
struct ParticleSystem {
    using Ptr = std::shared_ptr<ParticleSystem>;
    enum class IntegratorType {
        RungeKutta = 0,
        ModifiedEuler = 1,
        Euler = 2
    };

    static constexpr int RUNGE_KUTTA = 0;
    static constexpr int MODIFIED_EULER = 1;
    static constexpr int EULER = 2;

    // Factory (preferred — returns shared_ptr)
    static Ptr create(float gravityY = 0.0f, float drag = 0.001f) {
        return std::make_shared<ParticleSystem>(gravityY, drag);
    }

    static Ptr create(float gx, float gy, float gz, float drag = 0.001f) {
        return std::make_shared<ParticleSystem>(gx, gy, gz, drag);
    }

    // --- Construction ---

    ParticleSystem() {
        integrator = std::make_unique<RungeKuttaIntegrator>(this);
    }

    ParticleSystem(float gravityY, float dragVal)
        : gravity(0, gravityY, 0), drag(dragVal) {
        integrator = std::make_unique<RungeKuttaIntegrator>(this);
    }

    ParticleSystem(float gx, float gy, float gz, float dragVal)
        : gravity(gx, gy, gz), drag(dragVal) {
        integrator = std::make_unique<RungeKuttaIntegrator>(this);
    }

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) = delete;
    ParticleSystem& operator=(ParticleSystem&&) = delete;

    // --- Integrator ---

    void setIntegrator(std::unique_ptr<Integrator> i) {
        if (!i) {
            throw std::invalid_argument("ParticleSystem::setIntegrator requires a non-null integrator");
        }
        integrator = std::move(i);
    }

    void setIntegrator(IntegratorType type) {
        switch (type) {
            case IntegratorType::RungeKutta:
                integrator = std::make_unique<RungeKuttaIntegrator>(this);
                break;
            case IntegratorType::ModifiedEuler:
                integrator = std::make_unique<ModifiedEulerIntegrator>(this);
                break;
            case IntegratorType::Euler:
                integrator = std::make_unique<EulerIntegrator>(this);
                break;
        }
    }

    void setIntegrator(int type) {
        switch (type) {
            case RUNGE_KUTTA: setIntegrator(IntegratorType::RungeKutta); break;
            case MODIFIED_EULER: setIntegrator(IntegratorType::ModifiedEuler); break;
            case EULER: setIntegrator(IntegratorType::Euler); break;
            default:
                throw std::invalid_argument("Unknown ParticleSystem integrator type");
        }
    }

    // --- Gravity & Drag ---

    void setGravity(float x, float y, float z) { gravity = tc::Vec3(x, y, z); }
    void setGravity(float g)                   { gravity = tc::Vec3(0, g, 0); }
    void setDrag(float d)                      { drag = d; }

    // --- Step physics ---

    void tick()              { tick(1.0f); }  // matches original TraerPhysics default
    void tick(float dt)      { integrator->step(dt); }

    // --- Factory methods ---

    std::shared_ptr<Particle> makeParticle() {
        return makeParticle(1.0f, 0, 0, 0);
    }

    std::shared_ptr<Particle> makeParticle(float mass, float x, float y, float z = 0) {
        if (mass < 0.001f) mass = 0.001f;  // prevent division-by-zero in integrators
        auto p = std::make_shared<Particle>(mass, x, y, z);
        particles.push_back(p);
        return p;
    }

    std::shared_ptr<Particle> makeParticle(float mass, const tc::Vec3& pos) {
        return makeParticle(mass, pos.x, pos.y, pos.z);
    }

    void reserve(std::size_t particleCount,
                 std::size_t springCount = 0,
                 std::size_t attractionCount = 0,
                 std::size_t customForceCount = 0)
    {
        particles.reserve(particleCount);
        springs.reserve(springCount);
        attractions.reserve(attractionCount);
        customForces.reserve(customForceCount);
    }

    void reserveParticles(std::size_t count) { particles.reserve(count); }
    void reserveSprings(std::size_t count) { springs.reserve(count); }
    void reserveAttractions(std::size_t count) { attractions.reserve(count); }
    void reserveCustomForces(std::size_t count) { customForces.reserve(count); }

    std::shared_ptr<Spring> makeSpring(
        std::shared_ptr<Particle> a, std::shared_ptr<Particle> b,
        float stiffness, float damping, float restLength)
    {
        auto s = std::make_shared<Spring>(a, b, stiffness, damping, restLength);
        springs.push_back(s);
        return s;
    }

    std::shared_ptr<Attraction> makeAttraction(
        std::shared_ptr<Particle> a, std::shared_ptr<Particle> b,
        float strength, float minDistance)
    {
        auto m = std::make_shared<Attraction>(a, b, strength, minDistance);
        attractions.push_back(m);
        return m;
    }

    void addCustomForce(std::shared_ptr<Force> f) {
        customForces.push_back(f);
    }

    // --- Accessors ---

    int numParticles()   const { return (int)particles.size(); }
    int numSprings()     const { return (int)springs.size(); }
    int numAttractions() const { return (int)attractions.size(); }
    int numCustomForces() const { return (int)customForces.size(); }
    int numberOfParticles() const { return numParticles(); }
    int numberOfSprings() const { return numSprings(); }
    int numberOfAttractions() const { return numAttractions(); }
    int numberOfCustomForces() const { return numCustomForces(); }

    std::shared_ptr<Particle>   getParticle(int i)   { return particles[checkedIndex(i, particles.size(), "particle")]; }
    std::shared_ptr<Spring>     getSpring(int i)     { return springs[checkedIndex(i, springs.size(), "spring")]; }
    std::shared_ptr<Attraction> getAttraction(int i) { return attractions[checkedIndex(i, attractions.size(), "attraction")]; }
    std::shared_ptr<Force>      getCustomForce(int i) { return customForces[checkedIndex(i, customForces.size(), "custom force")]; }
    const std::vector<std::shared_ptr<Particle>>& getParticles() const { return particles; }
    const std::vector<std::shared_ptr<Spring>>& getSprings() const { return springs; }
    const std::vector<std::shared_ptr<Attraction>>& getAttractions() const { return attractions; }
    const std::vector<std::shared_ptr<Force>>& getCustomForces() const { return customForces; }

    // --- Removal (by index or pointer) ---

    std::shared_ptr<Particle> removeParticle(int i) {
        auto index = checkedIndex(i, particles.size(), "particle");
        auto removed = particles[index];
        particles.erase(particles.begin() + index);
        return removed;
    }

    std::shared_ptr<Spring> removeSpring(int i) {
        auto index = checkedIndex(i, springs.size(), "spring");
        auto removed = springs[index];
        springs.erase(springs.begin() + index);
        return removed;
    }

    std::shared_ptr<Attraction> removeAttraction(int i) {
        auto index = checkedIndex(i, attractions.size(), "attraction");
        auto removed = attractions[index];
        attractions.erase(attractions.begin() + index);
        return removed;
    }

    std::shared_ptr<Force> removeCustomForce(int i) {
        auto index = checkedIndex(i, customForces.size(), "custom force");
        auto removed = customForces[index];
        customForces.erase(customForces.begin() + index);
        return removed;
    }

    bool removeParticle(std::shared_ptr<Particle> p) { return eraseOne(particles, p); }
    bool removeSpring(std::shared_ptr<Spring> s) { return eraseOne(springs, s); }
    bool removeAttraction(std::shared_ptr<Attraction> a) { return eraseOne(attractions, a); }
    bool removeCustomForce(std::shared_ptr<Force> f) { return eraseOne(customForces, f); }

    // Traer 3.0 keeps removeParticle() independent from force removal. This
    // helper is the safer C++ ownership path for callers that want both.
    bool removeParticleAndForces(std::shared_ptr<Particle> p) {
        bool removed = removeParticle(p);
        removeForcesConnectedTo(p);
        return removed;
    }

    void removeForcesConnectedTo(std::shared_ptr<Particle> p) {
        springs.erase(std::remove_if(springs.begin(), springs.end(),
            [&](const auto& s) { return s->a == p || s->b == p; }), springs.end());
        attractions.erase(std::remove_if(attractions.begin(), attractions.end(),
            [&](const auto& a) { return a->a == p || a->b == p; }), attractions.end());
    }

    // --- Clear all ---

    void clear() {
        particles.clear();
        springs.clear();
        attractions.clear();
        customForces.clear();
    }

    // --- Force computation (called by integrators) ---

    void clearForces() {
        for (auto& p : particles) {
            p->force = tc::Vec3(0, 0, 0);
        }
    }

    void applyForces() {
        // Matches Traer 3.0: gravity is a global force vector, not mass-scaled acceleration.
        const bool hasGravity = !isZero(gravity);
        const bool hasDrag = drag != 0.0f;
        if (hasGravity || hasDrag) {
            for (auto& p : particles) {
                if (p->isFixed()) continue;
                if (hasGravity) p->force += gravity;
                if (hasDrag) {
                    p->force.x -= p->velocity.x * drag;
                    p->force.y -= p->velocity.y * drag;
                    p->force.z -= p->velocity.z * drag;
                }
            }
        }

        // Springs
        for (auto& s : springs) s->apply();

        // Attractions
        for (auto& a : attractions) a->apply();

        // Custom forces
        for (auto& f : customForces) f->apply();
    }

    Integrator* getIntegrator() { return integrator.get(); }

    // --- Public data (for integrator access) ---

    std::vector<std::shared_ptr<Particle>>   particles;
    std::vector<std::shared_ptr<Spring>>     springs;
    std::vector<std::shared_ptr<Attraction>> attractions;
    std::vector<std::shared_ptr<Force>>      customForces;
    tc::Vec3   gravity{0, 0, 0};
    float  drag = 0.001f;

private:
    std::unique_ptr<Integrator> integrator;  // auto-deleted on destruction

    static std::size_t checkedIndex(int i, std::size_t size, const char* label) {
        if (i < 0 || static_cast<std::size_t>(i) >= size) {
            throw std::out_of_range(std::string("ParticleSystem ") + label + " index out of range");
        }
        return static_cast<std::size_t>(i);
    }

    static bool isZero(const tc::Vec3& v) {
        return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f;
    }

    template <class T>
    static bool eraseOne(std::vector<std::shared_ptr<T>>& values, const std::shared_ptr<T>& value) {
        auto it = std::find(values.begin(), values.end(), value);
        if (it == values.end()) return false;
        values.erase(it);
        return true;
    }
};

// =========================================================================
// Integrator implementations (inline, after ParticleSystem definition)
// =========================================================================

// --- Euler (forward) ---
// Forward Euler: x += v*dt, then v += a*dt. Simplest but least stable.
// FIXED: was dividing by dt (p->force / (p->mass*t)), now correctly multiplies.
inline void EulerIntegrator::step(float dt) {
    system->clearForces();
    system->applyForces();

    for (auto& p : system->particles) {
        p->age += dt;
        if (p->isFixed()) continue;
        // Forward Euler: position first with old velocity
        p->position += p->velocity * dt;
        p->velocity += p->force * (dt / p->mass);
    }
}

// --- Modified Euler (symplectic) ---
// Semi-implicit: v(t+dt) = v + a*dt, then x(t+dt) = x + v(t+dt)*dt
// FIXED: original formula was incorrect. Now uses proper symplectic Euler.
inline void ModifiedEulerIntegrator::step(float dt) {
    system->clearForces();
    system->applyForces();

    for (auto& p : system->particles) {
        p->age += dt;
        if (p->isFixed()) continue;
        // Semi-implicit: v += a*dt, then x += v_new*dt
        float invMassDt = dt / p->mass;
        p->velocity += p->force * invMassDt;
        p->position += p->velocity * dt;
    }
}

// --- Runge-Kutta 4 ---
inline void RungeKuttaIntegrator::step(float dt) {
    int n = system->numParticles();
    ensureCapacity(n);

    // Save original state
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            originalPositions[i]  = p->position;
            originalVelocities[i] = p->velocity;
        }
        p->force.x = 0.0f; p->force.y = 0.0f; p->force.z = 0.0f;
    }

    // --- k1 ---
    system->applyForces();
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            k1Forces[i]     = p->force;
            k1Velocities[i] = p->velocity;
        }
        p->force.x = 0.0f; p->force.y = 0.0f; p->force.z = 0.0f;
    }

    // --- k2 (midpoint 1) ---
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            p->position = originalPositions[i]  + k1Velocities[i] * (0.5f * dt);
            p->velocity = originalVelocities[i] + k1Forces[i]     * (0.5f * dt / p->mass);
        }
    }
    system->applyForces();
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            k2Forces[i]     = p->force;
            k2Velocities[i] = p->velocity;
        }
        p->force.x = 0.0f; p->force.y = 0.0f; p->force.z = 0.0f;
    }

    // --- k3 (midpoint 2) ---
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            p->position = originalPositions[i]  + k2Velocities[i] * (0.5f * dt);
            p->velocity = originalVelocities[i] + k2Forces[i]     * (0.5f * dt / p->mass);
        }
    }
    system->applyForces();
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            k3Forces[i]     = p->force;
            k3Velocities[i] = p->velocity;
        }
        p->force.x = 0.0f; p->force.y = 0.0f; p->force.z = 0.0f;
    }

    // --- k4 (full step) ---
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            p->position = originalPositions[i]  + k3Velocities[i] * dt;
            p->velocity = originalVelocities[i] + k3Forces[i]     * (dt / p->mass);
        }
    }
    system->applyForces();
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        if (p->isFree()) {
            k4Forces[i]     = p->force;
            k4Velocities[i] = p->velocity;
        }
        p->force.x = 0.0f; p->force.y = 0.0f; p->force.z = 0.0f;
    }

    // --- Combine ---
    for (int i = 0; i < n; i++) {
        auto& p = system->particles[i];
        p->age += dt;
        if (p->isFixed()) continue;

        p->position = originalPositions[i] + (k1Velocities[i] + k2Velocities[i] * 2.0f +
                                              k3Velocities[i] * 2.0f + k4Velocities[i]) * (dt / 6.0f);

        p->velocity = originalVelocities[i] + (k1Forces[i] + k2Forces[i] * 2.0f +
                                               k3Forces[i] * 2.0f + k4Forces[i]) * (dt / (6.0f * p->mass));
    }
}

inline void RungeKuttaIntegrator::ensureCapacity(int n) {
    auto resize = [n](auto& v) { if ((int)v.size() < n) v.resize(n); };
    resize(originalPositions);  resize(originalVelocities);
    resize(k1Forces); resize(k1Velocities);
    resize(k2Forces); resize(k2Velocities);
    resize(k3Forces); resize(k3Velocities);
    resize(k4Forces); resize(k4Velocities);
}

} // namespace tcx
