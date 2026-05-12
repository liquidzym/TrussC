#pragma once

#include "../Fluid/Fluid2D.h"
#include "../OpticalFlow/OpticalFlow.h"

namespace tcx::flow {

class FlowVisualizer {
public:
    enum class Mode {
        Density,
        Velocity,
        Pressure,
        Temperature
    };

    void draw(const Fluid2D& fluid, float x, float y, float w, float h, Mode mode = Mode::Density) const;
    void draw(const OpticalFlow& opticalFlow, float x, float y, float w, float h) const;
};

using DensityVisualizer = FlowVisualizer;
using VelocityFieldVisualizer = FlowVisualizer;
using PressureVisualizer = FlowVisualizer;
using TemperatureVisualizer = FlowVisualizer;

} // namespace tcx::flow
