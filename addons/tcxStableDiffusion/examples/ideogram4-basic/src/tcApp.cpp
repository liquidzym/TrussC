#include "tcApp.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
    SD15ControlNetCanny,
};

enum class WorkflowMode {
    TextToImage,
    ImageToImage,
    Inpaint,
    ControlNet,
    LoraStack,
    Refine,
    Upscale,
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

constexpr const char* kIdeogramVerifiedPrompt = R"tcxsd({"high_level_description":"A square 1024 x 1024 luxury fashion magazine cover featuring exactly one short chubby fluffy cat as the main model. The cat sits on a soft ivory studio floor, facing the viewer with a stylish calm expression, wearing tiny black sunglasses, a red silk scarf, and a small gold collar charm. In front of the cat on the floor is a wide horizontal luxury nameplate that clearly reads ideogram4.cpp. The whole design feels premium, fashionable, clean, and editorial.","style_description":{"aesthetics":"luxury fashion magazine cover, high-end pet couture campaign, minimalist editorial design, elegant studio photography, soft paper texture, refined typography, fashionable and polished","lighting":"Soft diffused studio lighting, gentle spotlight on the cat, subtle floor shadow, warm ivory highlights, clean separation between subject and background","photo":"high-resolution fashion editorial photography look, front-facing cat portrait, crisp fur details, glossy sunglasses, clear readable nameplate text, shallow depth of field","medium":"mixed media fashion photography and premium editorial graphic design","color_palette":["#F4EFE7","#111111","#D8B56D","#B73A3A","#FFFFFF","#8A7A6A"]},"compositional_deconstruction":{"canvas":"Square 1024 x 1024 canvas with a normal upright orientation. Do not rotate the poster or any text. Use a clean fashion magazine cover layout.","background":"Warm ivory studio backdrop with subtle paper grain, a soft spotlight gradient, faint floor shadow, and a few minimal gold editorial lines. The background is spacious, premium, and uncluttered.","layout":"Top center has a small elegant headline. Center area features one cat as the main fashion model. Lower foreground has a wide horizontal luxury nameplate placed on the floor in front of the cat. Bottom center has a small footer. All text is horizontal, upright, and readable left to right.","elements":[{"type":"text","desc":"Top center headline reading LOOK WHAT I FOUND in a refined high-fashion serif font. The headline is horizontal, centered, elegant, and secondary to the nameplate text."},{"type":"obj","desc":"Exactly one short chubby fluffy cat sitting in the center like a luxury fashion model. The cat has a large round head, compact body, short legs, soft detailed fur, expressive eyes, and a calm confident pose. The cat is cute and rounded, not tall, not stretched, not duplicated."},{"type":"obj","desc":"Tiny glossy black sunglasses worn naturally by the cat, slightly oversized but still showing the cat face clearly. The sunglasses add a chic fashion-editorial attitude."},{"type":"obj","desc":"A red silk scarf tied neatly around the cat neck, with soft folds and a couture feeling. The scarf must not cover the cat face or the nameplate."},{"type":"obj","desc":"A small gold collar charm or fashion accessory under the scarf, subtle and premium, adding a luxury campaign detail."},{"type":"obj","desc":"In the lower foreground, place a wide horizontal luxury nameplate on the floor in front of the cat. The nameplate is low, flat, landscape-oriented, much wider than tall, like a fashion show seat card or premium display plaque. It is centered, front-facing, level, and fully visible. It must not become vertical, tall, standing, rotated, or side-facing."},{"type":"text","desc":"Print the exact text ideogram4.cpp only on the wide horizontal nameplate. Use clean bold black lettering, perfectly spelled, lowercase, with the number 4 and .cpp extension. The text must fit completely inside the nameplate, stay horizontal, and be readable from left to right."},{"type":"obj","desc":"Add sparse premium editorial accents around the edges: thin gold lines, small code brackets, tiny cursor marks, subtle dots, and minimal geometric details. No extra cats, no stickers, no animal faces, no busy decorations."},{"type":"text","desc":"Bottom center footer reading tiny paws, big compile energy in a small refined monospace or editorial font. The footer is horizontal, centered, understated, and much smaller than the nameplate text."}]}})tcxsd";

const std::array<ModelProfile, 4> kProfiles = {{
    {
        ProfileKind::Ideogram4,
        "ideogram4-q4_0",
        "Ideogram4 Q4_0",
        "ideogram4",
        "poster",
        1024,
        1024,
        20,
        42,
        7.0f,
        true,
        "A square 1024 x 1024 luxury fashion magazine cover featuring exactly one short chubby fluffy cat as the main model. The cat sits on a soft ivory studio floor, facing the viewer with a stylish calm expression, wearing tiny black sunglasses, a red silk scarf, and a small gold collar charm. In front of the cat on the floor is a wide horizontal luxury nameplate that clearly reads ideogram4.cpp. The whole design feels premium, fashionable, clean, and editorial",
        "ideogram4.cpp",
        "luxury fashion magazine cover, high-end pet couture campaign, minimalist editorial design, elegant studio photography, soft paper texture, refined typography, fashionable and polished",
        "#F4EFE7, #111111, #D8B56D, #B73A3A, #FFFFFF, #8A7A6A",
        kIdeogramVerifiedPrompt,
        "",
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
    {
        ProfileKind::SD15ControlNetCanny,
        "sd15-controlnet-canny",
        "SD 1.5 ControlNet Canny",
        "sd15-controlnet",
        "control-net",
        512,
        512,
        20,
        5120,
        7.5f,
        false,
        "A clean architectural product scene guided by a Canny control image",
        "tcxStableDiffusion",
        "controlled composition, clear edges, polished local AI generation demo",
        "#17130E, #2B2419, #C59A42, #E1C16A, #F4E6C1",
        "A clean architectural product scene guided by a Canny control image, warm studio lighting, precise edge-following composition, dark charcoal and earthy-gold visual direction",
        "low quality, blurry, distorted edges, clutter, watermark, signature",
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
    if (text == "controlnet" || text == "control-net" || text == "sd15-controlnet" || text == "sd15-controlnet-canny") {
        return 3;
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

std::string trimText(std::string text) {
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
        item = trimText(std::move(item));
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

std::filesystem::path resolveExamplePath(const char* text) {
    std::string value = trimText(text ? text : "");
    if (value.empty()) {
        return {};
    }
    std::filesystem::path path = value;
    if (path.is_absolute()) {
        return path;
    }
    return exampleRoot() / path;
}

std::string relativeToExamplePath(const std::filesystem::path& path) {
    std::error_code ec;
    const auto root = exampleRoot();
    const auto relative = std::filesystem::relative(path, root, ec);
    if (!ec && !relative.empty()) {
        const std::string text = relative.generic_string();
        if (text != "." && text.rfind("../", 0) != 0 && text.rfind("..\\", 0) != 0) {
            return text;
        }
    }
    return path.string();
}

std::string lowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

bool isLoraFile(const std::filesystem::path& path) {
    const std::string ext = lowerExtension(path);
    return ext == ".safetensors" || ext == ".sft" || ext == ".pt" || ext == ".ckpt" || ext == ".bin";
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
    colors[ImGuiCol_Text] = colorFromBytes(238, 231, 214);
    colors[ImGuiCol_TextDisabled] = colorFromBytes(143, 132, 112);
    colors[ImGuiCol_WindowBg] = colorFromBytes(13, 12, 10, 0.98f);
    colors[ImGuiCol_ChildBg] = colorFromBytes(24, 21, 16, 0.90f);
    colors[ImGuiCol_PopupBg] = colorFromBytes(27, 23, 17, 0.98f);
    colors[ImGuiCol_Border] = colorFromBytes(83, 70, 44, 0.90f);
    colors[ImGuiCol_BorderShadow] = colorFromBytes(0, 0, 0, 0.0f);
    colors[ImGuiCol_FrameBg] = colorFromBytes(38, 32, 23, 0.96f);
    colors[ImGuiCol_FrameBgHovered] = colorFromBytes(60, 48, 30, 1.0f);
    colors[ImGuiCol_FrameBgActive] = colorFromBytes(86, 64, 34, 1.0f);
    colors[ImGuiCol_TitleBg] = colorFromBytes(11, 10, 8, 1.0f);
    colors[ImGuiCol_TitleBgActive] = colorFromBytes(34, 27, 17, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = colorFromBytes(11, 10, 8, 0.86f);
    colors[ImGuiCol_MenuBarBg] = colorFromBytes(24, 21, 16, 1.0f);
    colors[ImGuiCol_ScrollbarBg] = colorFromBytes(12, 10, 8, 0.82f);
    colors[ImGuiCol_ScrollbarGrab] = colorFromBytes(75, 61, 37, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = colorFromBytes(109, 84, 44, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = colorFromBytes(141, 105, 48, 1.0f);
    colors[ImGuiCol_CheckMark] = colorFromBytes(218, 170, 74, 1.0f);
    colors[ImGuiCol_SliderGrab] = colorFromBytes(197, 154, 66, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = colorFromBytes(225, 193, 106, 1.0f);
    colors[ImGuiCol_Button] = colorFromBytes(95, 69, 30, 0.94f);
    colors[ImGuiCol_ButtonHovered] = colorFromBytes(129, 93, 38, 1.0f);
    colors[ImGuiCol_ButtonActive] = colorFromBytes(172, 126, 47, 1.0f);
    colors[ImGuiCol_Header] = colorFromBytes(63, 48, 27, 0.95f);
    colors[ImGuiCol_HeaderHovered] = colorFromBytes(103, 75, 35, 1.0f);
    colors[ImGuiCol_HeaderActive] = colorFromBytes(166, 122, 48, 1.0f);
    colors[ImGuiCol_Separator] = colorFromBytes(91, 75, 48, 0.78f);
    colors[ImGuiCol_SeparatorHovered] = colorFromBytes(197, 154, 66, 0.88f);
    colors[ImGuiCol_SeparatorActive] = colorFromBytes(225, 193, 106, 1.0f);
    colors[ImGuiCol_ResizeGrip] = colorFromBytes(197, 154, 66, 0.24f);
    colors[ImGuiCol_ResizeGripHovered] = colorFromBytes(197, 154, 66, 0.56f);
    colors[ImGuiCol_ResizeGripActive] = colorFromBytes(225, 193, 106, 0.78f);
    colors[ImGuiCol_Tab] = colorFromBytes(34, 28, 20, 1.0f);
    colors[ImGuiCol_TabHovered] = colorFromBytes(109, 80, 37, 1.0f);
    colors[ImGuiCol_TabActive] = colorFromBytes(68, 51, 28, 1.0f);
    colors[ImGuiCol_TabUnfocused] = colorFromBytes(22, 19, 15, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = colorFromBytes(43, 34, 22, 1.0f);
    colors[ImGuiCol_TextSelectedBg] = colorFromBytes(154, 112, 44, 0.42f);
    colors[ImGuiCol_NavHighlight] = colorFromBytes(225, 193, 106, 0.74f);
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

bool looksLikeSafetyPlaceholder(const Pixels& pixels) {
    if (!pixels.isAllocated() || pixels.isFloat() || pixels.getChannels() < 3 || pixels.getWidth() < 128 || pixels.getHeight() < 128) {
        return false;
    }

    const int width = pixels.getWidth();
    const int height = pixels.getHeight();
    const int channels = pixels.getChannels();
    const unsigned char* data = pixels.getData();
    const int stride = std::max(1, (width * height) / 32768);

    std::uint64_t sampled = 0;
    std::uint64_t neutral = 0;
    std::uint64_t midGray = 0;
    std::uint64_t brightNeutral = 0;
    std::uint64_t colored = 0;
    double sum = 0.0;
    double sumSq = 0.0;

    for (int index = 0; index < width * height; index += stride) {
        const unsigned char* px = data + static_cast<size_t>(index) * static_cast<size_t>(channels);
        const int r = px[0];
        const int g = px[1];
        const int b = px[2];
        const int maxChannel = std::max({r, g, b});
        const int minChannel = std::min({r, g, b});
        const int spread = maxChannel - minChannel;
        const double luminance = (static_cast<double>(r) + static_cast<double>(g) + static_cast<double>(b)) / 3.0;

        ++sampled;
        sum += luminance;
        sumSq += luminance * luminance;

        if (spread <= 6) {
            ++neutral;
            if (luminance >= 92.0 && luminance <= 150.0) {
                ++midGray;
            }
            if (luminance >= 175.0) {
                ++brightNeutral;
            }
        } else if (spread >= 24) {
            ++colored;
        }
    }

    if (sampled == 0) {
        return false;
    }

    const double count = static_cast<double>(sampled);
    const double neutralRatio = static_cast<double>(neutral) / count;
    const double midGrayRatio = static_cast<double>(midGray) / count;
    const double brightRatio = static_cast<double>(brightNeutral) / count;
    const double coloredRatio = static_cast<double>(colored) / count;
    const double mean = sum / count;
    const double variance = std::max(0.0, (sumSq / count) - (mean * mean));
    const double stddev = std::sqrt(variance);

    const bool mostlyNeutral = neutralRatio > 0.90 && coloredRatio < 0.035;
    const bool warningLikeContrast = stddev >= 7.0 && stddev <= 85.0;
    const bool grayWarning = midGrayRatio > 0.55 && mean >= 85.0 && mean <= 165.0;
    const bool brightWarning = brightRatio > 0.55 && mean >= 170.0 && mean <= 245.0;
    return mostlyNeutral && warningLikeContrast && (grayWarning || brightWarning);
}

} // namespace

void tcApp::setup() {
    setWindowTitle("tcxStableDiffusion - 多模型工作台");
    imguiSetup();
    setupImGuiLookAndFeel();

    copyText(projectName_, "workflow-demo");
    copyText(initImagePath_, "inputs/source.png");
    copyText(maskImagePath_, "inputs/mask.png");
    copyText(controlImagePath_, "inputs/control_canny.png");
    copyText(sourceImagePath_, "inputs/source.png");
    copyText(loraPath_, "style.safetensors");

    selectModel(0, true);
    refreshLoraFiles();
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
        if (const char* preprocess = std::getenv("TCXSD_SMOKE_PREPROCESS")) {
            const std::string mode = trimText(preprocess);
            if (mode == "control" || mode == "controlnet" || mode == "canny") {
                generateControlImage();
                writeSmokeLog("preprocess=control status=" + status_);
            } else if (mode == "mask" || mode == "inpaint") {
                generateInpaintMask();
                writeSmokeLog("preprocess=mask status=" + status_);
            } else {
                lastError_ = "Unknown TCXSD_SMOKE_PREPROCESS mode: " + mode;
                writeSmokeLog("preprocess failed: " + lastError_);
            }
            smokeExitRequested_ = true;
            return;
        }
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
    clear(0.045f, 0.040f, 0.032f);

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
    copyEnvText("TCXSD_SMOKE_INIT_IMAGE", initImagePath_);
    copyEnvText("TCXSD_SMOKE_MASK_IMAGE", maskImagePath_);
    copyEnvText("TCXSD_SMOKE_CONTROL_IMAGE", controlImagePath_);
    copyEnvText("TCXSD_SMOKE_SOURCE_IMAGE", sourceImagePath_);
    copyEnvText("TCXSD_SMOKE_LORA", loraPath_);
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
    if (const char* workflow = std::getenv("TCXSD_SMOKE_WORKFLOW")) {
        std::string text = workflow;
        text = trimText(std::move(text));
        if (text == "img2img" || text == "image_to_image") workflowMode_ = static_cast<int>(WorkflowMode::ImageToImage);
        else if (text == "inpaint") workflowMode_ = static_cast<int>(WorkflowMode::Inpaint);
        else if (text == "controlnet" || text == "control_net") workflowMode_ = static_cast<int>(WorkflowMode::ControlNet);
        else if (text == "lora" || text == "lora_stack") workflowMode_ = static_cast<int>(WorkflowMode::LoraStack);
        else if (text == "refine") workflowMode_ = static_cast<int>(WorkflowMode::Refine);
        else if (text == "upscale") workflowMode_ = static_cast<int>(WorkflowMode::Upscale);
        else workflowMode_ = static_cast<int>(WorkflowMode::TextToImage);
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
    modelDir_ = exampleRoot() / "bin" / "data" / "models" / profileAt(selectedModel_).id;
}

void tcApp::initializeModel() {
    setupAttempted_ = true;
    refreshModelDir();
    status_ = "正在初始化模型";
    lastError_.clear();

    if (profileAt(selectedModel_).kind == ProfileKind::SD15ControlNetCanny && !envEnabled("TCXSD_ENABLE_UNSTABLE_CXX_CONTROLNET")) {
        lastError_ = "BACKEND_UNSUPPORTED: SD 1.5 ControlNet Canny runs through the tracked Node CLI and JSON job examples. The C++ workbench blocks this native path because GUI smoke testing found a Windows native backend access violation during ControlNet startup. Set TCXSD_ENABLE_UNSTABLE_CXX_CONTROLNET=1 only for diagnostics.";
        status_ = "初始化失败";
        writeSmokeLog("model failed: " + lastError_);
        if (smokeMode_) {
            std::_Exit(0);
        }
        return;
    }

#if defined(__APPLE__)
    auto settings = tcx::sd::RuntimeSettings::macMetal();
#else
    auto settings = tcx::sd::ModelProfile::byId(profileAt(selectedModel_).id).runtime(
        lowVramMode_ ? tcx::sd::RuntimePreset::LowVram : tcx::sd::RuntimePreset::Default);
#endif
    auto project = tcx::sd::GenerationProject::at(exampleRoot() / "outputs", projectName_.data());
    settings = project.apply(settings);
    settings.outputDirectory = project.outputRoot / profileAt(selectedModel_).id / "native";
    settings.tempDirectory = project.tempRoot;
    settings.cacheDirectory = project.cacheRoot;
    settings.loraModelDirectory = loraModelDir();

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
        case ProfileKind::SD15ControlNetCanny:
            started = sd_.setupSD15ControlNetCannyAsync(modelDir_, settings);
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
        .compositionDescription("Square 1024 x 1024 editorial layout with a clear top headline, one centered main subject, a wide horizontal foreground nameplate, and small footer text. All text is horizontal, upright, and readable left to right.")
        .backgroundDescription("warm ivory studio backdrop with subtle paper grain, a soft spotlight gradient, faint floor shadow, and sparse premium edge accents")
        .lightingDescription("soft diffused studio lighting, gentle spotlight on the main subject, subtle floor shadow, warm ivory highlights, clean separation between subject and background")
        .mediumDescription("mixed media product photography and premium editorial graphic design")
        .moodDescription("premium, calm, polished, and editorial");

    const auto colors = splitCsv(templatePalette_.data());
    if (!colors.empty()) {
        prompt.palette(colors);
    }

    prompt.element("text", "Top center headline reading LOOK WHAT I FOUND in a refined editorial serif font. The headline is horizontal, centered, elegant, and secondary to the foreground nameplate text.");
    prompt.element("obj", "The main subject described above, centered as the single hero object with a premium studio photography look, clean silhouette, and no duplicated subjects.");
    prompt.element("obj", "In the lower foreground, place a wide horizontal nameplate. It is low, flat, landscape-oriented, centered, front-facing, level, and fully visible.");
    prompt.element("text", "Print the exact text from the requested visible-text field only on the wide horizontal nameplate. Use clean bold black lettering, keep the spelling exact, and keep the text horizontal and readable from left to right.");
    prompt.element("obj", "Add sparse premium editorial accents around the edges: thin lines, tiny cursor marks, subtle dots, and minimal geometric details. Keep the background spacious and uncluttered.");
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
    usePromptComposer_ = profile.supportsIdeogramComposer && (!profile.prompt || !*profile.prompt);
    copyText(templateSubject_, profile.subject);
    copyText(templateText_, profile.visibleText);
    copyText(templateStyle_, profile.style);
    copyText(templatePalette_, profile.palette);
    if (profile.prompt && *profile.prompt) {
        copyText(prompt_, profile.prompt);
        copyText(negativePrompt_, profile.negative);
    } else if (profile.supportsIdeogramComposer) {
        applyPromptTemplate();
    } else {
        copyText(prompt_, profile.prompt);
        copyText(negativePrompt_, profile.negative);
    }
}

std::filesystem::path tcApp::loraModelDir() const {
    return exampleRoot() / "bin" / "data" / "models" / "loras";
}

std::filesystem::path tcApp::resolveLoraPath(const char* text) const {
    std::string value = trimText(text ? text : "");
    if (value.empty()) {
        return {};
    }
    std::filesystem::path path = value;
    if (path.is_absolute()) {
        return path;
    }
    return loraModelDir() / path;
}

void tcApp::refreshLoraFiles() {
    loraFiles_.clear();
    selectedLora_ = -1;

    const auto root = loraModelDir();
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return;
    }

    for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (ec || !it->is_regular_file(ec)) {
            continue;
        }
        const auto path = it->path();
        if (isLoraFile(path)) {
            loraFiles_.push_back(path);
        }
    }

    std::sort(loraFiles_.begin(), loraFiles_.end(), [](const auto& left, const auto& right) {
        return left.generic_string() < right.generic_string();
    });

    if (loraFiles_.empty()) {
        return;
    }

    const auto current = resolveLoraPath(loraPath_.data());
    for (size_t i = 0; i < loraFiles_.size(); ++i) {
        if (std::filesystem::equivalent(current, loraFiles_[i], ec) && !ec) {
            selectedLora_ = static_cast<int>(i);
            return;
        }
        ec.clear();
    }
    selectedLora_ = 0;
    std::error_code relativeError;
    const auto relative = std::filesystem::relative(loraFiles_.front(), root, relativeError);
    copyText(loraPath_, relativeError ? loraFiles_.front().filename().string() : relative.generic_string());
}

void tcApp::generateControlImage() {
    auto source = resolveExamplePath(sourceImagePath_.data());
    if (source.empty()) {
        source = resolveExamplePath(initImagePath_.data());
    }
    if (source.empty() || !pathExists(source)) {
        lastError_ = "ControlNet preprocessing requires an existing source image.";
        status_ = "缺少控制源图";
        return;
    }

    const auto output = exampleRoot() / "inputs" / "control_canny_generated.png";
    tcx::sd::ControlPreprocessOptions options;
    options.lowThreshold = 32;
    options.highThreshold = 96;
    const auto result = tcx::sd::preprocessControlImage(source, output, options);
    if (!result.ok) {
        lastError_ = result.error;
        status_ = "控制图生成失败";
        return;
    }

    copyText(controlImagePath_, relativeToExamplePath(output));
    lastError_.clear();
    status_ = "已生成 ControlNet 控制图";
}

void tcApp::generateInpaintMask() {
    auto source = resolveExamplePath(initImagePath_.data());
    if (source.empty()) {
        source = resolveExamplePath(sourceImagePath_.data());
    }
    if (source.empty() || !pathExists(source)) {
        lastError_ = "Inpaint mask generation requires an existing source/init image.";
        status_ = "缺少蒙版源图";
        return;
    }

    const auto output = exampleRoot() / "inputs" / "mask_generated.png";
    tcx::sd::InpaintMaskOptions options;
    options.marginRatio = 0.28f;
    options.featherPixels = 12;
    const auto result = tcx::sd::createInpaintMask(source, output, options);
    if (!result.ok) {
        lastError_ = result.error;
        status_ = "蒙版生成失败";
        return;
    }

    copyText(maskImagePath_, relativeToExamplePath(output));
    lastError_.clear();
    status_ = "已生成 inpaint 蒙版";
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
    const auto workflow = static_cast<WorkflowMode>(std::clamp(workflowMode_, 0, 6));
    auto basePrompt = std::string(prompt_.data());
    if (usePromptComposer_ && profile.supportsIdeogramComposer) {
        auto composed = tcx::StableDiffusionRequest::fromIdeogram4(buildPromptTemplate());
        basePrompt = composed.prompt;
        copyText(prompt_, composed.prompt);
        copyText(negativePrompt_, composed.negativePrompt);
    }

    tcx::StableDiffusionRequest request;
    if (workflow == WorkflowMode::ImageToImage) {
        const auto image = resolveExamplePath(initImagePath_.data());
        if (image.empty()) {
            lastError_ = "Image-to-image requires an init image path.";
            status_ = "缺少输入图";
            return;
        }
        request = tcx::StableDiffusionRequest::imageToImage(basePrompt, image, strength_);
    } else if (workflow == WorkflowMode::Inpaint) {
        const auto image = resolveExamplePath(initImagePath_.data());
        const auto mask = resolveExamplePath(maskImagePath_.data());
        if (image.empty() || mask.empty()) {
            lastError_ = "Inpaint requires both init image and mask image paths.";
            status_ = "缺少修补输入";
            return;
        }
        request = tcx::StableDiffusionRequest::inpaint(basePrompt, image, mask, strength_);
    } else if (workflow == WorkflowMode::ControlNet) {
        const auto control = resolveExamplePath(controlImagePath_.data());
        if (control.empty()) {
            lastError_ = "ControlNet requires a control image path.";
            status_ = "缺少控制图";
            return;
        }
        request = tcx::StableDiffusionRequest::controlNet(basePrompt, control, controlStrength_);
    } else if (workflow == WorkflowMode::LoraStack) {
        const auto lora = resolveLoraPath(loraPath_.data());
        if (lora.empty()) {
            lastError_ = "LoRA stack requires a LoRA file path.";
            status_ = "缺少 LoRA";
            return;
        }
        request = tcx::StableDiffusionRequest::loraStack(basePrompt, {{lora, 0.8f}});
    } else if (workflow == WorkflowMode::Refine) {
        const auto source = resolveExamplePath(sourceImagePath_.data());
        if (source.empty()) {
            lastError_ = "Refine requires a source image path.";
            status_ = "缺少源图";
            return;
        }
        request = tcx::StableDiffusionRequest::refine(basePrompt, source, strength_);
    } else if (workflow == WorkflowMode::Upscale) {
        const auto source = resolveExamplePath(sourceImagePath_.data());
        if (source.empty()) {
            lastError_ = "Upscale requires a source image path.";
            status_ = "缺少源图";
            return;
        }
        request = tcx::StableDiffusionRequest::upscale(basePrompt, source, upscaleFactor_);
    } else {
        request = tcx::StableDiffusionRequest::textToImage(basePrompt);
    }

    if (usePromptComposer_ && profile.supportsIdeogramComposer) {
        request.negativePrompt = negativePrompt_.data();
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
    request.metadata["project_name"] = projectName_.data();

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
    const bool hasPixels = result.hasImage();
    const bool hasOutputFile = !result.outputPath.empty() && std::filesystem::exists(result.outputPath);
    if (!result.ok || (!hasPixels && !hasOutputFile)) {
        lastError_ = result.error;
        if (autoSave_) {
            const auto project = tcx::sd::GenerationProject::at(exampleRoot() / "outputs", projectName_.data());
            const auto outputDir = project.outputRoot / profile.id;
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

    if (hasPixels && looksLikeSafetyPlaceholder(result.pixels)) {
        result.metadata["placeholder_detected"] = "true";
        result.metadata["placeholder_detector"] = "neutral_gray_warning_screen";
        result.metadata["placeholder_note"] = "The model generated a neutral warning-style placeholder image; this is image content, not an addon safety filter.";
        lastError_ = "检测到模型生成了安全拦截占位图；这是模型画出来的内容，不是程序拦截。请使用官方验证模板、提高分辨率/步数，或改写提示词。";
        status_ = "检测到占位图";
        writeSmokeLog("placeholder detected");
    }

    if (autoSave_) {
        const auto project = tcx::sd::GenerationProject::at(exampleRoot() / "outputs", projectName_.data());
        const auto outputDir = project.outputRoot / profile.id;
        std::filesystem::create_directories(outputDir);
        lastOutput_ = outputDir / (std::string(profile.id) + "_job_" + std::to_string(result.jobId) + ".png");
        lastMetadata_ = lastOutput_;
        lastMetadata_.replace_extension(".json");
        result.metadata["saved_image_path"] = lastOutput_.string();
        if (hasPixels) {
            result.saveWithMetadata(lastOutput_, lastMetadata_);
        } else {
            result.metadata["preview_decode"] = "skipped_for_cli_process";
            std::error_code copyError;
            std::filesystem::copy_file(result.outputPath, lastOutput_, std::filesystem::copy_options::overwrite_existing, copyError);
            if (copyError) {
                result.metadata["file_copy_error"] = copyError.message();
                lastError_ = "生成完成，但复制输出文件失败: " + copyError.message();
            }
            result.saveMetadata(lastMetadata_);
        }
        writeSmokeLog("saved: " + lastOutput_.string());
        writeSmokeLog("saved metadata: " + lastMetadata_.string());
    }

    if (hasPixels) {
        preview_.allocate(result.pixels.getWidth(), result.pixels.getHeight(), result.pixels.getChannels());
        std::memcpy(preview_.getPixelsData(), result.pixels.getData(), result.pixels.getTotalBytes());
        preview_.setDirty();
        preview_.update();
    }
    if (smokeMode_) {
        smokeExitRequested_ = true;
    }

    status_ = hasPixels ? "生成完成" : "生成完成，已保存文件";
}

void tcApp::drawGui() {
    imguiBegin();

    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(kPanelWidth, 850), ImGuiCond_FirstUseEver);
    ImGui::Begin("tcxStableDiffusion 多模型工作台", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(colorFromBytes(225, 193, 106), "状态");
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
        kProfiles[3].label,
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

    ImGui::SeparatorText("工作流");
    const char* workflowLabels[] = {
        "文生图",
        "图生图",
        "局部重绘",
        "ControlNet Canny",
        "LoRA 风格",
        "细化",
        "放大/细化",
    };
    ImGui::TextUnformatted("任务类型");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##workflow", &workflowMode_, workflowLabels, 7)) {
        if (static_cast<WorkflowMode>(workflowMode_) == WorkflowMode::ControlNet && selectedModel_ != 3 && !busy) {
            selectModel(3, true);
        }
    }
    inputTextFullWidth("项目名", "##project_name", projectName_);

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

    const auto workflow = static_cast<WorkflowMode>(std::clamp(workflowMode_, 0, 6));
    if (workflow == WorkflowMode::ImageToImage || workflow == WorkflowMode::Inpaint) {
        inputTextFullWidth("输入图路径", "##init_image", initImagePath_);
        sliderFloatFullWidth("重绘强度", "##strength_img", &strength_, 0.05f, 1.0f);
    }
    if (workflow == WorkflowMode::Inpaint) {
        inputTextFullWidth("遮罩图路径", "##mask_image", maskImagePath_);
        if (ImGui::Button("由输入图生成中心蒙版", ImVec2(-1, 30))) {
            generateInpaintMask();
        }
    }
    if (workflow == WorkflowMode::ControlNet) {
        inputTextFullWidth("控制源图路径", "##control_source_image", sourceImagePath_);
        if (ImGui::Button("由源图生成 Canny 控制图", ImVec2(-1, 30))) {
            generateControlImage();
        }
        inputTextFullWidth("ControlNet 控制图路径", "##control_image", controlImagePath_);
        sliderFloatFullWidth("ControlNet 强度", "##control_strength", &controlStrength_, 0.0f, 2.0f);
    }
    if (workflow == WorkflowMode::LoraStack) {
        ImGui::TextWrapped("LoRA 根目录");
        ImGui::PushStyleColor(ImGuiCol_Text, colorFromBytes(168, 180, 198));
        ImGui::TextWrapped("%s", loraModelDir().string().c_str());
        ImGui::PopStyleColor();
        if (ImGui::Button("刷新 LoRA 列表", ImVec2(-1, 30))) {
            refreshLoraFiles();
        }
        if (!loraFiles_.empty()) {
            std::string preview = "选择 LoRA";
            if (selectedLora_ >= 0 && selectedLora_ < static_cast<int>(loraFiles_.size())) {
                std::error_code ec;
                auto relative = std::filesystem::relative(loraFiles_[static_cast<size_t>(selectedLora_)], loraModelDir(), ec);
                preview = ec ? loraFiles_[static_cast<size_t>(selectedLora_)].filename().string() : relative.generic_string();
            }
            ImGui::TextUnformatted("LoRA 列表");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##lora_files", preview.c_str())) {
                for (size_t i = 0; i < loraFiles_.size(); ++i) {
                    std::error_code ec;
                    auto relative = std::filesystem::relative(loraFiles_[i], loraModelDir(), ec);
                    const std::string item = ec ? loraFiles_[i].filename().string() : relative.generic_string();
                    const bool selected = selectedLora_ == static_cast<int>(i);
                    if (ImGui::Selectable(item.c_str(), selected)) {
                        selectedLora_ = static_cast<int>(i);
                        copyText(loraPath_, item);
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::TextWrapped("未发现 LoRA 文件；将 .safetensors 放入上面的根目录后刷新。");
        }
        inputTextFullWidth("LoRA 文件路径", "##lora_path", loraPath_);
    }
    if (workflow == WorkflowMode::Refine || workflow == WorkflowMode::Upscale) {
        inputTextFullWidth("源图路径", "##source_image", sourceImagePath_);
        sliderFloatFullWidth("细化强度", "##strength_refine", &strength_, 0.05f, 1.0f);
    }
    if (workflow == WorkflowMode::Upscale) {
        sliderFloatFullWidth("放大倍率", "##upscale_factor", &upscaleFactor_, 1.0f, 4.0f);
    }

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
