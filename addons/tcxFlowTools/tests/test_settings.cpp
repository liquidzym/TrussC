#include <tcxFlowTools.h>

#include <cassert>
#include <cmath>

int main() {
    tcx::flow::FluidSettings settings;
    assert(settings.solverIterations == 20);
    settings.resolutionScale = 0.5f;

    tcx::flow::Fluid2D fluid;
    fluid.setup(320, 200, settings);
    assert(fluid.simWidth() == 160);
    assert(fluid.simHeight() == 100);
    fluid.addDensity(tc::Vec2(160, 100), 20.0f, tc::Color(1, 0, 0, 1));
    fluid.addVelocity(tc::Vec2(160, 100), 20.0f, tc::Vec2(40.0f, 0.0f));
    fluid.addTemperature(tc::Vec2(160, 100), 20.0f, 1.0f);
    fluid.addObstacle(tc::Vec2(120, 100), 12.0f);
    assert(fluid.densityEnergy() > 0.0f);
    assert(fluid.velocityEnergy() > 0.0f);
    assert(fluid.temperatureEnergy() > 0.0f);
    fluid.update(1.0f / 60.0f);
    assert(fluid.densityEnergy() > 0.0f);
    assert(fluid.pressureEnergy() > 0.0f);
    assert(std::isfinite(fluid.densityEnergy()));
    assert(std::isfinite(fluid.velocityEnergy()));
    assert(std::isfinite(fluid.pressureEnergy()));
    assert(std::isfinite(fluid.temperatureEnergy()));
    fluid.clearObstacles();

    tcx::flow::OpticalFlow opticalFlow;
    opticalFlow.setup(64, 32);
    opticalFlow.updateProcedural(1.0f, 1.0f / 60.0f);
    assert(!opticalFlow.cpuFlow().empty());
    assert(!opticalFlow.currentFrame().empty());
    assert(!opticalFlow.previousFrame().empty());
    assert(opticalFlow.currentFrameEnergy() > 0.0f);
    assert(opticalFlow.flowEnergy() > 0.0f);
    opticalFlow.updateProcedural(1.2f, 1.0f / 60.0f);
    assert(opticalFlow.flowEnergy() > 0.0f);
    fluid.applyVelocityField(opticalFlow.cpuFlow(), opticalFlow.width(), opticalFlow.height(), 0.25f);
    assert(fluid.velocityEnergy() > 0.0f);

    tcx::flow::AverageFlow averageFlow;
    averageFlow.update(fluid, 8, 6);
    assert(averageFlow.sampleCount() == 48);
    assert(std::isfinite(averageFlow.averageSpeed()));

    tcx::flow::SplitVelocity splitVelocity;
    splitVelocity.update(fluid, 8, 6);
    assert(std::isfinite(splitVelocity.result().horizontalEnergy));
    assert(std::isfinite(splitVelocity.result().verticalEnergy));
    return 0;
}
