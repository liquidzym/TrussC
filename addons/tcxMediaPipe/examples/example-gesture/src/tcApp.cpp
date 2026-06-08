#include "tcApp.h"

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

string gestureLabel(const tcx::mediapipe::Gesture& gesture) {
    const string name = gesture.displayName.empty() ? gesture.categoryName : gesture.displayName;
    return name.empty() ? "None" : name;
}

} // namespace

void tcApp::setup() {
    tcx::mediapipe::Settings settings;
    settings.enableGesture = true;
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
    drawBitmapString("tcxMediaPipe gesture example", 20, 28);

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

    const auto result = mediaPipe_.getGestureResult();
    if (!result || result->gestures.empty()) {
        setColor(0.5f);
        drawBitmapString("Waiting for gestures...", 20, statusY);
        return;
    }

    const float width = static_cast<float>(getWindowWidth());
    const float height = static_cast<float>(getWindowHeight());
    int infoY = statusY;
    setColor(colors::yellow);
    drawBitmapString("Gestures: " + toString(static_cast<int>(result->gestures.size())), 20, infoY);
    infoY += 20;
    for (const auto& gesture : result->gestures) {
        setColor(colors::cyan);
        for (const auto& [a, b] : tcx::mediapipe::kHandConnections) {
            if (a < static_cast<int>(gesture.landmarks.size()) && b < static_cast<int>(gesture.landmarks.size())) {
                const Vec2 p0 = toScreen(gesture.landmarks[a], width, height);
                const Vec2 p1 = toScreen(gesture.landmarks[b], width, height);
                drawLine(p0.x, p0.y, p1.x, p1.y);
            }
        }

        setColor(colors::lime);
        for (const auto& landmark : gesture.landmarks) {
            const Vec2 p = toScreen(landmark, width, height);
            drawCircle(p.x, p.y, 4.0f);
        }

        setColor(colors::yellow);
        drawBitmapString(gesture.handedness + " " + gestureLabel(gesture) + " " + toString(gesture.score, 2),
                         20,
                         infoY);
        infoY += 20;
    }
}
