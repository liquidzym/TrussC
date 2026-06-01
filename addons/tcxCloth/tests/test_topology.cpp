#include "test_common.h"

#include <iostream>
#include <string>

int main() {
    tcxCloth::ClothSettings settings;
    settings.columns = 5;
    settings.rows = 4;
    settings.width = 80.0f;
    settings.height = 60.0f;
    settings.backend = tcxCloth::ClothSettings::SolverBackend::CpuReference;

    tcxCloth::Cloth cloth;
    cloth.setup(settings);

    require(cloth.columns() == 5, "columns are preserved");
    require(cloth.rows() == 4, "rows are preserved");
    require(cloth.particleCount() == 20, "particle count is columns * rows");
    require(cloth.triangleIndexCount() == 72, "triangle index count follows grid topology");
    require(cloth.wireIndexCount() == 62, "wire index count follows horizontal and vertical edges");
    require(cloth.constraintCount() == 89, "constraints include structural, shear, axial bend, and diagonal bend");
    require(cloth.activeBackend() == tcxCloth::ClothSettings::SolverBackend::CpuReference,
            "forced CPU backend is active");

    tcxCloth::ClothSettings gpuSettings = settings;
    gpuSettings.backend = tcxCloth::ClothSettings::SolverBackend::TexturePingPong;
    tcxCloth::Cloth gpuRequested;
    gpuRequested.setup(gpuSettings);
    require(gpuRequested.activeBackend() == tcxCloth::ClothSettings::SolverBackend::CpuReference,
            "TexturePingPong request falls back in headless tests");
    require(std::string(gpuRequested.backendReason()).find("GPU unavailable") != std::string::npos,
            "TexturePingPong fallback explains that GPU is unavailable");

    const auto particles = cloth.particles();
    require(particles.size() == 20, "particle span exposes topology");
    for (const auto& particle : particles) {
        require(finite(particle.position), "initial positions are finite");
        require(finite(particle.previousPosition), "initial previous positions are finite");
        require(particle.uv.x >= 0.0f && particle.uv.x <= 1.0f, "uv x is in range");
        require(particle.uv.y >= 0.0f && particle.uv.y <= 1.0f, "uv y is in range");
    }

    const auto tri = cloth.triangleIndices();
    require(tri.size() == 72, "triangle index span has expected length");
    require(tri[0] == 0 && tri[1] == 5 && tri[2] == 1, "first triangle winding matches taskbook");
    require(tri[3] == 1 && tri[4] == 5 && tri[5] == 6, "second triangle winding matches taskbook");

    const auto wire = cloth.wireIndices();
    require(wire.size() == 62, "wire index span has expected length");
    require(wire[0] == 0 && wire[1] == 1, "first wire edge is horizontal");

    std::cout << "tcxCloth_topology passed\n";
    return 0;
}
