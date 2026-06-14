#include "test_common.h"

#include <utility>

namespace {

class FakeReader final : public tcx::nfc::IReader {
public:
    tcx::nfc::Result<tcx::nfc::CardUid> readUid(uint8_t, uint8_t) override {
        tcx::nfc::CardUid uid;
        uid.cardType = 0x04;
        uid.uidBytes = {0x04, 0xA1, 0xB2, 0xC3};
        uid.uidHex = "04:A1:B2:C3";
        return tcx::nfc::Result<tcx::nfc::CardUid>::success(std::move(uid));
    }

    tcx::nfc::Result<tcx::nfc::WriteResult> writeUrlRawNtag(const std::string& url, int maxUserBytes, int startPage) override {
        lastUrl = url;
        lastMaxUserBytes = maxUserBytes;
        lastStartPage = startPage;

        tcx::nfc::WriteResult result;
        result.writeStrategy = "raw_0x62";
        result.verificationLevel = "verified";
        result.pagesWritten = 7;
        return tcx::nfc::Result<tcx::nfc::WriteResult>::success(std::move(result));
    }

    std::string lastUrl;
    int lastMaxUserBytes = 0;
    int lastStartPage = 0;
};

class FailingTokenProvider final : public tcx::nfc::ITokenProvider {
public:
    tcx::nfc::Result<tcx::nfc::PreparedToken> prepareToken(std::string_view) override {
        return tcx::nfc::Result<tcx::nfc::PreparedToken>::failure("cloud offline");
    }
};

class CapturePlugin final : public tcx::nfc::IActivationPlugin {
public:
    void onActivation(const tcx::nfc::ActivationEvent& event) override {
        called = true;
        captured = event;
    }

    bool called = false;
    tcx::nfc::ActivationEvent captured;
};

} // namespace

void test_activation_runtime() {
    FakeReader reader;
    FailingTokenProvider cloud;
    tcx::nfc::FixedUrlTokenProvider fallback("https://wstree.cn/t/FALLBACK");
    tcx::nfc::FallbackTokenProvider tokens(cloud, fallback);

    CapturePlugin plugin;
    tcx::nfc::PluginHost plugins;
    plugins.add(&plugin);

    tcx::nfc::ActivationConfig config;
    config.ntagMaxUserBytes = 144;
    config.ntagStartPage = 4;
    config.deviceId = "pi01";
    config.readerId = "bks710i-main";

    auto result = tcx::nfc::ActivationRuntime::runOnce(config, reader, tokens, &plugins);
    require(result.ok, result.error.c_str());
    require(result.value.uid == "04:A1:B2:C3", "activation should return card UID");
    require(result.value.url == "https://wstree.cn/t/FALLBACK", "activation should use fallback URL when cloud token provider fails");
    require(result.value.urlMode == "fixed", "activation should surface fallback URL mode");
    require(result.value.writeStrategy == "raw_0x62", "activation should write through raw Modbus 0x62 strategy");
    require(result.value.verificationLevel == "verified", "activation should surface readback verification result");
    require(reader.lastUrl == "https://wstree.cn/t/FALLBACK", "reader should receive resolved activation URL");
    require(reader.lastMaxUserBytes == 144, "reader should receive configured NTAG capacity");
    require(reader.lastStartPage == 4, "reader should receive configured NTAG start page");
    require(plugin.called, "plugin host should dispatch successful activation");
    require(plugin.captured.deviceId == "pi01", "activation event should include device id");
    require(plugin.captured.readerId == "bks710i-main", "activation event should include reader id");
    require(plugin.captured.uid == "04:A1:B2:C3", "activation event should include UID");
}
