#include "tcApp.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace {
enum class ProfileKind {
    Ideogram4,
    Flux2Klein,
    ZImageTurbo,
};

struct ModelProfile {
    ProfileKind kind;
    const char* id;
    const char* label;
    const char* promptProfile;
    const char* promptKind;
    int width;
    int height;
    int steps;
    int seed;
    float cfg;
    bool supportsIdeogramComposer;
    const char* subject;
    const char* visibleText;
    const char* style;
    const char* palette;
    const char* prompt;
    const char* negative;
};

const std::array<ModelProfile, 3> kProfiles = {{
    {
        ProfileKind::Ideogram4,
        "ideogram4-q4_0",
        "Ideogram4 Q4_0",
        "ideogram4",
        "poster",
        1024,
        1024,
        8,
        -1,
        1.0f,
        true,
        "A clean futuristic product poster for tcxStableDiffusion, a modular local AI image addon for creative coders",
        "tcxStableDiffusion",
        "premium technical product poster, refined typography, elegant studio lighting, precise interface details",
        "#F7F4EC, #111111, #2F80ED, #27AE60, #FFFFFF",
        "",
        "low quality, blurry, cluttered layout, distorted anatomy, duplicate subjects, watermark, signature, misspelled text, unreadable text, cropped text, mirrored text, rotated text, extra words",
    },
    {
        ProfileKind::Flux2Klein,
        "flux2-klein-4b-q4_0",
        "FLUX.2-klein 4B Q4_0",
        "flux2-klein",
        "product",
        512,
        512,
        4,
        2048,
        1.0f,
        false,
        "A modular local AI image generation addon running on a Windows workstation",
        "tcxStableDiffusion",
        "clean product visualization, crisp interface panels, refined studio lighting, professional software-tool aesthetic",
        "#F5F7FA, #111111, #2F80ED, #00A878, #FFFFFF",
        "A clean product visualization of a modular local AI image generation addon running on a Windows workstation, crisp interface panels, refined studio lighting, professional software-tool aesthetic",
        "low quality, blurry, noisy text, cluttered layout, watermark, signature",
    },
    {
        ProfileKind::ZImageTurbo,
        "z-image-turbo-q3_k",
        "Z-Image Turbo Q3_K",
        "z-image",
        "wide-scene",
        1024,
        512,
        8,
        4096,
        1.0f,
        false,
        "A local creative coding studio with a Windows CUDA workstation",
        "tcxStableDiffusion",
        "cinematic wide composition, polished technical atmosphere, clear subject hierarchy, refined color and lighting",
        "#F2F0EA, #151515, #3A7BD5, #36C486, #FFFFFF",
        "A cinematic wide composition of a local creative coding studio, a Windows CUDA workstation generating images in real time, polished technical atmosphere, clear subject hierarchy, refined color and lighting",
        "low quality, blurry, distorted perspective, clutter, watermark, signature",
    },
}};

constexpr float kPanelWidth = 460.0f;

std::filesystem::path exampleRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
}

const ModelProfile& profileAt(int index) {
    const int safeIndex = std::clamp(index, 0, static_cast<int>(kProfiles.size()) - 1);
    return kProfiles[static_cast<size_t>(safeIndex)];
}

int profileIndexForId(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    for (size_t i = 0; i < kProfiles.size(); ++i) {
        const std::string id = kProfiles[i].id;
        const std::string profile = kProfiles[i].promptProfile;
        if (text == id || text == profile) {
            return static_cast<int>(i);
        }
    }
    if (text == "flux" || text == "flux2" || text == "klein") {
        return 1;
    }
    if (text == "z" || text == "z-image" || text == "zimage") {
        return 2;
    }
    return 0;
}

bool envEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (!value) {
        return false;
    }
    std::string text = value;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text == "1" || text == "true" || text == "yes" || text == "on";
}

bool envExists(const char* name) {
    return std::getenv(name) != nullptr;
}

