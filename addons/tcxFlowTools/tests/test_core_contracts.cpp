#include <tcxFlowTools.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    assert(tcx::flow::textureFormatName(tc::TextureFormat::BGRA8) == std::string("BGRA8"));
    assert(tcx::flow::textureFormatName(tc::TextureFormat::RGBA16) == std::string("RGBA16"));
    assert(tcx::flow::chooseRenderableFlowFormat(false) == tc::TextureFormat::RGBA8);

    const auto addonRoot = std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR).parent_path();
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "common" / "common.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "common" / "common.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "fluid" / "fluid.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "fluid" / "fluid.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "opticalflow" / "opticalflow.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "opticalflow" / "opticalflow.glsl.h"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "bridge" / "bridge.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "bridge" / "bridge.glsl.h"));
    {
        std::ifstream bridgeShaderFile(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "bridge" / "bridge.glsl");
        assert(bridgeShaderFile.is_open());
        const std::string bridgeShader((std::istreambuf_iterator<char>(bridgeShaderFile)),
                                       std::istreambuf_iterator<char>());
        assert(bridgeShader.find("bridge_sample_uv") != std::string::npos);
        assert(bridgeShader.find("bridge_mask_value") != std::string::npos);
        assert(bridgeShader.find("bridge_mask_source_mode") != std::string::npos);
        assert(bridgeShader.find("bridge_soft_mask") != std::string::npos);
        assert(bridgeShader.find("bridge_mask_gamma") != std::string::npos);
        assert(bridgeShader.find("use_alpha") != std::string::npos);
        assert(bridgeShader.find("mirror_x") != std::string::npos);
        assert(bridgeShader.find("mirror_y") != std::string::npos);
    }
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "visualization" / "visualization.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "visualization" / "visualization.glsl.h"));
    {
        const auto visualizerHeader = addonRoot / "src" / "tcxFlow" / "Visualization" / "FlowVisualizer.h";
        std::ifstream visualizerHeaderFile(visualizerHeader);
        assert(visualizerHeaderFile.is_open());
        const std::string visualizerSource((std::istreambuf_iterator<char>(visualizerHeaderFile)),
                                           std::istreambuf_iterator<char>());
        assert(visualizerSource.find("VelocityField") != std::string::npos);
        assert(visualizerSource.find("VelocityDots") != std::string::npos);
        assert(visualizerSource.find("PressureField") != std::string::npos);
        assert(visualizerSource.find("TemperatureField") != std::string::npos);
        assert(visualizerSource.find("FlowFieldStyle") != std::string::npos);
        assert(visualizerSource.find("drawVelocityDots") != std::string::npos);
        assert(visualizerSource.find("drawPressureField") != std::string::npos);
        assert(visualizerSource.find("drawTemperatureField") != std::string::npos);
    }
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "particles" / "particles.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "particles" / "particles.glsl.h"));
    {
        std::ifstream particlesShaderFile(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "particles" / "particles.glsl");
        assert(particlesShaderFile.is_open());
        const std::string particlesShader((std::istreambuf_iterator<char>(particlesShaderFile)),
                                          std::istreambuf_iterator<char>());
        assert(particlesShader.find("particle_mass_scale") != std::string::npos);
        assert(particlesShader.find("particle_lifespan_scale") != std::string::npos);
        assert(particlesShader.find("particle_spawn_seed") != std::string::npos);
        assert(particlesShader.find("particle_birth_velocity") != std::string::npos);
        assert(particlesShader.find("particle_age_lifespan_mass_size") != std::string::npos);
        assert(particlesShader.find("particleSeedTex") == std::string::npos);
        assert(particlesShader.find("state.w") != std::string::npos);
    }
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "extensions" / "extensions.glsl"));
    assert(std::filesystem::exists(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "extensions" / "extensions.glsl.h"));
    {
        std::ifstream extensionsShaderFile(std::filesystem::path(TCX_FLOWTOOLS_SHADER_DIR) / "extensions" / "extensions.glsl");
        assert(extensionsShaderFile.is_open());
        const std::string extensionsShader((std::istreambuf_iterator<char>(extensionsShaderFile)),
                                           std::istreambuf_iterator<char>());
        assert(extensionsShader.find("fs_split_velocity_raw") != std::string::npos);
        assert(extensionsShader.find("fs_split_velocity_visual") != std::string::npos);
        assert(extensionsShader.find("fs_normalize_vector") != std::string::npos);
        assert(extensionsShader.find("fs_decay") != std::string::npos);
        assert(extensionsShader.find("fs_colorize_luminance") != std::string::npos);
        assert(extensionsShader.find("fs_colorize_velocity") != std::string::npos);
        assert(extensionsShader.find("fs_colorize_gradient") != std::string::npos);
        assert(extensionsShader.find("fs_dilate") != std::string::npos);
        assert(extensionsShader.find("fs_erode") != std::string::npos);
        assert(extensionsShader.find("fs_inverse_warp") != std::string::npos);
        assert(extensionsShader.find("fs_ease") != std::string::npos);
        assert(extensionsShader.find("fs_time_blur") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_split_velocity_raw") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_split_velocity_visual") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_normalize_vector") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_decay") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_colorize_luminance") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_colorize_velocity") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_colorize_gradient") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_dilate") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_erode") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_inverse_warp") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_ease") != std::string::npos);
        assert(extensionsShader.find("tcx_flow_extensions_time_blur") != std::string::npos);
    }
    {
        const auto helperPipelineHeader = addonRoot / "src" / "tcxFlow" / "Extensions" / "FlowHelperPipeline.h";
        assert(std::filesystem::exists(helperPipelineHeader));
        std::ifstream helperPipelineHeaderFile(helperPipelineHeader);
        assert(helperPipelineHeaderFile.is_open());
        const std::string helperPipelineSource((std::istreambuf_iterator<char>(helperPipelineHeaderFile)),
                                               std::istreambuf_iterator<char>());
        assert(helperPipelineSource.find("FlowHelperPipeline") != std::string::npos);
        assert(helperPipelineSource.find("colorizeLuminance") != std::string::npos);
        assert(helperPipelineSource.find("inverseWarp") != std::string::npos);
        assert(helperPipelineSource.find("timeBlur") != std::string::npos);
    }
    {
        const auto splitVelocityHeader = addonRoot / "src" / "tcxFlow" / "Extensions" / "SplitVelocity.h";
        std::ifstream splitVelocityHeaderFile(splitVelocityHeader);
        assert(splitVelocityHeaderFile.is_open());
        const std::string splitVelocitySource((std::istreambuf_iterator<char>(splitVelocityHeaderFile)),
                                              std::istreambuf_iterator<char>());
        assert(splitVelocitySource.find("SplitVelocityFieldStyle") != std::string::npos);
        assert(splitVelocitySource.find("drawField") != std::string::npos);
    }
    const auto streamlinesExample = addonRoot / "examples" / "example-fluid-streamlines";
    assert(std::filesystem::exists(streamlinesExample / "CMakeLists.txt"));
    assert(std::filesystem::exists(streamlinesExample / "addons.make"));
    assert(std::filesystem::exists(streamlinesExample / "src" / "tcApp.cpp"));
    assert(std::filesystem::exists(streamlinesExample / "src" / "tcApp.h"));
    assert(std::filesystem::exists(streamlinesExample / "src" / "main.cpp"));
    std::ifstream streamlinesSourceFile(streamlinesExample / "src" / "tcApp.cpp");
    assert(streamlinesSourceFile.is_open());
    const std::string streamlinesSource((std::istreambuf_iterator<char>(streamlinesSourceFile)),
                                        std::istreambuf_iterator<char>());
    assert(streamlinesSource.find("p pause") != std::string::npos);
    assert(streamlinesSource.find("m mode") != std::string::npos);
    assert(streamlinesSource.find("v vectors") != std::string::npos);
    assert(streamlinesSource.find("a particles") != std::string::npos);
    assert(streamlinesSource.find("line length") != std::string::npos);
    const auto customParticlesExample = addonRoot / "examples" / "example-fluid-custom-particles";
    assert(std::filesystem::exists(customParticlesExample / "CMakeLists.txt"));
    assert(std::filesystem::exists(customParticlesExample / "addons.make"));
    assert(std::filesystem::exists(customParticlesExample / "src" / "tcApp.cpp"));
    std::ifstream customParticlesSourceFile(customParticlesExample / "src" / "tcApp.cpp");
    assert(customParticlesSourceFile.is_open());
    const std::string customParticlesSource((std::istreambuf_iterator<char>(customParticlesSourceFile)),
                                            std::istreambuf_iterator<char>());
    assert(customParticlesSource.find("left velocity+particles") != std::string::npos);
    assert(customParticlesSource.find("middle heat+particles") != std::string::npos);
    assert(customParticlesSource.find("right particles") != std::string::npos);
    assert(customParticlesSource.find("spawn requests stay independent") != std::string::npos);
    const auto averageFlowExample = addonRoot / "examples" / "example-average-flow";
    assert(std::filesystem::exists(averageFlowExample / "CMakeLists.txt"));
    assert(std::filesystem::exists(averageFlowExample / "addons.make"));
    assert(std::filesystem::exists(averageFlowExample / "src" / "tcApp.cpp"));
    std::ifstream averageFlowSourceFile(averageFlowExample / "src" / "tcApp.cpp");
    assert(averageFlowSourceFile.is_open());
    const std::string averageFlowSource((std::istreambuf_iterator<char>(averageFlowSourceFile)),
                                        std::istreambuf_iterator<char>());
    assert(averageFlowSource.find("ofxFlowTools AverageFlowWatcher parity") != std::string::npos);
    assert(averageFlowSource.find("ROI average") != std::string::npos);
    assert(averageFlowSource.find("magnitude event") != std::string::npos);
    assert(averageFlowSource.find("persistent settings") != std::string::npos);
    const auto bridgeExample = addonRoot / "examples" / "example-fluid-bridges";
    assert(std::filesystem::exists(bridgeExample / "src" / "tcApp.cpp"));
    std::ifstream bridgeExampleSourceFile(bridgeExample / "src" / "tcApp.cpp");
    assert(bridgeExampleSourceFile.is_open());
    const std::string bridgeExampleSource((std::istreambuf_iterator<char>(bridgeExampleSourceFile)),
                                          std::istreambuf_iterator<char>());
    assert(bridgeExampleSource.find("invert") != std::string::npos);
    assert(bridgeExampleSource.find("alpha-mask") != std::string::npos);
    assert(bridgeExampleSource.find("mirror x") != std::string::npos);
    assert(bridgeExampleSource.find("mirror y") != std::string::npos);
    assert(bridgeExampleSource.find("mask-source") != std::string::npos);
    assert(bridgeExampleSource.find("softness") != std::string::npos);
    const auto windTunnelExample = addonRoot / "examples" / "example-wind-tunnel";
    std::ifstream windTunnelSourceFile(windTunnelExample / "src" / "tcApp.cpp");
    assert(windTunnelSourceFile.is_open());
    const std::string windTunnelSource((std::istreambuf_iterator<char>(windTunnelSourceFile)),
                                       std::istreambuf_iterator<char>());
    assert(windTunnelSource.find("velocity field") != std::string::npos);
    assert(windTunnelSource.find("velocity dots") != std::string::npos);
    assert(windTunnelSource.find("pressure field") != std::string::npos);
    assert(windTunnelSource.find("temperature field") != std::string::npos);
    assert(windTunnelSource.find("FlowFieldStyle") != std::string::npos);

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
             tcx::flow::FlowPassKind::ExtensionNormalizeVector,
             tcx::flow::FlowPassKind::ExtensionDecay,
             tcx::flow::FlowPassKind::ExtensionSplitVelocityVisual,
             tcx::flow::FlowPassKind::ExtensionColorizeLuminance,
             tcx::flow::FlowPassKind::ExtensionColorizeVelocity,
             tcx::flow::FlowPassKind::ExtensionColorizeGradient,
             tcx::flow::FlowPassKind::ExtensionDilate,
             tcx::flow::FlowPassKind::ExtensionErode,
             tcx::flow::FlowPassKind::ExtensionInverseWarp,
             tcx::flow::FlowPassKind::ExtensionEase,
             tcx::flow::FlowPassKind::ExtensionTimeBlur,
         }) {
        tcx::flow::FlowPass pass;
        pass.setup(kind);
        assert(pass.isReady());
        assert(pass.kind() == kind);
    }

    static_assert(std::is_move_constructible_v<tcx::flow::FlowHelperPipeline>);
    static_assert(std::is_move_assignable_v<tcx::flow::FlowHelperPipeline>);

    return 0;
}
