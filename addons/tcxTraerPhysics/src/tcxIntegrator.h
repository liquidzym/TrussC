#pragma once
// =============================================================================
// tcxIntegrator.h — Abstract integrator interface
// =============================================================================

namespace tcx {

struct ParticleSystem; // forward declaration

struct Integrator {
    virtual ~Integrator() = default;
    virtual void step(float dt) = 0;
};

} // namespace tcx
