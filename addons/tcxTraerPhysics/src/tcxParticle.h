#pragma once
// =============================================================================
// tcxParticle.h — Particle with position, velocity, force
// =============================================================================
// Uses tc::Vec3 for 3D physics. For 2D-only, set z = 0 and ignore.

#include <TrussC.h>
#include <memory>

namespace tcx {

struct Particle {
    tc::Vec3  position{0, 0, 0};
    tc::Vec3  velocity{0, 0, 0};
    tc::Vec3  force{0, 0, 0};
    float mass  = 1.0f;
    float age   = 0.0f;
    bool  fixed = false;
    bool  locked = false;  // integrators and two-body forces treat locked as fixed

    Particle() = default;

    Particle(float m)
        : mass(m) {}

    Particle(float m, const tc::Vec3& pos)
        : position(pos), mass(m) {}

    Particle(float m, float x, float y, float z = 0)
        : position(x, y, z), mass(m) {}

    // Make particle immovable
    void makeFixed() { fixed = true; velocity = tc::Vec3(0, 0, 0); }
    void makeFree()  { if (!locked) fixed = false; }
    bool isFixed() const { return fixed || locked; }
    bool isFree()  const { return !fixed && !locked; }
    void lock() { locked = true; velocity = tc::Vec3(0, 0, 0); }
    void unlock() { locked = false; }
    float getAge() const { return age; }
    float getMass() const { return mass; }
    void  setMass(float m) { mass = (m < 0.001f) ? 0.001f : m; }
    float distanceTo(const Particle& p) const { return position.distance(p.position); }

    // Reset to initial state
    void reset() {
        position = tc::Vec3(0, 0, 0);
        velocity = tc::Vec3(0, 0, 0);
        force    = tc::Vec3(0, 0, 0);
        mass     = 1.0f;
        age      = 0.0f;
        fixed    = false;
        locked   = false;
    }
};

} // namespace tcx
