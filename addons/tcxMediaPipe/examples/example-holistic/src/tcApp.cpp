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
    settings.enableHand = true;
    settings.enablePose = true;
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

void tcApp::drawHandLandmarks(const tcx::mediapipe::HandResult& result, float width, float height) {
    for (const auto& hand : result.hands) {
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
            drawCircle(p.x, p.y, 3.5f);
        }
    }
}

void tcApp::drawPoseLandmarks(const tcx::mediapipe::PoseResult& result, float width, float height) {
    const array<array<float, 3>, 4> lineColors = {{
        {{0.0f, 0.65f, 1.0f}},
        {{1.0f, 0.45f, 0.75f}},
        {{1.0f, 0.78f, 0.25f}},
        {{0.35f, 0.95f, 0.45f}},
    }};
    const array<array<float, 3>, 4> pointColors = {{
        {{0.25f, 0.95f, 0.35f}},
        {{1.0f, 0.65f, 0.9f}},
        {{1.0f, 0.95f, 0.35f}},
        {{0.55f, 1.0f, 0.65f}},
    }};

    for (size_t poseIndex = 0; poseIndex < result.poses.size(); ++poseIndex) {
        const auto& pose = result.poses[poseIndex];
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
            drawCircle(p.x, p.y, 3.0f);
        }
    }
}

void tcApp::drawFaceLandmarks(const tcx::mediapipe::FaceResult& result, float width, float height) {
    if (result.faces.empty()) {
        return;
    }

    const array<array<float, 3>, 4> faceColors = {{
        {{1.0f, 0.35f, 0.75f}},
        {{0.3f, 0.85f, 1.0f}},
        {{1.0f, 0.85f, 0.25f}},
        {{0.35f, 1.0f, 0.5f}},
    }};
    for (size_t faceIndex = 0; faceIndex < result.faces.size(); ++faceIndex) {
        const auto& face = result.faces[faceIndex];
        const auto& color = faceColors[faceIndex % faceColors.size()];
        setColor(color[0], color[1], color[2]);
        for (size_t i = 0; i < face.landmarks.size(); i += 4) {
            const Vec2 p = toScreen(face.landmarks[i], width, height);
            drawCircle(p.x, p.y, 2.0f);
        }
    }
}

void tcApp::draw() {
    clear(0.045f, 0.055f, 0.07f);
    setColor(colors::white);
    drawBitmapString("tcxMediaPipe hand + pose + face example", 20, 28);

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

    const float width = static_cast<float>(getWindowWidth());
    const float height = static_cast<float>(getWindowHeight());
    const auto hands = mediaPipe_.getHandResult();
    const auto pose = mediaPipe_.getPoseResult();
    const auto face = mediaPipe_.getFaceResult();

    bool drewAnything = false;
    if (pose && !pose->poses.empty()) {
        drawPoseLandmarks(*pose, width, height);
        drewAnything = true;
    }
    if (face && !face->faces.empty()) {
        drawFaceLandmarks(*face, width, height);
        drewAnything = true;
    }
    if (hands && !hands->hands.empty()) {
        drawHandLandmarks(*hands, width, height);
        drewAnything = true;
    }

    setColor(colors::yellow);
    drawBitmapString("Hands: " + toString(hands ? static_cast<int>(hands->hands.size()) : 0), 20, statusY);
    drawBitmapString("Poses: " + toString(pose ? static_cast<int>(pose->poses.size()) : 0), 20, statusY + 20);
    drawBitmapString("Faces: " + toString(face ? static_cast<int>(face->faces.size()) : 0), 20, statusY + 40);

    if (!drewAnything) {
        setColor(0.5f);
        drawBitmapString("Waiting for hand, pose, or face landmarks...", 20, statusY + 70);
    }
}