int envInt(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value || !*value) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::string trim(std::string text) {
    auto isSpace = [](unsigned char c) {
        return std::isspace(c) != 0;
    };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

std::vector<std::string> splitCsv(const std::string& text) {
    std::vector<std::string> values;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = trim(std::move(item));
        if (!item.empty()) {
            values.push_back(std::move(item));
        }
    }
    return values;
}

template <std::size_t N>
void copyText(std::array<char, N>& target, const std::string& text) {
    std::memset(target.data(), 0, target.size());
    std::strncpy(target.data(), text.c_str(), target.size() - 1);
}

ImVec4 colorFromBytes(int r, int g, int b, float a = 1.0f) {
    return ImVec4(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        a);
}

bool pathExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

std::vector<std::filesystem::path> cjkFontCandidates() {
#if defined(_WIN32)
    return {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/NotoSansSC-VF.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/Deng.ttf",
    };
#elif defined(__APPLE__)
    return {
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
    };
#else
    return {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    };
#endif
}

void setupImGuiFont() {
    auto& io = ImGui::GetIO();
    io.FontAllowUserScaling = false;

    for (const auto& candidate : cjkFontCandidates()) {
        if (!pathExists(candidate)) {
            continue;
        }

        const std::string fontPath = candidate.generic_string();
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 17.0f)) {
            io.FontDefault = font;
            tc::logNotice() << "Loaded ImGui CJK font: " << fontPath;
            return;
        }
        tc::logWarning() << "Failed to load ImGui CJK font candidate: " << fontPath;
    }

    tc::logWarning() << "No CJK ImGui font found; Chinese labels may render as question marks.";
}

void setupImGuiStyle() {
    ImGui::StyleColorsDark();

    auto& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(9.0f, 9.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 7.0f);
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 12.0f;
    style.WindowRounding = 7.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = colorFromBytes(232, 238, 247);
    colors[ImGuiCol_TextDisabled] = colorFromBytes(126, 137, 153);
    colors[ImGuiCol_WindowBg] = colorFromBytes(13, 17, 23, 0.97f);
    colors[ImGuiCol_ChildBg] = colorFromBytes(18, 24, 33, 0.88f);
    colors[ImGuiCol_PopupBg] = colorFromBytes(19, 25, 34, 0.98f);
    colors[ImGuiCol_Border] = colorFromBytes(45, 56, 72, 0.86f);
    colors[ImGuiCol_BorderShadow] = colorFromBytes(0, 0, 0, 0.0f);
    colors[ImGuiCol_FrameBg] = colorFromBytes(29, 41, 58, 0.96f);
    colors[ImGuiCol_FrameBgHovered] = colorFromBytes(42, 61, 85, 1.0f);
    colors[ImGuiCol_FrameBgActive] = colorFromBytes(49, 78, 112, 1.0f);
    colors[ImGuiCol_TitleBg] = colorFromBytes(11, 15, 21, 1.0f);
    colors[ImGuiCol_TitleBgActive] = colorFromBytes(18, 28, 41, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = colorFromBytes(11, 15, 21, 0.86f);
    colors[ImGuiCol_MenuBarBg] = colorFromBytes(18, 24, 33, 1.0f);
    colors[ImGuiCol_ScrollbarBg] = colorFromBytes(10, 14, 20, 0.78f);
    colors[ImGuiCol_ScrollbarGrab] = colorFromBytes(54, 69, 89, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = colorFromBytes(75, 94, 121, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = colorFromBytes(89, 112, 144, 1.0f);
    colors[ImGuiCol_CheckMark] = colorFromBytes(57, 217, 138, 1.0f);
    colors[ImGuiCol_SliderGrab] = colorFromBytes(96, 165, 250, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = colorFromBytes(45, 212, 191, 1.0f);
    colors[ImGuiCol_Button] = colorFromBytes(37, 74, 120, 0.94f);
    colors[ImGuiCol_ButtonHovered] = colorFromBytes(50, 101, 163, 1.0f);
    colors[ImGuiCol_ButtonActive] = colorFromBytes(42, 151, 132, 1.0f);
    colors[ImGuiCol_Header] = colorFromBytes(35, 58, 89, 0.95f);
    colors[ImGuiCol_HeaderHovered] = colorFromBytes(50, 84, 128, 1.0f);
    colors[ImGuiCol_HeaderActive] = colorFromBytes(42, 151, 132, 1.0f);
    colors[ImGuiCol_Separator] = colorFromBytes(56, 68, 86, 0.72f);
    colors[ImGuiCol_SeparatorHovered] = colorFromBytes(96, 165, 250, 0.88f);
    colors[ImGuiCol_SeparatorActive] = colorFromBytes(45, 212, 191, 1.0f);
    colors[ImGuiCol_ResizeGrip] = colorFromBytes(96, 165, 250, 0.24f);
    colors[ImGuiCol_ResizeGripHovered] = colorFromBytes(96, 165, 250, 0.56f);
    colors[ImGuiCol_ResizeGripActive] = colorFromBytes(45, 212, 191, 0.78f);
    colors[ImGuiCol_Tab] = colorFromBytes(24, 34, 48, 1.0f);
    colors[ImGuiCol_TabHovered] = colorFromBytes(46, 91, 147, 1.0f);
    colors[ImGuiCol_TabActive] = colorFromBytes(35, 58, 89, 1.0f);
    colors[ImGuiCol_TabUnfocused] = colorFromBytes(19, 25, 34, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = colorFromBytes(25, 38, 56, 1.0f);
    colors[ImGuiCol_TextSelectedBg] = colorFromBytes(37, 99, 235, 0.38f);
    colors[ImGuiCol_NavHighlight] = colorFromBytes(45, 212, 191, 0.74f);
}

void setupImGuiLookAndFeel() {
    setupImGuiFont();
    setupImGuiStyle();
}

template <std::size_t N>
bool inputTextFullWidth(const char* label, const char* id, std::array<char, N>& value) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::InputText(id, value.data(), value.size());
}

template <std::size_t N>
bool inputTextMultilineFullWidth(const char* label, const char* id, std::array<char, N>& value, float height) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::InputTextMultiline(id, value.data(), value.size(), ImVec2(-1.0f, height));
}

bool sliderIntFullWidth(const char* label, const char* id, int* value, int min, int max) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::SliderInt(id, value, min, max);
}

bool sliderFloatFullWidth(const char* label, const char* id, float* value, float min, float max) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::SliderFloat(id, value, min, max);
}

bool inputIntFullWidth(const char* label, const char* id, int* value) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::InputInt(id, value);
}
} // namespace

