#pragma once
// =============================================================================
// tcxEulerIntegrator.h — Forward Euler integration
// =============================================================================
// Simplest, fastest. Suitable for damped systems but can add energy.
// v += F/m * dt,  x += v * dt
//
// NOTE: the original ofxTraerPhysics had a bug — dividing by t instead of
// multiplying. This version uses correct forward Euler.

#include "tcxIntegrator.h"

namespace tcx {

struct ParticleSystem;

struct EulerIntegrator : public Integrator {
    ParticleSystem* system;

    explicit EulerIntegrator(ParticleSystem* s) : system(s) {}

    void step(float dt) override;
};

} // namespace tcx
