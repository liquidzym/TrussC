#include "tcxMediaPipeRuntime.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <stdexcept>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace tcx::mediapipe {
namespace {

using Json = nlohmann::json;

std::string delegateToString(Delegate delegate) {
    return delegate == Delegate::GPU ? "GPU" : "CPU";
}

std::string inputModeToString(InputMode mode) {
    return mode == InputMode::WebCamera ? "WebCamera" : "ExternalFrame";
}

int detectionLimit(bool multiPerson, int requested) {
    return multiPerson ? std::max(1, requested) : 1;
}

double currentEpochMs() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<double>(ms.count());
}

std::filesystem::path executablePath() {
#if defined(__APPLE__)
    std::array<char, 4096> stackBuffer{};
    uint32_t size = static_cast<uint32_t>(stackBuffer.size());
    if (_NSGetExecutablePath(stackBuffer.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(stackBuffer.data()));
    }

    std::string heapBuffer(size, '\0');
    if (_NSGetExecutablePath(heapBuffer.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(heapBuffer.c_str()));
    }
#elif defined(_WIN32)
    return {};
#else
    std::error_code error;
    auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        return path;
    }
#endif
    return {};
}

std::filesystem::path bundledWebRoot() {
    const auto executable = executablePath();
    if (executable.empty()) {
        return {};
    }

#if defined(__APPLE__)
    const auto contentsDir = executable.parent_path().parent_path();
    const auto candidate = contentsDir / "Resources" / "tcxMediaPipe" / "web";
#else
    const auto candidate = executable.parent_path() / "tcxMediaPipe" / "web";
#endif
    if (std::filesystem::is_regular_file(candidate / "dist" / "index.html")) {
        return candidate;
    }
    return {};
}

} // namespace

MediaPipe::MediaPipe() {
    bridgeMessageListener_ = bridge_.onMessage.listen(this, &MediaPipe::handleBridgeMessage);
    bridgeConnectListener_ = bridge_.onClientConnected.listen([this](int) {
        sendConfig();
    });
}

MediaPipe::~MediaPipe() {
    shutdown();
}

bool MediaPipe::setup(const Settings& settings) {
    shutdown();
    settings_ = settings;
    lastError_.clear();
    if (!settings_.configPath.empty()) {
        std::string configError;
        if (!loadSettingsJson(settings_, settings_.configPath, &configError)) {
            lastError_ = configError;
            return false;
        }
    }

    const std::filesystem::path webRoot = settings_.webRootOverride.empty() ? defaultWebRoot() : settings_.webRootOverride;
    if (!std::filesystem::is_directory(webRoot)) {
        lastError_ = "tcxMediaPipe web root does not exist: " + webRoot.string();
        return false;
    }
    if (!std::filesystem::is_regular_file(webRoot / "dist" / "index.html")) {
        lastError_ = "tcxMediaPipe web assets are missing. Run python addons/tcxMediaPipe/tools/build_web_assets.py";
        return false;
    }

    tcxCEF::LocalAssetServerSettings assetSettings;
    assetSettings.root = webRoot;
    if (!assetServer_.start(assetSettings)) {
        lastError_ = "Failed to start tcxCEF LocalAssetServer at " + webRoot.string();
        return false;
    }

    tcxCEF::WebSocketBridgeSettings bridgeSettings;
    if (!bridge_.start(bridgeSettings)) {
        lastError_ = "Failed to start tcxCEF WebSocketBridge";
        assetServer_.stop();
        return false;
    }

    tcxCEF::BrowserSettings browserSettings;
    browserSettings.url = assetServer_.url("/dist/index.html") + "?bridgePort=" + std::to_string(bridge_.port());
    browserSettings.showWindow = settings_.showCEFWindow;
    browserSettings.openDevTools = settings_.openDevTools;
    browserSettings.width = settings_.inputWidth;
    browserSettings.height = settings_.inputHeight;
    if (!browser_.setup(browserSettings)) {
        lastError_ = browser_.lastError();
        bridge_.stop();
        assetServer_.stop();
        return false;
    }

    return true;
}

