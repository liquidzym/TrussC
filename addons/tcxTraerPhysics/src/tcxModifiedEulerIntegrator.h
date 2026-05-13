#pragma once
// =============================================================================
// tcxModifiedEulerIntegrator.h — Symplectic Euler (semi-implicit)
// =============================================================================
// Better energy conservation than forward Euler. Uses velocity at t+dt:
//   v += F/m * dt,  x += v * dt   (updated v used for position)
//
// Fixed original bug where formula incorrectly divided by t.

#include "tcxIntegrator.h"

namespace tcx {

struct ParticleSystem;

struct ModifiedEulerIntegrator : public Integrator {
    ParticleSystem* system;

    explicit ModifiedEulerIntegrator(ParticleSystem* s) : system(s) {}

    void step(float dt) override;
};

} // namespace tcx
