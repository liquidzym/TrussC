#include "FlowVisualizer.h"

namespace tcx::flow {

void FlowVisualizer::draw(const Fluid2D& fluid, float x, float y, float w, float h, Mode mode) const {
    switch (mode) {
        case Mode::Density: fluid.drawDensity(x, y, w, h); break;
        case Mode::Velocity: fluid.drawVelocity(x, y, w, h); break;
        case Mode::Pressure: fluid.drawPressure(x, y, w, h); break;
        case Mode::Temperature: fluid.drawTemperature(x, y, w, h); break;
    }
}

void FlowVisualizer::draw(const OpticalFlow& opticalFlow, float x, float y, float w, float h) const {
    opticalFlow.drawFlow(x, y, w, h);
}

} // namespace tcx::flow
