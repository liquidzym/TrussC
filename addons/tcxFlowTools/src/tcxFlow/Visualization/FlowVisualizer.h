#pragma once

#include "../Fluid/Fluid2D.h"
#include "../OpticalFlow/OpticalFlow.h"

namespace tcx::flow {

struct FlowFieldStyle {
    int columns = 32;
    int rows = 18;
    float velocityScale = 0.055f;
    float minDotSize = 1.5f;
    float maxDotSize = 9.0f;
    float dotScale = 0.035f;
    float pressureScale = 8.0f;
    float temperatureScale = 1.0f;
    float alpha = 0.82f;
    float meshAlpha = 0.16f;
    bool drawMesh = true;
};

class FlowVisualizer {
public:
    enum class Mode {
        Density,
        Velocity,
        VelocityField,
        VelocityDots,
        Pressure,
        PressureField,
        Temperature,
        TemperatureField
    };

    void draw(const Fluid2D& fluid, float x, float y, float w, float h, Mode mode = Mode::Density) const;
    void draw(const OpticalFlow& opticalFlow, float x, float y, float w, float h) const;
    void drawVelocityField(const Fluid2D& fluid, float x, float y, float w, float h,
                           const FlowFieldStyle& style) const;
    void drawVelocityField(const Fluid2D& fluid, float x, float y, float w, float h,
                           int columns = 32, int rows = 18, float scale = 0.055f) const;
    void drawVelocityDots(const Fluid2D& fluid, float x, float y, float w, float h,
                          const FlowFieldStyle& style) const;
    void drawVelocityDots(const Fluid2D& fluid, float x, float y, float w, float h,
                          int columns = 48, int rows = 27, float minDotSize = 1.5f,
                          float maxDotSize = 9.0f, float scale = 0.035f) const;
    void drawPressureField(const Fluid2D& fluid, float x, float y, float w, float h,
                           const FlowFieldStyle& style = {}) const;
    void drawTemperatureField(const Fluid2D& fluid, float x, float y, float w, float h,
                              const FlowFieldStyle& style = {}) const;
};

using DensityVisualizer = FlowVisualizer;
using VelocityFieldVisualizer = FlowVisualizer;
using PressureVisualizer = FlowVisualizer;
using TemperatureVisualizer = FlowVisualizer;

} // namespace tcx::flow
