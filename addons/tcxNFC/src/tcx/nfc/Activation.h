#pragma once

#include "tcx/nfc/Cloud.h"
#include "tcx/nfc/Common.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tcx::nfc {

struct WriteResult {
    std::string writeStrategy;
    std::string verificationLevel;
    int pagesWritten = 0;
};

class IReader {
public:
    virtual ~IReader() = default;
    virtual Result<CardUid> readUid(uint8_t beepLedHint = 0x22, uint8_t controlByte = 0x00) = 0;
    virtual Result<WriteResult> writeUrlRawNtag(const std::string& url, int maxUserBytes, int startPage) = 0;
};

struct ActivationConfig {
    int ntagMaxUserBytes = 144;
    int ntagStartPage = 4;
    std::string deviceId;
    std::string readerId;
};

struct ActivationResult {
    std::string uid;
    std::string token;
    std::string url;
    std::string urlMode;
    std::string writeStrategy;
    std::string verificationLevel;
    int pagesWritten = 0;
};

struct ActivationEvent {
    std::string deviceId;
    std::string readerId;
    std::string uid;
    std::string token;
    std::string url;
    std::string urlMode;
    std::string writeStrategy;
    std::string verificationLevel;
};

class IActivationPlugin {
public:
    virtual ~IActivationPlugin() = default;
    virtual void onActivation(const ActivationEvent& event) = 0;
};

class PluginHost {
public:
    void add(IActivationPlugin* plugin);
    void dispatch(const ActivationEvent& event) const;
    size_t size() const;

private:
    std::vector<IActivationPlugin*> plugins_;
};

class ActivationRuntime {
public:
    static Result<ActivationResult> runOnce(const ActivationConfig& config, IReader& reader, ITokenProvider& tokenProvider, PluginHost* plugins = nullptr);
};

} // namespace tcx::nfc
