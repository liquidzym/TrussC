#include <tcxFlowTools.h>

#include <cassert>
#include <cmath>
#include <string>

int main() {
    tcx::flow::FluidSettings settings;
    assert(settings.solverIterations == 20);
    settings.resolutionScale = 0.5f;

    tcx::flow::Fluid2D fluid;
    fluid.setup(320, 200, settings);
    assert(fluid.simWidth() == 160);
    assert(fluid.simHeight() == 100);
    assert(fluid.outputWidth() == 160);
    assert(fluid.outputHeight() == 100);
    settings.outputResolutionScale = 1.0f;
    fluid.setup(320, 200, settings);
    assert(fluid.simWidth() == 160);
    assert(fluid.simHeight() == 100);
    assert(fluid.outputWidth() == 320);
    assert(fluid.outputHeight() == 200);
    fluid.addDensity(tc::Vec2(160, 100), 20.0f, tc::Color(1, 0, 0, 1));
    fluid.addVelocity(tc::Vec2(160, 100), 20.0f, tc::Vec2(40.0f, 0.0f));
    fluid.addTemperature(tc::Vec2(160, 100), 20.0f, 1.0f);
    fluid.addObstacle(tc::Vec2(120, 100), 12.0f);
    assert(fluid.densityEnergy() > 0.0f);
    assert(fluid.velocityEnergy() > 0.0f);
    assert(fluid.temperatureEnergy() > 0.0f);
    fluid.update(1.0f / 60.0f);
    assert(fluid.refreshVelocityReadback());
    assert(std::isfinite(fluid.sampleVelocityAtPosition(tc::Vec2(160, 100)).x));
    assert(std::isfinite(fluid.sampleVelocityAtPosition(tc::Vec2(160, 100)).y));
    assert(std::isfinite(fluid.samplePressureAtPosition(tc::Vec2(160, 100))));
    assert(std::isfinite(fluid.sampleTemperatureAtPosition(tc::Vec2(160, 100))));
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
    averageFlow.setRoi(0.2f, 0.1f, 0.5f, 0.6f);
    averageFlow.setNormalization(0.25f);
    averageFlow.setEventThreshold(0.02f);
    averageFlow.setEventBase(0.20f);
    averageFlow.update(fluid, 8, 6);
    assert(averageFlow.sampleCount() == 48);
    assert(std::isfinite(averageFlow.averageSpeed()));
    assert(std::isfinite(averageFlow.magnitude()));
    assert(std::isfinite(averageFlow.velocity().x));
    assert(averageFlow.roi().x == 0.2f);
    assert(averageFlow.eventThreshold() == 0.02f);
    assert(!averageFlow.history().empty());
    averageFlow.setHistoryCapacity(3);
    averageFlow.pushHistorySample(0.10f);
    averageFlow.pushHistorySample(0.20f);
    averageFlow.pushHistorySample(0.30f);
    averageFlow.pushHistorySample(0.40f);
    assert(averageFlow.history().size() == 3);
    auto averageSettings = averageFlow.settingsSnapshot();
    averageSettings.roi = {0.1f, 0.2f, 0.3f, 0.4f};
    averageSettings.normalization = 0.55f;
    averageSettings.eventThreshold = 0.12f;
    averageSettings.eventBase = 0.44f;
    averageSettings.historyCapacity = 8;
    averageFlow.applySettings(averageSettings);
    assert(averageFlow.roi().x == 0.1f);
    assert(averageFlow.historyCapacity() == 8);
    const std::string serializedAverage = averageFlow.serializeSettings();
    averageFlow.setRoi(0.0f, 0.0f, 1.0f, 1.0f);
    assert(averageFlow.applySettingsString(serializedAverage));
    assert(averageFlow.roi().x == 0.1f);
    assert(averageFlow.historyCapacity() == 8);

    tcx::flow::SplitVelocity splitVelocity;
    splitVelocity.setForce(1.25f);
    splitVelocity.setNormalizeRange(0.75f);
    splitVelocity.setNormalizeMin(0.01f);
    splitVelocity.setDecay(0.12f);
    splitVelocity.update(fluid, 8, 6);
    assert(splitVelocity.force() == 1.25f);
    assert(splitVelocity.normalizeRange() == 0.75f);
    assert(splitVelocity.normalizeMin() == 0.01f);
    assert(splitVelocity.decay() == 0.12f);
    assert(std::isfinite(splitVelocity.result().horizontalEnergy));
    assert(std::isfinite(splitVelocity.result().verticalEnergy));
    tcx::flow::SplitVelocityFieldStyle splitFieldStyle;
    splitFieldStyle.columns = 11;
    splitFieldStyle.rows = 7;
    splitFieldStyle.scale = 0.18f;
    splitVelocity.setFieldStyle(splitFieldStyle);
    assert(splitVelocity.fieldStyle().columns == 11);
    assert(splitVelocity.fieldStyle().rows == 7);
    assert(splitVelocity.fieldStyle().scale == 0.18f);
    assert(splitVelocity.splitTexture() == nullptr);
    assert(splitVelocity.normalizedTexture() == nullptr);
    assert(splitVelocity.trailTexture() == nullptr);

    tcx::flow::FlowFieldStyle visualizerStyle;
    visualizerStyle.columns = 14;
    visualizerStyle.rows = 9;
    visualizerStyle.pressureScale = 3.5f;
    visualizerStyle.temperatureScale = 1.7f;
    assert(visualizerStyle.columns == 14);
    assert(visualizerStyle.pressureScale == 3.5f);

    tcx::flow::BridgeSettings bridgeSettings;
    bridgeSettings.maskSource = tcx::flow::BridgeMaskSource::Saturation;
    bridgeSettings.maskSoftness = 0.08f;
    bridgeSettings.maskGamma = 1.7f;
    assert(bridgeSettings.maskSource == tcx::flow::BridgeMaskSource::Saturation);
    assert(bridgeSettings.maskSoftness == 0.08f);
    assert(bridgeSettings.maskGamma == 1.7f);

    tcx::flow::FlowHelperSettings helperSettings;
    helperSettings.gain = 0.5f;
    helperSettings.radius = 2.0f;
    helperSettings.sourceGain = 0.75f;
    tcx::flow::FlowHelperPipeline helperPipeline;
    assert(helperPipeline.outputTexture() == nullptr);
    assert(helperSettings.gain == 0.5f);

    tcx::flow::ParticleFlowSettings particleSettings;
    particleSettings.useGpuParticles = false;
    particleSettings.particleCount = 128;
    particleSettings.spawnRadius = 24.0f;
    particleSettings.lifespanSpread = 0.25f;
    particleSettings.mass = 1.5f;
    particleSettings.massSpread = 0.40f;
    particleSettings.sizeSpread = 0.30f;
    particleSettings.birthFromVelocity = true;
    particleSettings.birthVelocityScale = 1.8f;
    particleSettings.birthVelocityJitter = 0.15f;
    particleSettings.ageFadePower = 1.25f;
    tcx::flow::ParticleFlow particles;
    particles.setup(320, 200, particleSettings);
    assert(particles.particleCount() == 128);
    assert(particles.settings().lifespanSpread == 0.25f);
    assert(particles.settings().mass == 1.5f);
    assert(particles.settings().massSpread == 0.40f);
    assert(particles.settings().sizeSpread == 0.30f);
    assert(particles.settings().birthFromVelocity);
    assert(particles.settings().birthVelocityScale == 1.8f);
    assert(particles.settings().birthVelocityJitter == 0.15f);
    assert(particles.settings().ageFadePower == 1.25f);
    particles.spawn(tc::Vec2(160, 100), 30.0f, 24);
    particles.update(fluid, 1.0f / 60.0f);
    assert(particles.particleCount() == 128);
    return 0;
}