void MediaPipe::update() {
    browser_.update();
    const std::string& browserError = browser_.lastError();
    if (!browserError.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!runtimeStatus_.ready) {
            lastError_ = browserError;
        }
    }
}

void MediaPipe::shutdown() {
    browser_.shutdown();
    bridge_.stop();
    assetServer_.stop();

    std::lock_guard<std::mutex> lock(mutex_);
    runtimeStatus_ = RuntimeStatus{};
    handResult_.reset();
    poseResult_.reset();
    faceResult_.reset();
    gestureResult_.reset();
    newHandResult_ = false;
    newPoseResult_ = false;
    newFaceResult_ = false;
    newGestureResult_ = false;
}

void MediaPipe::enableHandTracking(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.enableHand = enabled;
    }
    sendConfig();
}

void MediaPipe::enablePoseTracking(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.enablePose = enabled;
    }
    sendConfig();
}

void MediaPipe::enableFaceTracking(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.enableFace = enabled;
    }
    sendConfig();
}

void MediaPipe::enableGestureRecognition(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.enableGesture = enabled;
    }
    sendConfig();
}

void MediaPipe::setDelegate(Delegate delegate) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.delegate = delegate;
    }
    sendConfig();
}

void MediaPipe::setInputMode(InputMode mode) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.inputMode = mode;
    }
    sendConfig();
}

void MediaPipe::setMaxFPS(int fps) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.maxFPS = std::max(1, fps);
    }
    sendConfig();
}

void MediaPipe::setInputResolution(int width, int height) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.inputWidth = std::max(1, width);
        settings_.inputHeight = std::max(1, height);
    }
    sendConfig();
}

void MediaPipe::setProcessingResolution(int width, int height) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.processingWidth = std::max(0, width);
        settings_.processingHeight = std::max(0, height);
    }
    sendConfig();
}

void MediaPipe::setMirror(bool mirror) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.mirror = mirror;
    }
    sendConfig();
}

void MediaPipe::setMultiPerson(bool enabled) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.multiPerson = enabled;
    }
    sendConfig();
}

void MediaPipe::setMaxHands(int count) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.maxHands = std::max(1, count);
    }
    sendConfig();
}

void MediaPipe::setMaxPoses(int count) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.maxPoses = std::max(1, count);
    }
    sendConfig();
}

void MediaPipe::setMaxFaces(int count) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.maxFaces = std::max(1, count);
    }
    sendConfig();
}

void MediaPipe::setMaxGestures(int count) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.maxGestures = std::max(1, count);
    }
    sendConfig();
}

void MediaPipe::setDetectionLimits(int hands, int poses, int faces, int gestures) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_.maxHands = std::max(1, hands);
        settings_.maxPoses = std::max(1, poses);
        settings_.maxFaces = std::max(1, faces);
        settings_.maxGestures = std::max(1, gestures);
    }
    sendConfig();
}

std::optional<HandResult> MediaPipe::getHandResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handResult_;
}

std::optional<PoseResult> MediaPipe::getPoseResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return poseResult_;
}

std::optional<FaceResult> MediaPipe::getFaceResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return faceResult_;
}

std::optional<GestureResult> MediaPipe::getGestureResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gestureResult_;
}

bool MediaPipe::hasNewHandResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return newHandResult_;
}

bool MediaPipe::hasNewPoseResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return newPoseResult_;
}

bool MediaPipe::hasNewFaceResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return newFaceResult_;
}

bool MediaPipe::hasNewGestureResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return newGestureResult_;
}

void MediaPipe::sendFrame(std::span<const std::uint8_t>, int, int, double) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = "ExternalFrame input is not implemented in tcxMediaPipe 0.1.0";
}

float MediaPipe::getSourceFPS() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sourceFPS_;
}

float MediaPipe::getInferenceFPS() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inferenceFPS_;
}

float MediaPipe::getAverageInferenceTimeMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return averageInferenceTimeMs_;
}

float MediaPipe::getBridgeLatencyMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bridgeLatencyMs_;
}

