#pragma once

#include "tcxCloth/ClothTypes.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tcxCloth {

class Cloth {
public:
    Cloth();
    ~Cloth();

    Cloth(const Cloth&) = delete;
    Cloth& operator=(const Cloth&) = delete;
    Cloth(Cloth&&) noexcept;
    Cloth& operator=(Cloth&&) noexcept;

    void setup(const ClothSettings& settings);
    void release();
    void reset();

    void update(float dt);
    void draw();
    void drawWire();

    void setGlobalForce(const tc::Vec3& force);
    void setGravity(const tc::Vec3& gravity);
    void setWind(const tc::Vec3& windDirection, float strength);

    void setSphereColliders(std::span<const SphereCollider> spheres);
    void setPlaneColliders(std::span<const PlaneCollider> planes);
    void clearColliders();

    void pinParticle(int x, int y, bool pinned = true);
    void pinTopEdge(int step = 1);
    void pinCorners();

    void setParticlePosition(int x, int y, const tc::Vec3& position);
    tc::Vec3 particlePosition(int x, int y) const;
    bool isPinned(int x, int y) const;

    int columns() const { return settings_.columns; }
    int rows() const { return settings_.rows; }
    int particleCount() const { return static_cast<int>(particles_.size()); }
    std::size_t triangleIndexCount() const { return triangleIndices_.size(); }
    std::size_t wireIndexCount() const { return wireIndices_.size(); }
    std::size_t constraintCount() const { return constraints_.size(); }
    ClothTopologyInfo topologyInfo() const;

    ClothSettings::SolverBackend activeBackend() const { return activeBackend_; }
    const std::string& backendReason() const { return backendReason_; }

    std::span<const CpuParticle> particles() const;
    std::span<const unsigned int> triangleIndices() const;
    std::span<const unsigned int> wireIndices() const;
    const tc::Mesh& mesh() const { return fillMesh_; }
    const tc::Mesh& wireMesh() const { return wireMesh_; }

private:
    int index(int x, int y) const;
    bool validCoord(int x, int y) const;
    void validateAndStoreSettings(const ClothSettings& settings);
    void chooseBackend();
    void buildParticles();
    void buildTopology();
    void addConstraint(int a, int b, float stiffness, ConstraintKind kind);
    bool canUseTexturePingPong() const;
    bool shouldUseGpuThisFrame();
    void setupGpuResources();
    void releaseGpuResources();
    void markGpuPinsDirty();
    void uploadGpuPinsIfNeeded();
    void resetGpuState();
    void updateGpu(float dt);
    void stepGpu(float dt);
    void solveGpuConstraints(float dt);
    void initializeGpuState();
    void renderGpuPass(tc::Fbo& target,
                       const tc::Texture* position,
                       const tc::Texture* previous,
                       float dt,
                       int mode);
    void syncParticlesFromGpu();
    void stepCpu(float dt);
    void accumulateForcesCpu();
    void addWindForTriangleCpu(int a, int b, int c);
    void integrateCpu(float dt);
    void solveConstraintsCpu();
    void collideCpu();
    float dampingFactorForStep(float dt) const;
    void recomputeNormalsCpu();
    void rebuildMeshes();
    tc::Color shadedColor(const tc::Vec3& normal, const tc::Vec2& uv) const;
    static bool finite(const tc::Vec3& v);

    ClothSettings settings_;
    ClothSettings::SolverBackend activeBackend_ = ClothSettings::SolverBackend::CpuReference;
    std::string backendReason_ = "not initialized";

    std::vector<CpuParticle> particles_;
    std::vector<ClothConstraint> constraints_;
    std::vector<unsigned int> triangleIndices_;
    std::vector<unsigned int> wireIndices_;
    std::vector<SphereCollider> sphereColliders_;
    std::vector<PlaneCollider> planeColliders_;

    tc::Vec3 gravity_ {0.0f, 420.0f, 0.0f};
    tc::Vec3 globalForce_ {0.0f, 0.0f, 0.0f};
    tc::Vec3 windDirection_ {0.0f, 0.0f, 1.0f};
    float windStrength_ = 0.0f;
    float accumulator_ = 0.0f;

    tc::Mesh fillMesh_;
    tc::Mesh wireMesh_;

    struct GpuResources;
    std::unique_ptr<GpuResources> gpu_;
};

} // namespace tcxCloth