void tcApp::setup() {
    setWindowTitle("tcxStableDiffusion - 多模型工作台");
    imguiSetup();
    setupImGuiLookAndFeel();

    selectModel(0, true);
    setupSmokeMode();

    sd_.onProgress([this](const tcx::sd::Progress& progress) {
        std::ostringstream out;
        if (progress.state == tcx::sd::JobState::LoadingModel) {
            out << "正在加载模型";
        } else if (progress.state == tcx::sd::JobState::Complete && progress.jobId == 0) {
            out << "模型已加载";
        } else if (progress.state == tcx::sd::JobState::Failed) {
            out << "任务失败";
        } else if (progress.totalSteps > 0) {
            out << "生成中 " << progress.step << "/" << progress.totalSteps;
            if (progress.seconds > 0.0f) {
                out << "  " << progress.seconds << "s";
            }
        } else if (!progress.message.empty()) {
            out << progress.message;
        } else {
            out << "正在运行";
        }
        status_ = out.str();
        if (smokeMode_) {
            writeSmokeLog("progress step=" + std::to_string(progress.step) +
                "/" + std::to_string(progress.totalSteps) +
                " state=" + tcx::sd::toString(progress.state) +
                " message=" + progress.message);
        }
    });

    sd_.onResult([this](const tcx::StableDiffusionImage& result) {
        if (!result.ok) {
            lastError_ = result.error;
            status_ = "生成失败";
        }
    });

    if (smokeMode_) {
        initializeModel();
        submitWhenReady_ = true;
    }
}

