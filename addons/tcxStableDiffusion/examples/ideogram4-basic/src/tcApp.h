#pragma once

#include <TrussC.h>
#include <tcxImGui.h>
#include <tcxStableDiffusion.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void cleanup() override;

private:
    void setupSmokeMode();
    void writeSmokeLog(const std::string& message);
    void selectModel(int index, bool resetPrompt);
    void refreshModelDir();
    void initializeModel();
    tcx::sd::IdeogramPrompt buildPromptTemplate() const;
    void applyPromptTemplate();
    void applyModelDefaults();
    void refreshLoraFiles();
    std::filesystem::path loraModelDir() const;
    std::filesystem::path resolveLoraPath(const char* text) const;
    void generateControlImage();
    void generateInpaintMask();
    void submitPrompt();
    void adoptResult(tcx::StableDiffusionImage&& result);
    void drawGui();

    tcx::StableDiffusion sd_;
    Image preview_;

    std::array<char, 4096> prompt_{};
    std::array<char, 1024> negativePrompt_{};
    std::array<char, 512> templateSubject_{};
    std::array<char, 256> templateText_{};
    std::array<char, 512> templateStyle_{};
    std::array<char, 256> templatePalette_{};
    std::array<char, 512> initImagePath_{};
    std::array<char, 512> maskImagePath_{};
    std::array<char, 512> controlImagePath_{};
    std::array<char, 512> sourceImagePath_{};
    std::array<char, 512> loraPath_{};
    std::array<char, 128> projectName_{};

    std::filesystem::path modelDir_;
    std::filesystem::path lastOutput_;
    std::filesystem::path lastMetadata_;
    std::vector<std::filesystem::path> loraFiles_;
    std::string status_ = "等待初始化";
    std::string lastError_;

    int selectedModel_ = 0;
    int width_ = 1024;
    int height_ = 1024;
    int steps_ = 8;
    int seed_ = -1;
    float cfgScale_ = 1.0f;
    float strength_ = 0.75f;
    float controlStrength_ = 0.9f;
    float upscaleFactor_ = 2.0f;
    int workflowMode_ = 0;
    int selectedLora_ = -1;
    bool lowVramMode_ = true;
    bool autoSave_ = true;
    bool usePromptComposer_ = true;
    bool setupAttempted_ = false;
    bool submitWhenReady_ = false;
    bool smokeMode_ = false;
    bool smokeExitRequested_ = false;
    std::uint64_t currentJob_ = 0;
};
