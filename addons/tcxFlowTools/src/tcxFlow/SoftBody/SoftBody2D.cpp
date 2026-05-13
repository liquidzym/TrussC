#include "SoftBody2D.h"

#include <algorithm>
#include <cmath>

namespace tcx::flow {

namespace {

float safeMass(float mass) {
    return std::max(0.001f, mass);
}

float pointSegmentDistanceSquared(const tc::Vec2& p, const tc::Vec2& a, const tc::Vec2& b) {
    const tc::Vec2 ab = b - a;
    const float lenSq = ab.lengthSquared();
    if (lenSq <= 0.000001f) return p.distanceSquared(a);
    const float t = std::clamp((p - a).dot(ab) / lenSq, 0.0f, 1.0f);
    return p.distanceSquared(a + ab * t);
}

} // namespace

void SoftBody2D::clear() {
    particles_.clear();
    constraints_.clear();
}

int SoftBody2D::addParticle(const tc::Vec2& position, float mass, float radius) {
    SoftBody2DParticle particle;
    particle.position = position;
    particle.previousPosition = position;
    particle.invMass = 1.0f / safeMass(mass);
    particle.radius = radius;
    particles_.push_back(particle);
    return static_cast<int>(particles_.size()) - 1;
}

int SoftBody2D::addConstraint(int a, int b, float restLength, float stiffness,
                              SoftBody2DConstraintKind kind) {
    if (a < 0 || b < 0 ||
        a >= static_cast<int>(particles_.size()) ||
        b >= static_cast<int>(particles_.size()) ||
        a == b) {
        return -1;
    }

    SoftBody2DConstraint constraint;
    constraint.a = a;
    constraint.b = b;
    constraint.restLength = std::max(0.001f, restLength);
    constraint.stiffness = std::clamp(stiffness, 0.0f, 1.0f);
    constraint.kind = kind;
    constraints_.push_back(constraint);
    return static_cast<int>(constraints_.size()) - 1;
}

SoftBody2DGrid SoftBody2D::addGrid(const tc::Vec2& origin, const SoftBody2DGridSettings& settings) {
    SoftBody2DGrid grid;
    grid.firstParticle = static_cast<int>(particles_.size());
    grid.columns = std::max(1, settings.columns);
    grid.rows = std::max(1, settings.rows);

    for (int y = 0; y < grid.rows; ++y) {
        for (int x = 0; x < grid.columns; ++x) {
            addParticle(origin + tc::Vec2(x * settings.spacing, y * settings.spacing),
                        settings.mass,
                        settings.particleRadius);
        }
    }

    if (settings.structural) {
        for (int y = 0; y < grid.rows; ++y) {
            for (int x = 0; x < grid.columns; ++x) {
                if (x + 1 < grid.columns) {
                    addConstraint(grid.node(x, y), grid.node(x + 1, y), settings.spacing,
                                  settings.structuralStiffness, SoftBody2DConstraintKind::Structural);
                }
                if (y + 1 < grid.rows) {
                    addConstraint(grid.node(x, y), grid.node(x, y + 1), settings.spacing,
                                  settings.structuralStiffness, SoftBody2DConstraintKind::Structural);
                }
            }
        }
    }

    if (settings.shear) {
        const float diag = settings.spacing * std::sqrt(2.0f);
        for (int y = 0; y + 1 < grid.rows; ++y) {
            for (int x = 0; x + 1 < grid.columns; ++x) {
                addConstraint(grid.node(x, y), grid.node(x + 1, y + 1), diag,
                              settings.shearStiffness, SoftBody2DConstraintKind::Shear);
                addConstraint(grid.node(x + 1, y), grid.node(x, y + 1), diag,
                              settings.shearStiffness, SoftBody2DConstraintKind::Shear);
            }
        }
    }

    if (settings.bend && settings.bendDistance > 1) {
        const int d = settings.bendDistance;
        const float rest = settings.spacing * static_cast<float>(d);
        for (int y = 0; y < grid.rows; ++y) {
            for (int x = 0; x < grid.columns; ++x) {
                if (x + d < grid.columns) {
                    addConstraint(grid.node(x, y), grid.node(x + d, y), rest,
                                  settings.bendStiffness, SoftBody2DConstraintKind::Bend);
                }
                if (y + d < grid.rows) {
                    addConstraint(grid.node(x, y), grid.node(x, y + d), rest,
                                  settings.bendStiffness, SoftBody2DConstraintKind::Bend);
                }
            }
        }
    }

    return grid;
}

void SoftBody2D::setFixed(int index, bool fixed) {
    if (index < 0 || index >= static_cast<int>(particles_.size())) return;
    particles_[index].fixed = fixed;
    if (fixed) {
        particles_[index].previousPosition = particles_[index].position;
        particles_[index].acceleration = tc::Vec2(0.0f);
    }
}

void SoftBody2D::setPosition(int index, const tc::Vec2& position, bool resetVelocity) {
    if (index < 0 || index >= static_cast<int>(particles_.size())) return;
    particles_[index].position = position;
    if (resetVelocity) {
        particles_[index].previousPosition = position;
    }
}

void SoftBody2D::addForce(int index, const tc::Vec2& force) {
    if (index < 0 || index >= static_cast<int>(particles_.size())) return;
    auto& particle = particles_[index];
    if (particle.fixed) return;
    particle.acceleration += force * particle.invMass;
}

void SoftBody2D::addImpulse(int index, const tc::Vec2& impulse) {
    if (index < 0 || index >= static_cast<int>(particles_.size())) return;
    auto& particle = particles_[index];
    if (particle.fixed) return;
    particle.previousPosition -= impulse * particle.invMass;
}

int SoftBody2D::nearestParticle(const tc::Vec2& position, float maxDistance) const {
    int nearest = -1;
    float best = maxDistance * maxDistance;
    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        const float d = particles_[i].position.distanceSquared(position);
        if (d <= best) {
            best = d;
            nearest = i;
        }
    }
    return nearest;
}

