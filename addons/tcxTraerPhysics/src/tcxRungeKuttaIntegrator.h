#pragma once
// =============================================================================
// tcxRungeKuttaIntegrator.h — RK4 (4th-order Runge-Kutta) integration
// =============================================================================
// Highest accuracy, 4 force evaluations per step. Best for orbital mechanics
// and systems where energy conservation matters.
// Uses variable-size internal arrays to avoid per-frame allocations.

#include "tcxIntegrator.h"
#include "tcxParticle.h"
#include <vector>

namespace tcx {

struct ParticleSystem;

struct RungeKuttaIntegrator : public Integrator {
    ParticleSystem* system;

    // Per-particle RK4 scratch arrays
    std::vector<tc::Vec3> originalPositions;
    std::vector<tc::Vec3> originalVelocities;
    std::vector<tc::Vec3> k1Forces, k1Velocities;
    std::vector<tc::Vec3> k2Forces, k2Velocities;
    std::vector<tc::Vec3> k3Forces, k3Velocities;
    std::vector<tc::Vec3> k4Forces, k4Velocities;

    explicit RungeKuttaIntegrator(ParticleSystem* s) : system(s) {}

    void step(float dt) override;

private:
    void ensureCapacity(int n);
};

} // namespace tcx
