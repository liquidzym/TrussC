#include "tcApp.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::filesystem::path exampleRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path();
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
} // namespace

void tcApp::setup() {
    setWindowTitle("tcxStableDiffusion - Ideogram4");
    imguiSetup();

    modelDir_ = exampleRoot() / "models";
    copyText(templateSubject_, "A clean futuristic product poster for tcxStableDiffusion, a modular local AI image addon for creative coders");
    copyText(templateText_, "tcxStableDiffusion");
    copyText(templateStyle_, "premium technical product poster, refined typography, elegant studio lighting, precise interface details");
    copyText(templatePalette_, "#F7F4EC, #111111, #2F80ED, #27AE60, #FFFFFF");
    applyPromptTemplate();
    setupSmokeMode();

    sd_.onProgress([this](const tcx::sd::Progress& progress) {
        std::ostringstream out;
        out << "生成中 " << progress.step << "/" << progress.totalSteps;
        if (progress.seconds > 0.0f) {
            out << "  " << progress.seconds << "s";
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
        if (sd_.isReady()) {
            submitPrompt();
        } else {
            smokeExitRequested_ = true;
        }
    }
}

void tcApp::update() {
    sd_.update();

    tcx::StableDiffusionImage result;
    while (sd_.pollResult(result)) {
        adoptResult(std::move(result));
    }

    if (smokeMode_ && smokeExitRequested_) {
        exitApp();
    }
}

void tcApp::draw() {
    clear(0.07f, 0.075f, 0.085f);

    if (preview_.isAllocated()) {
        const float margin = 390.0f;
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
        usePromptComposer_ = envEnabled("TCXSD_SMOKE_COMPOSE");
    } else {
        usePromptComposer_ = false;
    }
    autoSave_ = true;
    writeSmokeLog("smoke enabled");
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

void tcApp::initializeModel() {
    setupAttempted_ = true;
    status_ = "正在初始化模型";
    lastError_.clear();

#if defined(__APPLE__)
    auto settings = tcx::sd::RuntimeSettings::macMetal();
#else
    auto settings = lowVramMode_
        ? tcx::sd::RuntimeSettings::lowVramCuda()
        : tcx::sd::RuntimeSettings::windowsCuda();
#endif

    if (sd_.setupIdeogram4(modelDir_, settings)) {
        writeSmokeLog("model ready");
        status_ = "模型已加载";
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
    const auto prompt = buildPromptTemplate();
    copyText(prompt_, prompt.build());
    copyText(negativePrompt_, prompt.negative());
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

    tcx::StableDiffusionRequest request = usePromptComposer_
        ? tcx::StableDiffusionRequest::fromIdeogram4(buildPromptTemplate())
        : tcx::StableDiffusionRequest::fromPrompt(prompt_.data());

    if (usePromptComposer_) {
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
    request.metadata["model"] = "ideogram4-q4_0";

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
    if (!result.ok || !result.hasImage()) {
        lastError_ = result.error;
        if (autoSave_) {
            const auto outputDir = exampleRoot() / "outputs";
            std::filesystem::create_directories(outputDir);
            lastMetadata_ = outputDir / ("ideogram4_job_" + std::to_string(result.jobId) + "_failed.json");
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
        const auto outputDir = exampleRoot() / "outputs";
        std::filesystem::create_directories(outputDir);
        lastOutput_ = outputDir / ("ideogram4_job_" + std::to_string(result.jobId) + ".png");
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
    ImGui::SetNextWindowSize(ImVec2(360, 820), ImGuiCond_FirstUseEver);
    ImGui::Begin("Ideogram4 本地生成");

    ImGui::TextWrapped("状态: %s", status_.c_str());
    if (!lastError_.empty()) {
        ImGui::TextWrapped("错误: %s", lastError_.c_str());
    }
    ImGui::Separator();

    ImGui::TextWrapped("模型目录:");
    ImGui::TextWrapped("%s", modelDir_.string().c_str());
    if (ImGui::Button("初始化模型")) {
        initializeModel();
    }
    ImGui::SameLine();
    if (ImGui::Button("取消任务")) {
        sd_.cancel();
    }

    ImGui::Checkbox("低显存模式", &lowVramMode_);
    ImGui::Checkbox("自动保存图片", &autoSave_);
    ImGui::Checkbox("使用 Ideogram4 模板", &usePromptComposer_);

    ImGui::Separator();
    if (usePromptComposer_) {
        ImGui::InputText("主题", templateSubject_.data(), templateSubject_.size());
        ImGui::InputText("画面文字", templateText_.data(), templateText_.size());
        ImGui::InputTextMultiline("风格", templateStyle_.data(), templateStyle_.size(), ImVec2(-1, 64));
        ImGui::InputText("配色(逗号分隔)", templatePalette_.data(), templatePalette_.size());
        if (ImGui::Button("应用模板", ImVec2(-1, 28))) {
            applyPromptTemplate();
        }
        ImGui::Separator();
    }
    ImGui::InputTextMultiline("提示词", prompt_.data(), prompt_.size(), ImVec2(-1, 150));
    ImGui::InputTextMultiline("反向提示词", negativePrompt_.data(), negativePrompt_.size(), ImVec2(-1, 80));

    ImGui::SliderInt("宽度", &width_, 512, 1536);
    ImGui::SliderInt("高度", &height_, 512, 1536);
    ImGui::SliderInt("步数", &steps_, 1, 40);
    ImGui::SliderFloat("CFG", &cfgScale_, 0.0f, 8.0f);
    ImGui::InputInt("种子(-1随机)", &seed_);

    const bool canGenerate = sd_.isReady() && !sd_.isRunning();
    if (!canGenerate) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("生成图像", ImVec2(-1, 34))) {
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
        ImGui::TextWrapped("输出文件:");
        ImGui::TextWrapped("%s", lastOutput_.string().c_str());
    }
    if (!lastMetadata_.empty()) {
        ImGui::TextWrapped("记录文件:");
        ImGui::TextWrapped("%s", lastMetadata_.string().c_str());
    }

    if (!setupAttempted_ && !tcx::StableDiffusion::nativeAvailable()) {
        ImGui::Separator();
        ImGui::TextWrapped("尚未安装 native runtime。请先运行 tools/setup_sd.py build-native。");
    }

    ImGui::End();
    imguiEnd();
}
