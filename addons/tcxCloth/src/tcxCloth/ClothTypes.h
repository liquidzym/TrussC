#pragma once

#include "tcxCloth/ClothSettings.h"

#include <cstddef>

namespace tcxCloth {

enum class ConstraintKind {
    Structural,
    Shear,
    Bend
};

struct CpuParticle {
    tc::Vec3 position;
    tc::Vec3 previousPosition;
    tc::Vec3 acceleration {0.0f, 0.0f, 0.0f};
    tc::Vec3 normal {0.0f, 0.0f, 1.0f};
    tc::Vec2 uv;
    tc::Vec3 initialPosition;
    tc::Vec3 pinnedPosition;
    float inverseMass = 1.0f;
    bool pinned = false;
};

struct ClothConstraint {
    int a = -1;
    int b = -1;
    float restLength = 0.0f;
    float stiffness = 1.0f;
    ConstraintKind kind = ConstraintKind::Structural;
};

struct ClothTopologyInfo {
    int columns = 0;
    int rows = 0;
    std::size_t particleCount = 0;
    std::size_t triangleIndexCount = 0;
    std::size_t wireIndexCount = 0;
    std::size_t constraintCount = 0;
};

} // namespace tcxCloth
