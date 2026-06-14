#include "runtime/ActivationRuntime.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

namespace maya_rfid {
namespace {

class JsonlPlugin final : public tcx::nfc::IActivationPlugin {
public:
    explicit JsonlPlugin(const AppConfig& config)
        : config_(config) {}

    void onActivation(const tcx::nfc::ActivationEvent& event) override {
        std::filesystem::create_directories(config_.eventJsonlPath.parent_path());
        std::ofstream out(config_.eventJsonlPath, std::ios::app);
        if (!out) {
            std::cerr << "event log open failed: " << config_.eventJsonlPath << '\n';
            return;
        }

        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        out << "{\"created_at\":" << now
            << ",\"device_id\":\"" << event.deviceId
            << "\",\"reader_id\":\"" << event.readerId
            << "\",\"uid\":\"" << event.uid
            << "\",\"url\":\"" << event.url
            << "\",\"url_mode\":\"" << event.urlMode
            << "\",\"write_strategy\":\"" << event.writeStrategy
            << "\",\"verification\":\"" << event.verificationLevel
            << "\"}\n";
    }

private:
    AppConfig config_;
};

class MockReader final : public tcx::nfc::IReader {
public:
    tcx::nfc::Result<tcx::nfc::CardUid> readUid(uint8_t, uint8_t) override {
        tcx::nfc::CardUid uid;
        uid.cardType = 0x04;
        uid.uidBytes = {0x09, 0x44, 0x00, 0x04, 0x08, 0x58, 0x62};
        uid.uidHex = tcx::nfc::formatUidHex(uid.uidBytes);
        return tcx::nfc::Result<tcx::nfc::CardUid>::success(std::move(uid));
    }

    tcx::nfc::Result<tcx::nfc::WriteResult> writeUrlRawNtag(const std::string& url, int maxUserBytes, int startPage) override {
        auto ndef = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv(url, static_cast<size_t>(maxUserBytes));
        if (!ndef.ok) {
            return tcx::nfc::Result<tcx::nfc::WriteResult>::failure(ndef.error);
        }
        lastUrl = url;
        lastStartPage = startPage;

        tcx::nfc::WriteResult result;
        result.writeStrategy = "mock_raw_0x62";
        result.verificationLevel = "mock_verified";
        result.pagesWritten = ndef.value.pageCount;
        return tcx::nfc::Result<tcx::nfc::WriteResult>::success(std::move(result));
    }

    std::string lastUrl;
    int lastStartPage = 4;
};

template <typename Reader>
tcx::nfc::Result<ActivationSummary> runWithReader(const AppConfig& config, Reader& reader) {
    tcx::nfc::FixedUrlTokenProvider fallback(config.fixedFallbackUrl);
    JsonlPlugin logger(config);
    tcx::nfc::PluginHost plugins;
    plugins.add(&logger);

    tcx::nfc::ActivationConfig activationConfig;
    activationConfig.deviceId = config.deviceId;
    activationConfig.readerId = config.readerId;
    activationConfig.ntagStartPage = config.ntagStartPage;
    activationConfig.ntagMaxUserBytes = config.ntagMaxUserBytes;

    auto result = tcx::nfc::ActivationRuntime::runOnce(activationConfig, reader, fallback, &plugins);
    if (!result.ok) {
        return tcx::nfc::Result<ActivationSummary>::failure(result.error);
    }

    ActivationSummary summary;
    summary.uid = result.value.uid;
    summary.token = result.value.token;
    summary.url = result.value.url;
    summary.urlMode = result.value.urlMode;
    summary.writeStrategy = result.value.writeStrategy;
    summary.verificationLevel = result.value.verificationLevel;
    summary.pagesWritten = result.value.pagesWritten;
    return tcx::nfc::Result<ActivationSummary>::success(std::move(summary));
}

void printSummary(const ActivationSummary& summary) {
    std::cout << "uid: " << summary.uid << '\n';
    std::cout << "url: " << summary.url << '\n';
    std::cout << "url_mode: " << summary.urlMode << '\n';
    std::cout << "write_strategy: " << summary.writeStrategy << '\n';
    std::cout << "verification: " << summary.verificationLevel << '\n';
    std::cout << "pages: " << summary.pagesWritten << '\n';
}

int exitCodeFor(const ActivationSummary& summary) {
    return summary.verificationLevel.find("mismatch") == std::string::npos ? 0 : 2;
}

} // namespace

tcx::nfc::Result<ActivationSummary> activateOnce(const AppConfig& config, bool mock) {
    std::filesystem::create_directories(config.eventJsonlPath.parent_path());
    if (mock) {
        MockReader reader;
        return runWithReader(config, reader);
    }

    const auto endpoint = endpointFor(config);
    tcx::nfc::TcpSocketTransport privateTcp(endpoint);
    tcx::nfc::TcpSocketTransport modbusTcp(endpoint);
    tcx::nfc::Bks710iReader reader(privateTcp, modbusTcp);
    return runWithReader(config, reader);
}

int runActivationOnce(const AppConfig& config, bool mock) {
    auto result = activateOnce(config, mock);
    if (!result.ok) {
        std::cerr << "activation failed: " << result.error << '\n';
        return 1;
    }

    printSummary(result.value);
    return exitCodeFor(result.value);
}

int runActivationLoop(const AppConfig& config, int loopCount, bool mock) {
    std::cout << "mayaRFID loop started\n";
    int count = 0;
    while (loopCount <= 0 || count < loopCount) {
        ++count;
        const int code = runActivationOnce(config, mock);
        if (code != 0 && !mock) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }
        if (mock || loopCount == 1) {
            return code;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "mayaRFID loop stopped\n";
    return 0;
}

} // namespace maya_rfid
