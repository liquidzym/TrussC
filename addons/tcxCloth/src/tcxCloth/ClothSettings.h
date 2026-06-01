#pragma once

#include <TrussC.h>

namespace tcxCloth {

struct SphereCollider {
    tc::Vec3 center {};
    float radius = 1.0f;
};

struct PlaneCollider {
    tc::Vec3 normal {0.0f, 1.0f, 0.0f};
    float distance = 0.0f;
};

struct ClothSettings {
    int columns = 64;
    int rows = 64;

    float width = 400.0f;
    float height = 400.0f;
    tc::Vec3 origin {0.0f, 0.0f, 0.0f};

    float damping = 0.45f;
    float fixedTimeStep = 1.0f / 60.0f;
    int substeps = 1;

    int constraintIterations = 8;
    float structuralStiffness = 1.0f;
    float shearStiffness = 1.0f;
    float bendStiffness = 0.6f;

    bool enableShearConstraints = true;
    bool enableBendConstraints = true;
    bool enableWind = true;
    bool recomputeNormals = true;

    enum class SolverBackend {
        Auto,
        ComputeStorageBuffer,
        TexturePingPong,
        CpuReference
    };

    SolverBackend backend = SolverBackend::Auto;
};

} // namespace tcxCloth

namespace tcx {
namespace cloth = ::tcxCloth;
}
