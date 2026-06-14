#include "gui/MayaRFIDGuiApp.h"

#include <chrono>
#include <sstream>
#include <utility>

namespace maya_rfid {
namespace {

std::string summaryLine(const ActivationSummary& summary) {
    if (summary.uid.empty()) {
        return "last: none";
    }
    return "last: uid " + summary.uid + " -> " + summary.url + " (" + summary.verificationLevel + ")";
}

std::string shortPath(const std::filesystem::path& path) {
    const auto text = path.string();
    constexpr size_t maxLen = 92;
    if (text.size() <= maxLen) {
        return text;
    }
    return "..." + text.substr(text.size() - maxLen);
}

void drawLine(const std::string& text, float x, float& y, float r, float g, float b) {
    tc::setColor(r, g, b, 1.0f);
    tc::drawBitmapString(text, x, y);
    y += 22.0f;
}

} // namespace

void MayaRFIDGuiApp::setup() {
    tc::setIndependentFps(tc::VSYNC, tc::VSYNC);
    reloadConfig();
}

void MayaRFIDGuiApp::update() {
    if (hasPending_) {
        finishPending();
        tc::redraw();
    }
}

void MayaRFIDGuiApp::draw() {
    tc::clear(0.07f, 0.075f, 0.08f);

    const float width = getWidth();
    const float height = getHeight();
    const float pad = 28.0f;
    float y = 38.0f;

    tc::fill();
    tc::setColor(0.14f, 0.18f, 0.20f, 1.0f);
    tc::drawRect(0, 0, width, 88);
    tc::setColor(0.08f, 0.55f, 0.46f, 1.0f);
    tc::drawRect(0, 86, width, 2);

    tc::setColor(0.94f, 0.96f, 0.94f, 1.0f);
    tc::drawBitmapString("mayaRFID", pad, y, 2.0f);
    tc::setColor(0.64f, 0.72f, 0.72f, 1.0f);
    tc::drawBitmapString("tcxNFC GUI runtime", pad + 170.0f, y + 4.0f);

    y = 126.0f;
    drawLine("config: " + shortPath(configPath_), pad, y, 0.72f, 0.78f, 0.78f);
    drawLine("device: " + config_.deviceId + "  reader: " + config_.readerId, pad, y, 0.92f, 0.94f, 0.90f);
    drawLine("endpoint: " + config_.readerHost + ":" + std::to_string(config_.readerPort), pad, y, 0.92f, 0.94f, 0.90f);
    if (!config_.readerSourceHost.empty()) {
        drawLine("source host: " + config_.readerSourceHost, pad, y, 0.92f, 0.94f, 0.90f);
    }
    drawLine("fallback url: " + config_.fixedFallbackUrl, pad, y, 0.92f, 0.94f, 0.90f);

    y += 12.0f;
    tc::setColor(hasPending_ ? 0.78f : 0.11f, hasPending_ ? 0.55f : 0.45f, hasPending_ ? 0.18f : 0.38f, 1.0f);
    tc::drawRect(pad, y - 14.0f, width - pad * 2.0f, 34.0f);
    tc::setColor(1.0f, 1.0f, 1.0f, 1.0f);
    tc::drawBitmapString("status: " + status_, pad + 12.0f, y + 8.0f);

    y += 58.0f;
    drawLine(summaryLine(lastSummary_), pad, y, 0.82f, 0.88f, 0.84f);
    drawLine("runs: " + std::to_string(runCount_), pad, y, 0.62f, 0.70f, 0.70f);

    const float footerY = height - 34.0f;
    tc::setColor(0.46f, 0.50f, 0.50f, 1.0f);
    tc::drawBitmapString("[M] mock activation   [H] hardware activation   [C] reload config   [N] NDEF preview", pad, footerY);
}

void MayaRFIDGuiApp::keyPressed(int key) {
    if (key == 'M') {
        startActivation(true);
    } else if (key == 'H') {
        startActivation(false);
    } else if (key == 'C') {
        reloadConfig();
    } else if (key == 'N') {
        auto built = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv(config_.fixedFallbackUrl, static_cast<size_t>(config_.ntagMaxUserBytes));
        if (built.ok) {
            setStatus("NDEF preview ok, pages " + std::to_string(built.value.pageCount));
        } else {
            setStatus("NDEF preview failed: " + built.error);
        }
    }
    tc::redraw();
}

void MayaRFIDGuiApp::reloadConfig() {
    configPath_ = resolveConfigPath({}, nullptr);
    auto loaded = loadConfig(configPath_);
    if (!loaded.ok) {
        configLoaded_ = false;
        setStatus("config failed: " + loaded.error);
        return;
    }

    config_ = std::move(loaded.value);
    configLoaded_ = true;
    setStatus("config loaded");
}

void MayaRFIDGuiApp::startActivation(bool mock) {
    if (!configLoaded_) {
        setStatus("config not loaded");
        return;
    }
    if (hasPending_) {
        setStatus("activation already running");
        return;
    }

    setStatus(mock ? "mock activation running" : "hardware activation running");
    const AppConfig runConfig = config_;
    pending_ = std::async(std::launch::async, [runConfig, mock]() {
        return activateOnce(runConfig, mock);
    });
    hasPending_ = true;
}

void MayaRFIDGuiApp::finishPending() {
    using namespace std::chrono_literals;
    if (!hasPending_ || !pending_.valid() || pending_.wait_for(0ms) != std::future_status::ready) {
        return;
    }

    auto result = pending_.get();
    hasPending_ = false;
    if (!result.ok) {
        setStatus("activation failed: " + result.error);
        return;
    }

    lastSummary_ = std::move(result.value);
    ++runCount_;
    setStatus("activation ok: " + lastSummary_.verificationLevel);
}

void MayaRFIDGuiApp::setStatus(std::string status) {
    status_ = std::move(status);
    tc::redraw();
}

} // namespace maya_rfid
