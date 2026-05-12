#pragma once

namespace tcx::flow {

struct FluidSettings {
    int solverIterations = 20;
    float timestep = 0.125f;
    float resolutionScale = 1.0f;
    float velocityDissipation = 0.99f;
    float densityDissipation = 0.995f;
    float temperatureDissipation = 0.99f;
    float pressureDissipation = 0.98f;
    float vorticity = 0.2f;
    float viscosity = 0.0f;
    float buoyancy = 0.0f;
    float densityWeight = 0.0f;
    float inputVelocityScale = 1.0f;
    float inputDensityScale = 1.0f;
    float inputTemperatureScale = 1.0f;
    bool enableVorticity = true;
    bool enableTemperature = true;
    bool enableObstacles = false;
    bool enableBuoyancy = false;
    bool autoClearForces = true;
    bool useGpu = true;
    bool allowCpuFallback = true;
};

} // namespace tcx::flow
