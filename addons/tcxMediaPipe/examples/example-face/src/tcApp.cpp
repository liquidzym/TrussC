#include "tcApp.h"

#include <array>

namespace {

Vec2 toScreen(const tcx::mediapipe::Landmark& point, float width, float height) {
    return Vec2(point.x * width, point.y * height);
}

string compactRenderer(const tcx::mediapipe::RuntimeGpuInfo& gpu) {
    string renderer = gpu.webglRenderer.empty() ? gpu.webglVendor : gpu.webglRenderer;
    if (renderer.empty()) {
        return "unknown";
    }
    return renderer.size() > 92 ? renderer.substr(0, 89) + "..." : renderer;
}

string compactStatus(const string& value) {
    return value.size() > 110 ? value.substr(0, 107) + "..." : value;
}

} // namespace

void tcApp::setup() {
    tcx::mediapipe::Settings settings;
    settings.enableFace = true;
    settings.inputWidth = 640;
    settings.inputHeight = 480;
    settings.processingWidth = 480;
    settings.processingHeight = 360;
    settings.maxFPS = 30;

    if (!mediaPipe_.setup(settings)) {
        setupError_ = mediaPipe_.lastError();
    }
}

void tcApp::update() {
    mediaPipe_.update();
}

void tcApp::draw() {
    clear(0.05f, 0.05f, 0.07f);
    setColor(colors::white);
    drawBitmapString("tcxMediaPipe face example", 20, 28);

    if (!setupError_.empty()) {
        setColor(colors::red);
        drawBitmapString(setupError_, 20, 55);
        return;
    }

    setColor(0.8f);
    drawBitmapString("Delegate: " + mediaPipe_.activeDelegate(), 20, 55);
    drawBitmapString("Ready: " + string(mediaPipe_.isReady() ? "yes" : "no"), 20, 75);
    drawBitmapString("Source FPS: " + toString(mediaPipe_.getSourceFPS(), 1), 20, 95);
    drawBitmapString("Inference FPS: " + toString(mediaPipe_.getInferenceFPS(), 1), 20, 115);
    drawBitmapString("Inference ms: " + toString(mediaPipe_.getAverageInferenceTimeMs(), 1), 20, 135);
    drawBitmapString("Bridge latency ms: " + toString(mediaPipe_.getBridgeLatencyMs(), 1), 20, 155);
    drawBitmapString("GPU: " + compactRenderer(mediaPipe_.gpuInfo()), 20, 175);
    int statusY = 205;
    if (!mediaPipe_.lastError().empty()) {
        setColor(colors::red);
        drawBitmapString(mediaPipe_.lastError(), 20, statusY);
        statusY += 25;
    }
    if ((!mediaPipe_.isReady() || mediaPipe_.getSourceFPS() <= 0.0f) && !mediaPipe_.runtimeDetail().empty()) {
        setColor(0.55f);
        drawBitmapString(compactStatus(mediaPipe_.runtimeDetail()), 20, statusY);
        statusY += 25;
    }

    const auto result = mediaPipe_.getFaceResult();
    if (!result || result->faces.empty()) {
        setColor(0.5f);
        drawBitmapString("Waiting for face landmarks...", 20, statusY);
        return;
    }

    const float width = static_cast<float>(getWindowWidth());
    const float height = static_cast<float>(getWindowHeight());
    const array<array<float, 3>, 4> faceColors = {{
        {{0.35f, 1.0f, 0.45f}},
        {{1.0f, 0.45f, 0.75f}},
        {{0.25f, 0.85f, 1.0f}},
        {{1.0f, 0.85f, 0.25f}},
    }};
    for (size_t faceIndex = 0; faceIndex < result->faces.size(); ++faceIndex) {
        const auto& face = result->faces[faceIndex];
        const auto& color = faceColors[faceIndex % faceColors.size()];
        setColor(color[0], color[1], color[2]);
        for (size_t i = 0; i < face.landmarks.size(); i += 4) {
            const Vec2 p = toScreen(face.landmarks[i], width, height);
            drawCircle(p.x, p.y, 2.5f);
        }
    }

    const auto& face = result->faces.front();
    const auto jawOpen = face.blendshapes.find("jawOpen");
    const auto smile = face.blendshapes.find("mouthSmileLeft");
    setColor(colors::yellow);
    drawBitmapString("Faces: " + toString(static_cast<int>(result->faces.size())), 20, statusY);
    drawBitmapString("Face 0 landmarks: " + toString(static_cast<int>(face.landmarks.size())), 20, statusY + 20);
    drawBitmapString("jawOpen: " + toString(jawOpen == face.blendshapes.end() ? 0.0f : jawOpen->second, 2), 20, statusY + 40);
    drawBitmapString("mouthSmileLeft: " + toString(smile == face.blendshapes.end() ? 0.0f : smile->second, 2), 20, statusY + 60);
    drawBitmapString("Matrix available: " + string(face.facialTransformationMatrix[15] != 0.0f ? "yes" : "no"), 20, statusY + 80);
}