int SoftBody2D::cutConstraintsNear(const tc::Vec2& position, float radius) {
    const float radiusSq = radius * radius;
    int cut = 0;
    for (auto& constraint : constraints_) {
        if (!constraint.enabled) continue;
        const auto& a = particles_[constraint.a].position;
        const auto& b = particles_[constraint.b].position;
        if (pointSegmentDistanceSquared(position, a, b) <= radiusSq) {
            constraint.enabled = false;
            ++cut;
        }
    }
    return cut;
}

void SoftBody2D::step(float dt) {
    if (particles_.empty()) return;
    const float clampedDt = std::clamp(dt, 0.0f, 1.0f / 20.0f);
    const float dtSq = clampedDt * clampedDt;

    for (auto& particle : particles_) {
        if (particle.fixed) {
            particle.previousPosition = particle.position;
            particle.acceleration = tc::Vec2(0.0f);
            continue;
        }

        const tc::Vec2 velocity = (particle.position - particle.previousPosition) * damping;
        const tc::Vec2 next = particle.position + velocity + (gravity + particle.acceleration) * dtSq;
        particle.previousPosition = particle.position;
        particle.position = next;
        particle.acceleration = tc::Vec2(0.0f);
    }

    for (int i = 0; i < solverIterations; ++i) {
        for (auto& constraint : constraints_) {
            satisfyConstraint(constraint);
        }
        if (boundsEnabled) {
            for (auto& particle : particles_) {
                satisfyBounds(particle);
            }
        }
    }
}

void SoftBody2D::satisfyConstraint(SoftBody2DConstraint& constraint) {
    if (!constraint.enabled) return;
    auto& a = particles_[constraint.a];
    auto& b = particles_[constraint.b];
    const float invA = a.fixed ? 0.0f : a.invMass;
    const float invB = b.fixed ? 0.0f : b.invMass;
    const float invTotal = invA + invB;
    if (invTotal <= 0.0f) return;

    const tc::Vec2 delta = b.position - a.position;
    const float length = delta.length();
    if (length <= 0.000001f) return;

    const float error = (length - constraint.restLength) / length;
    const tc::Vec2 correction = delta * (error * constraint.stiffness);
    if (invA > 0.0f) {
        a.position += correction * (invA / invTotal);
    }
    if (invB > 0.0f) {
        b.position -= correction * (invB / invTotal);
    }
}

void SoftBody2D::satisfyBounds(SoftBody2DParticle& particle) {
    if (particle.fixed) return;
    const tc::Vec2 before = particle.position;
    particle.position.x = std::clamp(particle.position.x, boundsMin.x + particle.radius, boundsMax.x - particle.radius);
    particle.position.y = std::clamp(particle.position.y, boundsMin.y + particle.radius, boundsMax.y - particle.radius);
    if (before != particle.position) {
        const tc::Vec2 velocity = (particle.position - particle.previousPosition) * boundsBounce;
        particle.previousPosition = particle.position + velocity;
    }
}

} // namespace tcx::flow