void tcApp::update() {
    sd_.update();

    tcx::StableDiffusionImage result;
    while (sd_.pollResult(result)) {
        adoptResult(std::move(result));
    }

    if (submitWhenReady_ && !sd_.isSettingUp() && !sd_.isRunning()) {
        submitWhenReady_ = false;
        if (sd_.isReady()) {
            submitPrompt();
        } else {
            lastError_ = sd_.lastError();
            writeSmokeLog("model failed: " + lastError_);
            smokeExitRequested_ = smokeMode_;
        }
    }

    if (smokeMode_ && smokeExitRequested_) {
        exitApp();
    }
}

void tcApp::draw() {
    clear(0.07f, 0.075f, 0.085f);

    if (preview_.isAllocated()) {
        const float margin = kPanelWidth + 30.0f;
        const float areaW = std::max(100.0f, static_cast<float>(getWindowWidth()) - margin - 28.0f);
        const float areaH = std::max(100.0f, static_cast<float>(getWindowHeight()) - 28.0f);
        const float scale = std::min(areaW / preview_.getWidth(), areaH / preview_.getHeight());
        const float drawW = preview_.getWidth() * scale;
        const float drawH = preview_.getHeight() * scale;
        preview_.draw(margin + (areaW - drawW) * 0.5f, 14.0f + (areaH - drawH) * 0.5f, drawW, drawH);
    }

    drawGui();
}

void tcApp::cleanup() {
    sd_.shutdown();
    preview_.clear();
    imguiShutdown();
}

void tcApp::setupSmokeMode() {
    smokeMode_ = envEnabled("TCXSD_SMOKE");
    if (!smokeMode_) {
        return;
    }

    if (const char* model = std::getenv("TCXSD_SMOKE_MODEL")) {
        selectModel(profileIndexForId(model), true);
    }

    auto copyEnvText = [](const char* name, auto& target) {
        const char* value = std::getenv(name);
        if (!value) {
            return;
        }
        std::strncpy(target.data(), value, target.size() - 1);
        target[target.size() - 1] = '\0';
    };

    copyEnvText("TCXSD_SMOKE_PROMPT", prompt_);
    copyEnvText("TCXSD_SMOKE_NEGATIVE", negativePrompt_);
    width_ = envInt("TCXSD_SMOKE_WIDTH", width_);
    height_ = envInt("TCXSD_SMOKE_HEIGHT", height_);
    steps_ = envInt("TCXSD_SMOKE_STEPS", steps_);
    seed_ = envInt("TCXSD_SMOKE_SEED", seed_);
    if (envExists("TCXSD_SMOKE_LOW_VRAM")) {
        lowVramMode_ = envEnabled("TCXSD_SMOKE_LOW_VRAM");
    }
    if (envExists("TCXSD_SMOKE_COMPOSE")) {
        usePromptComposer_ = envEnabled("TCXSD_SMOKE_COMPOSE") && profileAt(selectedModel_).supportsIdeogramComposer;
    }
    autoSave_ = true;
    writeSmokeLog("smoke enabled");
    writeSmokeLog("model=" + std::string(profileAt(selectedModel_).id));
    writeSmokeLog("model_dir=" + modelDir_.string());
    writeSmokeLog("size=" + std::to_string(width_) + "x" + std::to_string(height_) +
        " steps=" + std::to_string(steps_) +
        " seed=" + std::to_string(seed_) +
        " low_vram=" + std::string(lowVramMode_ ? "true" : "false"));
}

void tcApp::writeSmokeLog(const std::string& message) {
    if (!smokeMode_) {
        return;
    }

    const auto outputDir = exampleRoot() / "outputs";
    std::filesystem::create_directories(outputDir);
    std::ofstream out(outputDir / "smoke_status.txt", std::ios::app);
    out << message << "\n";
}

void tcApp::selectModel(int index, bool resetPrompt) {
    const int safeIndex = std::clamp(index, 0, static_cast<int>(kProfiles.size()) - 1);
    const bool changed = selectedModel_ != safeIndex;
    if (changed && (sd_.isReady() || sd_.isRunning() || sd_.isSettingUp())) {
        sd_.shutdown();
    }

    selectedModel_ = safeIndex;
    refreshModelDir();
    setupAttempted_ = false;
    currentJob_ = 0;
    lastError_.clear();
    status_ = std::string("已选择模型: ") + profileAt(selectedModel_).label;

    if (resetPrompt || changed) {
        applyModelDefaults();
    }
}

