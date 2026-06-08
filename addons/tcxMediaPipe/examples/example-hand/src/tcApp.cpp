#include "tcApp.h"

namespace {

Vec2 toScreen(const tcx::mediapipe::Landmark& point, float width, float height) {
    return Vec2(point.x * width, point.y * height);
}

float distance2d(const tcx::mediapipe::Landmark& a, const tcx::mediapipe::Landmark& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
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
    settings.enableHand = true;
    settings.inputWidth = 640;
    settings.inputHeight = 480;
    settings.processingWidth = 480;
    settings.processingHeight = 360;
    settings.maxFPS = 30;
    settings.showCEFWindow = true;

    if (!mediaPipe_.setup(settings)) {
        setupError_ = mediaPipe_.lastError();
    }
}

void tcApp::update() {
    mediaPipe_.update();
}

void tcApp::draw() {
    clear(0.06f, 0.07f, 0.08f);
    setColor(colors::white);
    drawBitmapString("tcxMediaPipe hand example", 20, 28);

    if (!setupError_.empty()) {
        setColor(colors::red);
        drawBitmapString(setupError_, 20, 55);
        drawBitmapString("Run tcxCEF setup and tcxMediaPipe asset scripts first.", 20, 75);
        return;
    }

    setColor(0.8f);
    drawBitmapString("Delegate: " + mediaPipe_.activeDelegate(), 20, 55);
    drawBitmapString("Ready: " + string(mediaPipe_.isReady() ? "yes" : "no"), 20, 75);
    drawBitmapString("Source FPS: " + toString(mediaPipe_.getSourceFPS(), 1), 20, 95);
    drawBitmapString("Inference FPS: " + toString(mediaPipe_.getInferenceFPS(), 1), 20, 115);
    drawBitmapString("Inference ms: " + toString(mediaPipe_.getAverageInferenceTimeMs(), 1), 20, 135);
    drawBitmapString("Bridge latency ms: " + toString(mediaPipe_.getBridgeLatencyMs(), 1), 20, 155);
    drawBitmapString("Frame age ms: " + toString(mediaPipe_.getFrameAgeMs(), 1), 20, 175);
    drawBitmapString("GPU: " + compactRenderer(mediaPipe_.gpuInfo()), 20, 195);
    int statusY = 225;
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

    const auto result = mediaPipe_.getHandResult();
    if (!result || result->hands.empty()) {
        setColor(0.5f);
        drawBitmapString("Waiting for hand landmarks...", 20, statusY);
        return;
    }

    const float width = static_cast<float>(getWindowWidth());
    const float height = static_cast<float>(getWindowHeight());
    int infoY = statusY;
    setColor(colors::yellow);
    drawBitmapString("Hands: " + toString(static_cast<int>(result->hands.size())), 20, infoY);
    infoY += 20;
    for (const auto& hand : result->hands) {
        setColor(colors::cyan);
        for (const auto& [a, b] : tcx::mediapipe::kHandConnections) {
            if (a < static_cast<int>(hand.landmarks.size()) && b < static_cast<int>(hand.landmarks.size())) {
                const Vec2 p0 = toScreen(hand.landmarks[a], width, height);
                const Vec2 p1 = toScreen(hand.landmarks[b], width, height);
                drawLine(p0.x, p0.y, p1.x, p1.y);
            }
        }

        setColor(colors::lime);
        for (const auto& landmark : hand.landmarks) {
            const Vec2 p = toScreen(landmark, width, height);
            drawCircle(p.x, p.y, 4.0f);
        }

        setColor(colors::yellow);
        drawBitmapString(hand.handedness + " " + toString(hand.score, 2), 20, infoY);
        infoY += 20;
        if (hand.landmarks.size() > 8) {
            drawBitmapString("Pinch distance: " + toString(distance2d(hand.landmarks[4], hand.landmarks[8]), 3), 20, infoY);
            infoY += 20;
        }
    }
}
