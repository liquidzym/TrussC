#include "test_common.h"

#include <iostream>
#include <limits>
#include <sstream>

namespace {

struct Bounds {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();
};

Bounds particleBounds(const tcxCloth::Cloth& cloth) {
    Bounds bounds;
    for (const auto& particle : cloth.particles()) {
        bounds.minX = std::min(bounds.minX, particle.position.x);
        bounds.minY = std::min(bounds.minY, particle.position.y);
        bounds.minZ = std::min(bounds.minZ, particle.position.z);
        bounds.maxX = std::max(bounds.maxX, particle.position.x);
        bounds.maxY = std::max(bounds.maxY, particle.position.y);
        bounds.maxZ = std::max(bounds.maxZ, particle.position.z);
    }
    return bounds;
}

void requireBounds(bool condition, const Bounds& bounds, const char* message) {
    if (condition) return;
    std::ostringstream out;
    out << message << " bounds=(" << bounds.minX << "," << bounds.minY
        << "," << bounds.minZ << ")-(" << bounds.maxX << "," << bounds.maxY
        << "," << bounds.maxZ << ")";
    throw std::runtime_error(out.str());
}

} // namespace

int main() {
    tcxCloth::ClothSettings settings;
    settings.columns = 8;
    settings.rows = 6;
    settings.width = 140.0f;
    settings.height = 100.0f;
    settings.origin = tc::Vec3(20.0f, 30.0f, 0.0f);
    settings.constraintIterations = 8;
    settings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;

    tcxCloth::Cloth cloth;
    cloth.setup(settings);
    cloth.pinParticle(0, 0, true);

    const tc::Vec3 pinnedBefore = cloth.particlePosition(0, 0);
    cloth.setGlobalForce(tc::Vec3(400.0f, 400.0f, 0.0f));
    cloth.update(1.0f / 60.0f);
    require(cloth.particlePosition(0, 0) == pinnedBefore, "pinned particle remains fixed");

    tcxCloth::Cloth falling;
    falling.setup(settings);
    falling.setGravity(tc::Vec3(0.0f, 220.0f, 0.0f));
    const float freeYBefore = falling.particlePosition(4, 3).y;
    falling.update(1.0f / 60.0f);
    require(falling.particlePosition(4, 3).y > freeYBefore, "gravity moves a free particle down");

    falling.setGravity(tc::Vec3(0.0f, 0.0f, 0.0f));
    falling.setWind(tc::Vec3(0.0f, 0.0f, 1.0f), 180.0f);
    const float zBefore = falling.particlePosition(4, 3).z;
    falling.update(1.0f / 60.0f);
    require(falling.particlePosition(4, 3).z > zBefore, "wind affects free particles");

    tcxCloth::Cloth tangentWind;
    tangentWind.setup(settings);
    tangentWind.setGravity(tc::Vec3(0.0f, 0.0f, 0.0f));
    tangentWind.setWind(tc::Vec3(1.0f, 0.0f, 0.0f), 400.0f);
    const float tangentXBefore = tangentWind.particlePosition(4, 3).x;
    tangentWind.update(1.0f / 60.0f);
    requireNear(tangentWind.particlePosition(4, 3).x, tangentXBefore, 0.001f,
                "wind uses triangle normal projection, so tangent wind does not push a flat cloth");

    tcxCloth::SphereCollider sphere;
    sphere.center = falling.particlePosition(4, 3);
    sphere.radius = 18.0f;
    falling.setSphereColliders(std::span<const tcxCloth::SphereCollider>(&sphere, 1));
    falling.update(1.0f / 60.0f);
    require(falling.particlePosition(4, 3).distance(sphere.center) >= sphere.radius - 0.001f,
            "sphere collider pushes particles outside");

    tcxCloth::Cloth collisionVelocity;
    tcxCloth::ClothSettings collisionVelocitySettings = settings;
    collisionVelocitySettings.constraintIterations = 0;
    collisionVelocitySettings.enableShearConstraints = false;
    collisionVelocitySettings.enableBendConstraints = false;
    collisionVelocity.setup(collisionVelocitySettings);
    collisionVelocity.setGravity(tc::Vec3(0.0f, 0.0f, 0.0f));
    tcxCloth::SphereCollider impulseSphere;
    impulseSphere.center = collisionVelocity.particlePosition(4, 3);
    impulseSphere.radius = 18.0f;
    collisionVelocity.setSphereColliders(std::span<const tcxCloth::SphereCollider>(&impulseSphere, 1));
    collisionVelocity.update(1.0f / 60.0f);
    const float pushedY = collisionVelocity.particlePosition(4, 3).y;
    collisionVelocity.clearColliders();
    collisionVelocity.update(1.0f / 60.0f);
    requireNear(collisionVelocity.particlePosition(4, 3).y, pushedY, 0.001f,
                "sphere collision projection does not become Verlet velocity");

    falling.update(0.5f);
    for (const auto& particle : falling.particles()) {
        require(finite(particle.position), "dt spike keeps positions finite");
        require(finite(particle.normal), "dt spike keeps normals finite");
    }

    tcxCloth::Cloth stable;
    tcxCloth::ClothSettings stableSettings;
    stableSettings.columns = 32;
    stableSettings.rows = 24;
    stableSettings.width = 360.0f;
    stableSettings.height = 260.0f;
    stableSettings.origin = tc::Vec3(100.0f, 80.0f, 0.0f);
    stableSettings.constraintIterations = 8;
    stableSettings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;
    stable.setup(stableSettings);
    stable.pinTopEdge(2);
    stable.setGravity(tc::Vec3(0.0f, 430.0f, 0.0f));
    stable.setWind(tc::Vec3(0.30f, 0.0f, 0.0f), 60.0f);
    for (int i = 0; i < 180; ++i) {
        stable.update(1.0f / 60.0f);
    }
    const float maxReasonableEdge = 80.0f;
    for (const auto idx : stable.wireIndices()) {
        require(idx < stable.particles().size(), "wire index remains valid");
    }
    const auto particles = stable.particles();
    const auto wire = stable.wireIndices();
    for (std::size_t i = 0; i + 1 < wire.size(); i += 2) {
        const auto& a = particles[wire[i]].position;
        const auto& b = particles[wire[i + 1]].position;
        require(a.distance(b) < maxReasonableEdge, "visual stability keeps grid edges bounded");
    }

    const float viewW = 960.0f;
    const float viewH = 640.0f;

    tcxCloth::Cloth basicLayout;
    tcxCloth::ClothSettings basicSettings;
    basicSettings.columns = 64;
    basicSettings.rows = 64;
    basicSettings.width = std::min(viewW * 0.64f, 620.0f);
    basicSettings.height = std::min(viewH * 0.48f, 330.0f);
    basicSettings.origin = tc::Vec3((viewW - basicSettings.width) * 0.5f, 138.0f, 0.0f);
    basicSettings.constraintIterations = 8;
    basicSettings.damping = 0.66f;
    basicSettings.structuralStiffness = 0.88f;
    basicSettings.shearStiffness = 0.58f;
    basicSettings.bendStiffness = 0.18f;
    basicSettings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;
    basicLayout.setup(basicSettings);
    basicLayout.pinParticle(0, 0, true);
    basicLayout.pinParticle(basicSettings.columns - 1, 0, true);
    basicLayout.setGravity(tc::Vec3(0.0f, 560.0f, 0.0f));
    basicLayout.setWind(tc::Vec3(0.0f, 0.0f, 1.0f), 1.8f);
    for (int i = 0; i < 120; ++i) {
        basicLayout.update(1.0f / 60.0f);
    }
    const Bounds basicBounds = particleBounds(basicLayout);
    requireBounds(basicBounds.minX > 40.0f && basicBounds.maxX < viewW - 40.0f &&
                      basicBounds.minY > 100.0f && basicBounds.maxY < viewH - 20.0f,
                  basicBounds,
                  "basic example warmup keeps cloth visible");

    tcxCloth::Cloth windLayout;
    tcxCloth::ClothSettings windSettings;
    windSettings.columns = 96;
    windSettings.rows = 64;
    windSettings.width = std::min(viewW * 0.70f, 760.0f);
    windSettings.height = std::min(viewH * 0.48f, 340.0f);
    windSettings.origin = tc::Vec3((viewW - windSettings.width) * 0.5f, 138.0f, 0.0f);
    windSettings.constraintIterations = 12;
    windSettings.damping = 0.74f;
    windSettings.structuralStiffness = 0.86f;
    windSettings.shearStiffness = 0.54f;
    windSettings.bendStiffness = 0.16f;
    windSettings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;
    windLayout.setup(windSettings);
    windLayout.pinTopEdge(10);
    windLayout.setGravity(tc::Vec3(0.0f, 420.0f, 0.0f));
    for (int i = 0; i < 120; ++i) {
        const float t = static_cast<float>(i) / 60.0f;
        windLayout.setWind(tc::Vec3(0.32f * std::sin(t * 0.7f), 0.04f, 1.0f),
                           3.2f + 1.0f * std::sin(t * 1.1f));
        windLayout.update(1.0f / 60.0f);
    }
    const Bounds windBounds = particleBounds(windLayout);
    requireBounds(windBounds.maxZ - windBounds.minZ > 24.0f,
                  windBounds,
                  "wind example develops visible out-of-plane billow");
    requireBounds(windBounds.minX > 20.0f && windBounds.maxX < viewW - 20.0f &&
                      windBounds.minY > 90.0f && windBounds.maxY < viewH + 180.0f,
                  windBounds,
                  "wind example stays framed while billowing");

    tcxCloth::Cloth collisionLayout;
    tcxCloth::ClothSettings collisionSettings;
    collisionSettings.columns = 58;
    collisionSettings.rows = 48;
    collisionSettings.width = std::min(viewW * 0.60f, 560.0f);
    collisionSettings.height = std::min(viewH * 0.46f, 320.0f);
    collisionSettings.origin = tc::Vec3((viewW - collisionSettings.width) * 0.5f, 140.0f, 0.0f);
    collisionSettings.constraintIterations = 18;
    collisionSettings.damping = 0.57f;
    collisionSettings.structuralStiffness = 0.96f;
    collisionSettings.shearStiffness = 0.86f;
    collisionSettings.bendStiffness = 0.42f;
    collisionSettings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;
    collisionLayout.setup(collisionSettings);
    collisionLayout.pinParticle(1, 0, true);
    collisionLayout.pinParticle(collisionSettings.columns / 2, 0, true);
    collisionLayout.pinParticle(collisionSettings.columns - 2, 0, true);
    collisionLayout.setGravity(tc::Vec3(0.0f, 360.0f, 0.0f));
    collisionLayout.setWind(tc::Vec3(0.0f, 0.0f, 1.0f), 0.6f);
    tcxCloth::SphereCollider movingSphere;
    movingSphere.radius = 58.0f;
    for (int i = 0; i < 96; ++i) {
        const float t = static_cast<float>(i) / 60.0f;
        movingSphere.center = tc::Vec3(viewW * (0.52f + 0.10f * std::sin(t * 0.7f)),
                                       140.0f + collisionSettings.height * (0.58f + 0.08f * std::sin(t * 1.1f)),
                                       -100.0f + 170.0f * (0.5f + 0.5f * std::sin(t * 0.9f)));
        collisionLayout.setSphereColliders(std::span<const tcxCloth::SphereCollider>(&movingSphere, 1));
        collisionLayout.update(1.0f / 60.0f);
    }
    const Bounds collisionBounds = particleBounds(collisionLayout);
    requireBounds(collisionBounds.minX > 40.0f && collisionBounds.maxX < viewW - 40.0f &&
                      collisionBounds.minY > 100.0f && collisionBounds.maxY < viewH - 20.0f,
                  collisionBounds,
                  "collision example warmup keeps cloth visible");
    requireBounds(collisionBounds.maxZ - collisionBounds.minZ > 18.0f,
                  collisionBounds,
                  "collision example uses depth so sphere interaction is not a flat 2D push");

    cloth.reset();
    require(cloth.particlePosition(0, 0) == pinnedBefore, "reset preserves pinned position");
    requireNear(cloth.particlePosition(4, 3).z, 0.0f, 0.001f, "reset restores rest z position");

    std::cout << "tcxCloth_cpu_reference passed\n";
    return 0;
}
