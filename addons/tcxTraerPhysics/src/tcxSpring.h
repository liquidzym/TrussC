#pragma once
// =============================================================================
// tcxSpring.h — Hooke's law spring with damping
// =============================================================================
// Applies force proportional to stretch from rest length.
// F = -ks * (currentLength - restLength) - damping * (relative velocity along spring)

#include "tcxForce.h"
#include "tcxParticle.h"
#include <algorithm>
#include <memory>

namespace tcx {

struct Spring : public Force {
    using Ptr = std::shared_ptr<Spring>;

    std::shared_ptr<Particle> a;
    std::shared_ptr<Particle> b;
    float springConstant = 0.2f;  // stiffness
    float damping        = 0.01f; // velocity damping
    float restLength     = 100.0f;
    bool  on = true;

    Spring(std::shared_ptr<Particle> A,
           std::shared_ptr<Particle> B,
           float ks, float d, float r)
        : a(A), b(B), springConstant(ks), damping(d) {
        setRestLength(r);
    }

    void turnOn()  override { on = true; }
    void turnOff() override { on = false; }
    bool isOn()  const override { return on; }
    bool isOff() const override { return !on; }

    std::shared_ptr<Particle> getOneEnd()       const { return a; }
    std::shared_ptr<Particle> getTheOtherEnd()  const { return b; }

    float currentLength() const { return a->position.distance(b->position); }
    float getRestLength()  const { return restLength; }
    float restLengthValue() const { return restLength; }
    float getStrength()    const { return springConstant; }
    void  setStrength(float ks)  { springConstant = ks; }
    float getDamping()     const { return damping; }
    void  setDamping(float d)    { damping = d; }
    void  setRestLength(float r) { restLength = std::max(0.0f, r); }

    void apply() override {
        if (!on || (a->isFixed() && b->isFixed())) return;

        tc::Vec3 a2b       = a->position - b->position;
        float dist     = a2b.length();
        if (dist < 0.0001f) return;  // avoid division by zero

        // Normalize direction
        float invDist  = 1.0f / dist;
        a2b.x *= invDist;
        a2b.y *= invDist;
        a2b.z *= invDist;

        // Spring force: proportional to stretch
        float springForce = -(dist - restLength) * springConstant;

        // Damping: relative velocity projected onto spring direction
        tc::Vec3 relVel = a->velocity - b->velocity;
        float dampingForce = -damping * (a2b.x * relVel.x + a2b.y * relVel.y + a2b.z * relVel.z);

        float totalForce = springForce + dampingForce;
        tc::Vec3 forceVec = a2b * totalForce;

        if (a->isFree()) a->force += forceVec;
        if (b->isFree()) b->force -= forceVec;
    }
};

} // namespace tcx
