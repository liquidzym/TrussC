#include "tcApp.h"

#include "tc/utils/tcJson.h"

#include <array>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::filesystem::path currentExecutablePath() {
#ifdef _WIN32
    std::array<wchar_t, 4096> buffer{};
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size > 0 && size < buffer.size()) {
        return std::filesystem::path(buffer.data());
    }
#endif
    return {};
}

} // namespace

void tcApp::setup() {
    bridgeMessageListener_ = bridge_.onMessage.listen(this, &tcApp::handleBridgeMessage);

    const auto root = webRoot();
    if (!std::filesystem::is_regular_file(root / "dist" / "index.html")) {
        lastError_ = "Web UI is missing: " + (root / "dist" / "index.html").string();
        return;
    }

    tcxCEF::LocalAssetServerSettings assetSettings;
    assetSettings.root = root;
    if (!assetServer_.start(assetSettings)) {
        lastError_ = "Failed to start local asset server at " + root.string();
        return;
    }

    tcxCEF::WebSocketBridgeSettings bridgeSettings;
    bridgeSettings.path = "/bridge";
    if (!bridge_.start(bridgeSettings)) {
        lastError_ = "Failed to start WebSocket bridge";
        assetServer_.stop();
        return;
    }

    startWorker();
    if (!lastError_.empty()) {
        bridge_.stop();
        assetServer_.stop();
        return;
    }

    tcxCEF::BrowserSettings browserSettings;
    browserSettings.url = assetServer_.url("/dist/index.html") + "?bridgePort=" + std::to_string(bridge_.port());
    browserSettings.showWindow = true;
    browserSettings.openDevTools = false;
    browserSettings.width = 1440;
    browserSettings.height = 920;
    if (!browser_.setup(browserSettings)) {
        lastError_ = browser_.lastError();
        worker_.stop();
        bridge_.stop();
        assetServer_.stop();
        return;
    }

    status_ = "就绪";
    sendStatus("native-host", "宿主已就绪");
}

void tcApp::update() {
    browser_.update();
    worker_.drainMessages([this](const std::string& text) {
        bridge_.broadcast(text);
    });
    if (!browser_.lastError().empty()) {
        lastError_ = browser_.lastError();
    }
}

void tcApp::draw() {
    tc::setColor(0.05f, 0.045f, 0.035f, 1.0f);
    tc::drawRect(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    if (!lastError_.empty()) {
        tc::setColor(0.92f, 0.74f, 0.38f, 1.0f);
        tc::drawBitmapString("workflow-web-cef: " + lastError_, 24, 42);
    } else {
        tc::setColor(0.78f, 0.62f, 0.27f, 1.0f);
        tc::drawBitmapString("workflow-web-cef: " + status_, 24, 42);
    }
}

void tcApp::cleanup() {
    browser_.shutdown();
    worker_.stop();
    bridge_.stop();
    assetServer_.stop();
}

std::filesystem::path tcApp::executableDir() const {
    const auto exe = currentExecutablePath();
    if (!exe.empty()) {
        return exe.parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path tcApp::webRoot() const {
    return executableDir() / "workflow-web-cef" / "web";
}

std::filesystem::path tcApp::nodeExecutable() const {
#ifdef _WIN32
    return executableDir() / "runtime" / "node" / "node.exe";
#else
    return executableDir() / "runtime" / "node" / "bin" / "node";
#endif
}

std::filesystem::path tcApp::workerScript() const {
    return executableDir() / "workflow-web-cef" / "worker" / "dist" / "worker.mjs";
}

void tcApp::startWorker() {
    NodeWorkerSettings settings;
    settings.nodeExecutable = nodeExecutable();
    settings.workerScript = workerScript();
    settings.cwd = executableDir();
    if (!worker_.start(settings)) {
        lastError_ = worker_.lastError();
        return;
    }
    sendStatus("native-host", "Worker 已启动");
}

void tcApp::handleBridgeMessage(tcxCEF::WebSocketBridgeMessage& message) {
    if (!worker_.send(message.text)) {
        bridge_.send(message.clientId, workerUnavailableJson());
    }
}

void tcApp::sendStatus(const std::string& stage, const std::string& detail) {
    tc::Json message;
    message["type"] = "hostStatus";
    message["stage"] = stage;
    message["detail"] = detail;
    bridge_.broadcast(message.dump());
}

std::string tcApp::workerUnavailableJson() const {
    tc::Json error;
    error["type"] = "error";
    error["id"] = "";
    error["ok"] = false;
    error["error"] = {
        {"code", "WORKER_UNAVAILABLE"},
        {"message", worker_.lastError()},
        {"remediation_hints", tc::Json::array({
            "重启应用。",
            "检查内置 runtime/node/node.exe 和 workflow-web-cef/worker/dist/worker.mjs。"
        })}
    };
    return error.dump();
}
