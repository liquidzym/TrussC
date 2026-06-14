#pragma once

namespace tcx::flow {

enum class FluidVelocitySplatMode {
    MaxMagnitudeMix,
    AdditiveClamp
};

struct FluidSettings {
    int solverIterations = 20;
    float timestep = 0.125f;
    float resolutionScale = 1.0f;
    float outputResolutionScale = 0.0f;
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
    float velocitySplatClamp = 0.0f;
    FluidVelocitySplatMode velocitySplatMode = FluidVelocitySplatMode::MaxMagnitudeMix;
    bool enableVorticity = true;
    bool enableTemperature = true;
    bool enableObstacles = false;
    bool enableBuoyancy = false;
    bool autoClearForces = true;
    bool persistentPressure = false;
    bool wrapEdges = false;
    bool useGpu = true;
    bool allowCpuFallback = true;
};

struct FluidDisplaySettings {
    bool bloom = false;
    float baseGain = 1.0f;
    float bloomIntensity = 0.80f;
    float bloomThreshold = 0.60f;
    float bloomSoftKnee = 0.70f;
    float bloomPrefilterGain = 1.0f;
    float bloomSaturation = 1.15f;
    float bloomExposure = 0.0f;
    float bloomBlurRadius = 1.8f;
    float bloomResolutionScale = 0.50f;
    int bloomIterations = 4;
};

} // namespace tcx::flow