void tcApp::refreshModelDir() {
    modelDir_ = exampleRoot() / "data" / "models" / profileAt(selectedModel_).id;
}

void tcApp::initializeModel() {
    setupAttempted_ = true;
    refreshModelDir();
    status_ = "正在初始化模型";
    lastError_.clear();

#if defined(__APPLE__)
    auto settings = tcx::sd::RuntimeSettings::macMetal();
#else
    auto settings = lowVramMode_
        ? tcx::sd::RuntimeSettings::lowVramCuda()
        : tcx::sd::RuntimeSettings::windowsCuda();
#endif
    settings.outputDirectory = exampleRoot() / "outputs" / profileAt(selectedModel_).id / "native";

    bool started = false;
    switch (profileAt(selectedModel_).kind) {
        case ProfileKind::Ideogram4:
            started = sd_.setupIdeogram4Async(modelDir_, settings);
            break;
        case ProfileKind::Flux2Klein:
            started = sd_.setupFlux2KleinAsync(modelDir_, settings);
            break;
        case ProfileKind::ZImageTurbo:
            started = sd_.setupZImageTurboAsync(modelDir_, settings);
            break;
    }

    if (started) {
        writeSmokeLog("model loading");
        status_ = "正在异步加载模型";
        return;
    }

    lastError_ = sd_.lastError();
    writeSmokeLog("model failed: " + lastError_);
    status_ = "初始化失败";
}

tcx::sd::IdeogramPrompt tcApp::buildPromptTemplate() const {
    auto prompt = tcx::sd::IdeogramPrompt::poster(templateSubject_.data())
        .text(templateText_.data())
        .styleDescription(templateStyle_.data())
        .compositionDescription("upright poster layout with a clear title zone, central product/addon metaphor, visible interface fragments, and balanced technical details")
        .backgroundDescription("clean dark-to-light studio background with subtle grid hints and restrained interface texture")
        .lightingDescription("soft studio key light, crisp rim light on important edges, readable contrast for all text")
        .mediumDescription("mixed media product photography, refined editorial graphic design, and precise software UI illustration")
        .moodDescription("professional, calm, high-performance, and approachable");

    const auto colors = splitCsv(templatePalette_.data());
    if (!colors.empty()) {
        prompt.palette(colors);
    }

    prompt.element("obj", "A tasteful local AI workstation or abstract addon module that suggests Windows CUDA acceleration without showing noisy hardware clutter.");
    prompt.element("obj", "Small refined UI panels, node graph fragments, and generation preview tiles arranged as secondary details.");
    return prompt;
}

void tcApp::applyPromptTemplate() {
    if (!profileAt(selectedModel_).supportsIdeogramComposer) {
        return;
    }
    const auto prompt = buildPromptTemplate();
    copyText(prompt_, prompt.build());
    copyText(negativePrompt_, prompt.negative());
}

void tcApp::applyModelDefaults() {
    const auto& profile = profileAt(selectedModel_);
    width_ = profile.width;
    height_ = profile.height;
    steps_ = profile.steps;
    seed_ = profile.seed;
    cfgScale_ = profile.cfg;
    usePromptComposer_ = profile.supportsIdeogramComposer;
    copyText(templateSubject_, profile.subject);
    copyText(templateText_, profile.visibleText);
    copyText(templateStyle_, profile.style);
    copyText(templatePalette_, profile.palette);
    if (profile.supportsIdeogramComposer) {
        applyPromptTemplate();
    } else {
        copyText(prompt_, profile.prompt);
        copyText(negativePrompt_, profile.negative);
    }
}

