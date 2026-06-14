#include <tcxNFC.h>

#include <iostream>
#include <utility>

namespace {

class MockReader final : public tcx::nfc::IReader {
public:
    tcx::nfc::Result<tcx::nfc::CardUid> readUid(uint8_t, uint8_t) override {
        tcx::nfc::CardUid uid;
        uid.cardType = 0x04;
        uid.uidBytes = {0x04, 0xA1, 0xB2, 0xC3};
        uid.uidHex = tcx::nfc::formatUidHex(uid.uidBytes);
        return tcx::nfc::Result<tcx::nfc::CardUid>::success(std::move(uid));
    }

    tcx::nfc::Result<tcx::nfc::WriteResult> writeUrlRawNtag(const std::string& url, int, int) override {
        std::cout << "[trussc_activation_mock] write URL " << url << '\n';
        tcx::nfc::WriteResult result;
        result.writeStrategy = "mock_raw_0x62";
        result.verificationLevel = "mock_verified";
        result.pagesWritten = 7;
        return tcx::nfc::Result<tcx::nfc::WriteResult>::success(std::move(result));
    }
};

class PrintPlugin final : public tcx::nfc::IActivationPlugin {
public:
    void onActivation(const tcx::nfc::ActivationEvent& event) override {
        std::cout << "[trussc_activation_mock] plugin uid=" << event.uid
                  << " device=" << event.deviceId
                  << " reader=" << event.readerId
                  << " url=" << event.url << '\n';
    }
};

} // namespace

int main() {
    MockReader reader;
    tcx::nfc::FixedUrlTokenProvider tokens("https://wstree.cn/t/TRUSSCMOCK");

    PrintPlugin printPlugin;
    tcx::nfc::PluginHost plugins;
    plugins.add(&printPlugin);

    tcx::nfc::ActivationConfig config;
    config.deviceId = "trussc-dev";
    config.readerId = "mock-bks710i";

    auto result = tcx::nfc::ActivationRuntime::runOnce(config, reader, tokens, &plugins);
    if (!result.ok) {
        std::cerr << result.error << '\n';
        return 1;
    }

    std::cout << "[trussc_activation_mock] activation uid=" << result.value.uid
              << " mode=" << result.value.urlMode
              << " verification=" << result.value.verificationLevel << '\n';
    return 0;
}
