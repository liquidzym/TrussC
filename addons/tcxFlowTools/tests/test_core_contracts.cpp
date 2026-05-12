#include <tcxFlowTools.h>

#include <cassert>
#include <filesystem>
#include <type_traits>

int main() {
    static_assert(!std::is_copy_constructible_v<tcx::flow::PingPongBuffer>);
    static_assert(!std::is_copy_assignable_v<tcx::flow::PingPongBuffer>);
    static_assert(std::is_move_constructible_v<tcx::flow::PingPongBuffer>);
    static_assert(std::is_move_assignable_v<tcx::flow::PingPongBuffer>);
    static_assert(!std::is_copy_constructible_v<tcx::flow::FluidBuffers>);
    static_assert(!std::is_copy_assignable_v<tcx::flow::FluidBuffers>);
    static_assert(std::is_move_constructible_v<tcx::flow::FluidBuffers>);
    static_assert(std::is_move_assignable_v<tcx::flow::FluidBuffers>);
    static_assert(!std::is_copy_constructible_v<tcx::flow::FlowPass>);
    static_assert(!std::is_copy_assignable_v<tcx::flow::FlowPass>);
    static_assert(std::is_move_constructible_v<tcx::flow::FlowPass>);
    static_assert(std::is_move_assignable_v<tcx::flow::FlowPass>);

    assert(tcx::flow::textureFormatName(tc::TextureFormat::RGBA8) == std::string("RGBA8"));
    assert(tcx::flow::chooseRenderableFlowFormat(false) == tc::TextureFormat::RGBA8);

    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "common" / "common.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "common" / "common.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "fluid" / "fluid.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "fluid" / "fluid.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "opticalflow" / "opticalflow.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "opticalflow" / "opticalflow.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "bridge" / "bridge.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "bridge" / "bridge.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "visualization" / "visualization.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "visualization" / "visualization.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "particles" / "particles.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "particles" / "particles.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "extensions" / "extensions.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "extensions" / "extensions.glsl.h"));

    tc::headless::active = true;
    tcx::flow::FluidBuffers fluidBuffers;
    fluidBuffers.allocate(96, 48);
    assert(fluidBuffers.isAllocated());
    assert(fluidBuffers.width() == 96);
    assert(fluidBuffers.height() == 48);
    assert(fluidBuffers.velocity().isAllocated());
    assert(fluidBuffers.density().isAllocated());
    assert(fluidBuffers.pressure().isAllocated());
    fluidBuffers.resize(64, 32);
    assert(fluidBuffers.width() == 64);
    assert(fluidBuffers.height() == 32);
    fluidBuffers.clear();
    fluidBuffers.release();
    assert(!fluidBuffers.isAllocated());
    tc::headless::active = false;

    tcx::flow::FlowPass emptyPass;
    emptyPass.setup("");
    assert(!emptyPass.isReady());
    assert(!emptyPass.lastError().empty());

    tcx::flow::FlowPass namedPass;
    namedPass.setup("shaders/common/copy.glsl", "copy");
    assert(namedPass.isReady());
    assert(namedPass.label() == std::string("copy"));
    assert(namedPass.kind() == tcx::flow::FlowPassKind::Copy);

    tcx::flow::FlowPass clearPass;
    clearPass.setup(tcx::flow::FlowPassKind::Clear);
    assert(clearPass.isReady());
    assert(clearPass.label() == std::string("clear"));
    clearPass.setColor(tc::Color(0.25f, 0.5f, 0.75f, 1.0f));
    assert(clearPass.params().color[0] == 0.25f);
    assert(clearPass.params().color[1] == 0.5f);

    tcx::flow::FlowPass thresholdPass;
    thresholdPass.setup("threshold.glsl", "threshold");
    thresholdPass.setGain(2.0f);
    thresholdPass.setThreshold(0.35f);
    thresholdPass.setBlurRadius(3.0f);
    thresholdPass.setResolution(320, 160);
    assert(thresholdPass.isReady());
    assert(thresholdPass.kind() == tcx::flow::FlowPassKind::Threshold);
    assert(thresholdPass.params().options[0] == 2.0f);
    assert(thresholdPass.params().options[1] == 0.35f);
    assert(thresholdPass.params().options[2] == 3.0f);
    assert(thresholdPass.params().resolution[0] == 320.0f);
    assert(thresholdPass.params().texel[1] == 1.0f / 160.0f);

    for (auto kind : {
             tcx::flow::FlowPassKind::Multiply,
             tcx::flow::FlowPassKind::Luminance,
             tcx::flow::FlowPassKind::Difference,
             tcx::flow::FlowPassKind::BlurHorizontal,
             tcx::flow::FlowPassKind::BlurVertical,
             tcx::flow::FlowPassKind::FluidAdvect,
             tcx::flow::FlowPassKind::FluidSplat,
             tcx::flow::FlowPassKind::FluidAddVelocity,
             tcx::flow::FlowPassKind::FluidAddDensityTexture,
             tcx::flow::FlowPassKind::FluidAddTemperatureTexture,
             tcx::flow::FlowPassKind::FluidDivergence,
             tcx::flow::FlowPassKind::FluidJacobiPressure,
             tcx::flow::FlowPassKind::FluidGradientSubtract,
             tcx::flow::FlowPassKind::FluidVorticityCurl,
             tcx::flow::FlowPassKind::FluidVorticityForce,
             tcx::flow::FlowPassKind::FluidBuoyancy,
             tcx::flow::FlowPassKind::FluidDiffuse,
             tcx::flow::FlowPassKind::FluidObstacleSplat,
             tcx::flow::FlowPassKind::FluidApplyObstacle,
             tcx::flow::FlowPassKind::OpticalLuminance,
             tcx::flow::FlowPassKind::OpticalDifference,
             tcx::flow::FlowPassKind::OpticalGradient,
             tcx::flow::FlowPassKind::OpticalFlow,
             tcx::flow::FlowPassKind::OpticalTemporalSmooth,
             tcx::flow::FlowPassKind::OpticalVisualize,
             tcx::flow::FlowPassKind::BridgeLuminanceMask,
             tcx::flow::FlowPassKind::BridgeVelocity,
             tcx::flow::FlowPassKind::BridgeDensity,
             tcx::flow::FlowPassKind::BridgeTemperature,
             tcx::flow::FlowPassKind::VisualizeScalar,
             tcx::flow::FlowPassKind::VisualizeDensity,
             tcx::flow::FlowPassKind::VisualizeVelocityColor,
             tcx::flow::FlowPassKind::VisualizePressure,
             tcx::flow::FlowPassKind::VisualizeTemperature,
             tcx::flow::FlowPassKind::VisualizeCombined,
             tcx::flow::FlowPassKind::VisualizeLic,
             tcx::flow::FlowPassKind::ParticlesSpawn,
             tcx::flow::FlowPassKind::ParticlesUpdate,
             tcx::flow::FlowPassKind::ParticlesRender,
             tcx::flow::FlowPassKind::ExtensionSplitVelocity,
         }) {
        tcx::flow::FlowPass pass;
        pass.setup(kind);
        assert(pass.isReady());
        assert(pass.kind() == kind);
    }

    return 0;
}