std::string MediaPipe::activeDelegate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeStatus_.activeDelegate;
}

std::string MediaPipe::runtimeStage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeStatus_.stage;
}

std::string MediaPipe::runtimeDetail() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeStatus_.detail;
}

RuntimeGpuInfo MediaPipe::gpuInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeStatus_.gpu;
}

bool MediaPipe::isReady() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeStatus_.ready;
}

bool MediaPipe::isFallbackToCPU() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeStatus_.fallback;
}

std::string MediaPipe::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

std::filesystem::path MediaPipe::defaultWebRoot() const {
    const auto bundled = bundledWebRoot();
    if (!bundled.empty()) {
        return bundled;
    }
#ifdef TCXMEDIAPIPE_ADDON_ROOT
    return std::filesystem::path(TCXMEDIAPIPE_ADDON_ROOT) / "web";
#else
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "web";
#endif
}

void MediaPipe::sendConfig() {
    if (bridge_.isRunning()) {
        bridge_.broadcast(makeConfigJson());
    }
}

void MediaPipe::handleBridgeMessage(tcxCEF::WebSocketBridgeMessage& message) {
    try {
        const Json value = parseBridgeMessageJson(message.text);
        const std::string type = messageType(value);
        const RuntimeStats stats = parseRuntimeStats(value, currentEpochMs());
        std::lock_guard<std::mutex> lock(mutex_);
        if (type == "runtime_status") {
            runtimeStatus_ = parseRuntimeStatus(value);
            if (runtimeStatus_.ready) {
                lastError_.clear();
            } else if (!runtimeStatus_.reason.empty()) {
                lastError_ = runtimeStatus_.reason;
            }
        } else if (type == "hand_result") {
            handResult_ = parseHandResult(value);
            newHandResult_ = true;
        } else if (type == "pose_result") {
            poseResult_ = parsePoseResult(value);
            newPoseResult_ = true;
        } else if (type == "face_result") {
            faceResult_ = parseFaceResult(value);
            newFaceResult_ = true;
        } else if (type == "gesture_result") {
            gestureResult_ = parseGestureResult(value);
            newGestureResult_ = true;
        }

        if (type == "hand_result" || type == "pose_result" || type == "face_result" || type == "gesture_result") {
            sourceFPS_ = stats.sourceFPS;
            inferenceFPS_ = stats.inferenceFPS;
            averageInferenceTimeMs_ = stats.averageInferenceTimeMs;
            bridgeLatencyMs_ = stats.bridgeLatencyMs;
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = std::string("Failed to parse tcxMediaPipe bridge message: ") + e.what();
    }
}

std::string MediaPipe::makeConfigJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Json config;
    config["type"] = "config";
    config["delegate"] = delegateToString(settings_.delegate);
    config["inputMode"] = inputModeToString(settings_.inputMode);
    config["tasks"] = {
        {"hand", settings_.enableHand},
        {"pose", settings_.enablePose},
        {"face", settings_.enableFace},
        {"gesture", settings_.enableGesture},
    };
    config["maxFPS"] = settings_.maxFPS;
    config["inputWidth"] = settings_.inputWidth;
    config["inputHeight"] = settings_.inputHeight;
    config["processingWidth"] = settings_.processingWidth;
    config["processingHeight"] = settings_.processingHeight;
    config["mirror"] = settings_.mirror;
    config["multiPerson"] = settings_.multiPerson;
    config["maxHands"] = detectionLimit(settings_.multiPerson, settings_.maxHands);
    config["maxPoses"] = detectionLimit(settings_.multiPerson, settings_.maxPoses);
    config["maxFaces"] = detectionLimit(settings_.multiPerson, settings_.maxFaces);
    config["maxGestures"] = detectionLimit(settings_.multiPerson, settings_.maxGestures);
    config["outputFaceBlendshapes"] = settings_.outputFaceBlendshapes;
    config["outputFaceTransformationMatrix"] = settings_.outputFaceTransformationMatrix;
    return config.dump();
}

} // namespace tcx::mediapipe