void tcApp::submitPrompt() {
    if (!sd_.isReady()) {
        status_ = "请先初始化模型";
        return;
    }
    if (sd_.isRunning()) {
        status_ = "已有任务正在运行";
        return;
    }

    const auto& profile = profileAt(selectedModel_);
    tcx::StableDiffusionRequest request = (usePromptComposer_ && profile.supportsIdeogramComposer)
        ? tcx::StableDiffusionRequest::fromIdeogram4(buildPromptTemplate())
        : tcx::StableDiffusionRequest::fromPrompt(prompt_.data());

    if (usePromptComposer_ && profile.supportsIdeogramComposer) {
        copyText(prompt_, request.prompt);
        copyText(negativePrompt_, request.negativePrompt);
    } else {
        request.negative(negativePrompt_.data());
    }

    request
        .size(width_, height_)
        .stepsCount(steps_)
        .cfg(cfgScale_);
    request.metadata["example"] = "ideogram4-basic";
    request.metadata["model"] = profile.id;
    request.metadata["prompt_profile"] = profile.promptProfile;
    request.metadata["prompt_kind"] = profile.promptKind;

    if (seed_ >= 0) {
        request.seedValue(seed_);
    }

    currentJob_ = sd_.submit(std::move(request));
    writeSmokeLog("submit returned job=" + std::to_string(currentJob_));
    if (currentJob_ == 0) {
        lastError_ = sd_.lastError();
        writeSmokeLog("submit failed: " + lastError_);
        smokeExitRequested_ = smokeMode_;
        status_ = "提交失败";
    } else {
        status_ = "任务已提交";
    }
}

void tcApp::adoptResult(tcx::StableDiffusionImage&& result) {
    const auto& profile = profileAt(selectedModel_);
    if (!result.ok || !result.hasImage()) {
        lastError_ = result.error;
        if (autoSave_) {
            const auto outputDir = exampleRoot() / "outputs" / profile.id;
            std::filesystem::create_directories(outputDir);
            lastMetadata_ = outputDir / (std::string(profile.id) + "_job_" + std::to_string(result.jobId) + "_failed.json");
            result.saveMetadata(lastMetadata_);
            writeSmokeLog("saved metadata: " + lastMetadata_.string());
        }
        if (smokeMode_) {
            writeSmokeLog("generation failed: " + lastError_);
            smokeExitRequested_ = true;
        }
        status_ = "生成失败";
        return;
    }

    if (autoSave_) {
        const auto outputDir = exampleRoot() / "outputs" / profile.id;
        std::filesystem::create_directories(outputDir);
        lastOutput_ = outputDir / (std::string(profile.id) + "_job_" + std::to_string(result.jobId) + ".png");
        lastMetadata_ = lastOutput_;
        lastMetadata_.replace_extension(".json");
        result.metadata["saved_image_path"] = lastOutput_.string();
        result.saveWithMetadata(lastOutput_, lastMetadata_);
        writeSmokeLog("saved: " + lastOutput_.string());
        writeSmokeLog("saved metadata: " + lastMetadata_.string());
    }

    preview_.allocate(result.pixels.getWidth(), result.pixels.getHeight(), result.pixels.getChannels());
    std::memcpy(preview_.getPixelsData(), result.pixels.getData(), result.pixels.getTotalBytes());
    preview_.setDirty();
    preview_.update();
    if (smokeMode_) {
        smokeExitRequested_ = true;
    }

    status_ = "生成完成";
}

