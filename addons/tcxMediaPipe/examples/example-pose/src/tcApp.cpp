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
    settings.enablePose = true;
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
    clear(0.05f, 0.06f, 0.08f);
    setColor(colors::white);
    drawBitmapString("tcxMediaPipe pose example", 20, 28);

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

    const auto result = mediaPipe_.getPoseResult();
    if (!result || result->poses.empty()) {
        setColor(0.5f);
        drawBitmapString("Waiting for pose landmarks...", 20, statusY);
        return;
    }

    const float width = static_cast<float>(getWindowWidth());
    const float height = static_cast<float>(getWindowHeight());
    const array<array<float, 3>, 4> lineColors = {{
        {{0.0f, 0.72f, 1.0f}},
        {{1.0f, 0.45f, 0.75f}},
        {{1.0f, 0.78f, 0.25f}},
        {{0.35f, 0.95f, 0.45f}},
    }};
    const array<array<float, 3>, 4> pointColors = {{
        {{0.15f, 1.0f, 0.45f}},
        {{1.0f, 0.65f, 0.9f}},
        {{1.0f, 0.95f, 0.35f}},
        {{0.55f, 1.0f, 0.65f}},
    }};

    for (size_t poseIndex = 0; poseIndex < result->poses.size(); ++poseIndex) {
        const auto& pose = result->poses[poseIndex];
        const auto& lineColor = lineColors[poseIndex % lineColors.size()];
        setColor(lineColor[0], lineColor[1], lineColor[2]);
        for (const auto& [a, b] : tcx::mediapipe::kPoseConnections) {
            if (a < static_cast<int>(pose.landmarks.size()) && b < static_cast<int>(pose.landmarks.size())) {
                const Vec2 p0 = toScreen(pose.landmarks[a], width, height);
                const Vec2 p1 = toScreen(pose.landmarks[b], width, height);
                drawLine(p0.x, p0.y, p1.x, p1.y);
            }
        }

        const auto& pointColor = pointColors[poseIndex % pointColors.size()];
        setColor(pointColor[0], pointColor[1], pointColor[2]);
        for (const auto& landmark : pose.landmarks) {
            const Vec2 p = toScreen(landmark, width, height);
            drawCircle(p.x, p.y, 3.5f);
        }
    }

    setColor(colors::yellow);
    drawBitmapString("Poses: " + toString(static_cast<int>(result->poses.size())), 20, statusY);
}
