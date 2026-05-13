#pragma once
// =============================================================================
// tcxAttraction.h — N-body gravitational attraction (or repulsion)
// =============================================================================
// F = k * m1 * m2 / distance²   (positive k = attract, negative k = repel)
// Uses regular sqrt (modern CPUs handle it fine; removed fast-inverse-sqrt hack).

#include "tcxForce.h"
#include "tcxParticle.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace tcx {

struct Attraction : public Force {
    using Ptr = std::shared_ptr<Attraction>;

    std::shared_ptr<Particle> a;
    std::shared_ptr<Particle> b;
    float k           = 1.0f;   // strength (positive=attract, negative=repel)
    bool  on          = true;
    float distanceMin = 5.0f;         // prevent singularity at zero distance
    float distanceMinSquared = 25.0f; // precomputed

    Attraction(std::shared_ptr<Particle> A,
               std::shared_ptr<Particle> B,
               float strength, float minDist)
        : a(A), b(B), k(strength) {
        setMinimumDistance(minDist);
    }

    void turnOn()  override { on = true; }
    void turnOff() override { on = false; }
    bool isOn()  const override { return on; }
    bool isOff() const override { return !on; }

    float getMinimumDistance() const { return distanceMin; }
    float minimumDistance() const { return distanceMin; }
    void  setMinimumDistance(float d) {
        distanceMin = std::max(0.0f, d);
        distanceMinSquared = distanceMin * distanceMin;
    }
    float getStrength()        const { return k; }
    void  setStrength(float s)       { k = s; }
    float strength() const { return k; }

    std::shared_ptr<Particle> getOneEnd()      const { return a; }
    std::shared_ptr<Particle> getTheOtherEnd() const { return b; }

    void apply() override {
        if (!on || (a->isFixed() && b->isFixed())) return;

        // Direction vector a → b
        tc::Vec3 a2b = a->position - b->position;
        float distSq = a2b.x * a2b.x + a2b.y * a2b.y + a2b.z * a2b.z;

        // Normalize direction from actual positions (before clamping!)
        float dist = std::sqrt(distSq);
        if (dist < 0.0001f) return;
        float invDist = 1.0f / dist;
        float nx = a2b.x * invDist;
        float ny = a2b.y * invDist;
        float nz = a2b.z * invDist;

        // Clamp distance for force magnitude (prevent singularity)
        if (distSq < distanceMinSquared) distSq = distanceMinSquared;

        // F = k * m1 * m2 / r²
        float force = k * a->mass * b->mass / distSq;

        // Apply along normalized direction
        tc::Vec3 f(nx * force, ny * force, nz * force);

        if (a->isFree()) a->force -= f;
        if (b->isFree()) b->force += f;
    }
};

} // namespace tcx