void tcApp::drawGui() {
    imguiBegin();

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, 850), ImGuiCond_FirstUseEver);
    ImGui::Begin("tcxStableDiffusion 多模型工作台", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(colorFromBytes(45, 212, 191), "状态");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", status_.c_str());
    if (!lastError_.empty()) {
        ImGui::TextColored(colorFromBytes(251, 113, 133), "错误");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", lastError_.c_str());
    }
    ImGui::SeparatorText("模型");

    const bool busy = sd_.isRunning() || sd_.isSettingUp();
    int comboIndex = selectedModel_;
    const char* labels[] = {
        kProfiles[0].label,
        kProfiles[1].label,
        kProfiles[2].label,
    };
    if (busy) {
        ImGui::BeginDisabled();
    }
    ImGui::TextUnformatted("模型");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##model", &comboIndex, labels, static_cast<int>(kProfiles.size()))) {
        selectModel(comboIndex, true);
    }
    if (busy) {
        ImGui::EndDisabled();
    }

    ImGui::TextWrapped("模型目录");
    ImGui::PushStyleColor(ImGuiCol_Text, colorFromBytes(168, 180, 198));
    ImGui::TextWrapped("%s", modelDir_.string().c_str());
    ImGui::PopStyleColor();

    if (busy) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("初始化当前模型", ImVec2(214, 32))) {
        initializeModel();
    }
    if (busy) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("取消任务", ImVec2(-1, 32))) {
        sd_.cancel();
    }

    ImGui::Checkbox("低显存模式", &lowVramMode_);
    ImGui::SameLine();
    ImGui::Checkbox("自动保存图片", &autoSave_);

    const bool composerAvailable = profileAt(selectedModel_).supportsIdeogramComposer;
    if (!composerAvailable) {
        ImGui::BeginDisabled();
    }
    ImGui::Checkbox("使用 Ideogram4 结构化模板", &usePromptComposer_);
    if (!composerAvailable) {
        ImGui::EndDisabled();
        usePromptComposer_ = false;
    }

    if (ImGui::Button("套用当前模型默认提示词", ImVec2(-1, 32))) {
        applyModelDefaults();
    }

    ImGui::SeparatorText("提示词");
    if (usePromptComposer_ && composerAvailable) {
        inputTextFullWidth("主题", "##template_subject", templateSubject_);
        inputTextFullWidth("画面文字", "##template_text", templateText_);
        inputTextMultilineFullWidth("风格", "##template_style", templateStyle_, 68.0f);
        inputTextFullWidth("配色（逗号分隔）", "##template_palette", templatePalette_);
        if (ImGui::Button("应用模板", ImVec2(-1, 32))) {
            applyPromptTemplate();
        }
        ImGui::Spacing();
    }
    inputTextMultilineFullWidth("提示词", "##prompt", prompt_, 150.0f);
    inputTextMultilineFullWidth("反向提示词", "##negative_prompt", negativePrompt_, 82.0f);

    ImGui::SeparatorText("生成参数");
    sliderIntFullWidth("宽度", "##width", &width_, 512, 1536);
    sliderIntFullWidth("高度", "##height", &height_, 512, 1536);
    sliderIntFullWidth("步数", "##steps", &steps_, 1, 40);
    sliderFloatFullWidth("CFG", "##cfg", &cfgScale_, 0.0f, 8.0f);
    inputIntFullWidth("种子（-1 随机）", "##seed", &seed_);

    const bool canGenerate = sd_.isReady() && !sd_.isRunning();
    if (!canGenerate) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("生成图像", ImVec2(-1, 38))) {
        submitPrompt();
    }
    if (!canGenerate) {
        ImGui::EndDisabled();
    }

    const auto progress = sd_.progress();
    if (progress.totalSteps > 0) {
        const float ratio = std::clamp(
            static_cast<float>(progress.step) / static_cast<float>(progress.totalSteps),
            0.0f,
            1.0f);
        ImGui::ProgressBar(ratio, ImVec2(-1, 0));
    }

    if (!lastOutput_.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("输出文件");
        ImGui::PushStyleColor(ImGuiCol_Text, colorFromBytes(168, 180, 198));
        ImGui::TextWrapped("%s", lastOutput_.string().c_str());
        ImGui::PopStyleColor();
    }
    if (!lastMetadata_.empty()) {
        ImGui::TextWrapped("记录文件");
        ImGui::PushStyleColor(ImGuiCol_Text, colorFromBytes(168, 180, 198));
        ImGui::TextWrapped("%s", lastMetadata_.string().c_str());
        ImGui::PopStyleColor();
    }

    if (!setupAttempted_ && !tcx::StableDiffusion::nativeAvailable()) {
        ImGui::Separator();
        ImGui::TextWrapped("尚未安装 native runtime。请先运行 tools/setup_sd.py build-native。");
    }

    ImGui::End();
    imguiEnd();
}
