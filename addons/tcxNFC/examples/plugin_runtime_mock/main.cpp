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
        std::cout << "mock write URL: " << url << '\n';
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
        std::cout << "plugin activation uid=" << event.uid
                  << " url=" << event.url
                  << " reader=" << event.readerId << '\n';
    }
};

} // namespace

int main() {
    MockReader reader;
    tcx::nfc::FixedUrlTokenProvider tokenProvider("https://wstree.cn/t/MOCK001");

    PrintPlugin printPlugin;
    tcx::nfc::PluginHost plugins;
    plugins.add(&printPlugin);

    tcx::nfc::ActivationConfig config;
    config.deviceId = "dev-mac";
    config.readerId = "mock-reader";

    auto result = tcx::nfc::ActivationRuntime::runOnce(config, reader, tokenProvider, &plugins);
    if (!result.ok) {
        std::cerr << result.error << '\n';
        return 1;
    }

    std::cout << "activation result uid=" << result.value.uid
              << " urlMode=" << result.value.urlMode
              << " verification=" << result.value.verificationLevel << '\n';
    return 0;
}
