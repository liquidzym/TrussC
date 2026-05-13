#pragma once

#include <TrussC.h>

#include <vector>

namespace tcx::flow {

enum class SoftBody2DConstraintKind {
    Structural,
    Shear,
    Bend
};

struct SoftBody2DParticle {
    tc::Vec2 position;
    tc::Vec2 previousPosition;
    tc::Vec2 acceleration;
    float invMass = 1.0f;
    float radius = 4.0f;
    bool fixed = false;
};

struct SoftBody2DConstraint {
    int a = -1;
    int b = -1;
    float restLength = 0.0f;
    float stiffness = 1.0f;
    SoftBody2DConstraintKind kind = SoftBody2DConstraintKind::Structural;
    bool enabled = true;
};

struct SoftBody2DGridSettings {
    int columns = 24;
    int rows = 18;
    float spacing = 18.0f;
    float mass = 1.0f;
    float particleRadius = 3.0f;
    bool structural = true;
    bool shear = true;
    bool bend = true;
    int bendDistance = 2;
    float structuralStiffness = 0.95f;
    float shearStiffness = 0.72f;
    float bendStiffness = 0.36f;
};

struct SoftBody2DGrid {
    int firstParticle = 0;
    int columns = 0;
    int rows = 0;

    int node(int x, int y) const {
        return firstParticle + y * columns + x;
    }
};

class SoftBody2D {
public:
    void clear();

    int addParticle(const tc::Vec2& position, float mass = 1.0f, float radius = 4.0f);
    int addConstraint(int a, int b, float restLength, float stiffness,
                      SoftBody2DConstraintKind kind = SoftBody2DConstraintKind::Structural);
    SoftBody2DGrid addGrid(const tc::Vec2& origin, const SoftBody2DGridSettings& settings);

    void setFixed(int index, bool fixed);
    void setPosition(int index, const tc::Vec2& position, bool resetVelocity = true);
    void addForce(int index, const tc::Vec2& force);
    void addImpulse(int index, const tc::Vec2& impulse);
    int nearestParticle(const tc::Vec2& position, float maxDistance) const;
    int cutConstraintsNear(const tc::Vec2& position, float radius);

    void step(float dt);

    bool empty() const { return particles_.empty(); }
    int particleCount() const { return static_cast<int>(particles_.size()); }
    int constraintCount() const { return static_cast<int>(constraints_.size()); }
    const std::vector<SoftBody2DParticle>& particles() const { return particles_; }
    const std::vector<SoftBody2DConstraint>& constraints() const { return constraints_; }
    std::vector<SoftBody2DParticle>& particles() { return particles_; }
    std::vector<SoftBody2DConstraint>& constraints() { return constraints_; }

    tc::Vec2 gravity = tc::Vec2(0.0f, 340.0f);
    float damping = 0.992f;
    int solverIterations = 6;
    bool boundsEnabled = true;
    tc::Vec2 boundsMin = tc::Vec2(0.0f, 0.0f);
    tc::Vec2 boundsMax = tc::Vec2(1280.0f, 720.0f);
    float boundsBounce = 0.18f;

private:
    void satisfyConstraint(SoftBody2DConstraint& constraint);
    void satisfyBounds(SoftBody2DParticle& particle);

    std::vector<SoftBody2DParticle> particles_;
    std::vector<SoftBody2DConstraint> constraints_;
};

} // namespace tcx::flow
