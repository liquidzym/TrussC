#pragma once

#include "tcx/nfc/Common.h"

#include <string>
#include <string_view>

namespace tcx::nfc {

struct TokenRequest {
    std::string uid;
    std::string deviceId;
    std::string readerId;
};

struct PreparedToken {
    std::string token;
    std::string url;
    std::string urlMode;
};

class ITokenProvider {
public:
    virtual ~ITokenProvider() = default;
    virtual Result<PreparedToken> prepareToken(std::string_view uid) = 0;
};

class ICloudTokenClient {
public:
    virtual ~ICloudTokenClient() = default;
    virtual Result<PreparedToken> prepare(const TokenRequest& request) = 0;
};

class FixedUrlTokenProvider final : public ITokenProvider {
public:
    explicit FixedUrlTokenProvider(std::string fixedUrl);

    Result<PreparedToken> prepareToken(std::string_view uid) override;

private:
    std::string fixedUrl_;
};

class CloudTokenProvider final : public ITokenProvider {
public:
    CloudTokenProvider(ICloudTokenClient& client, std::string deviceId, std::string readerId);

    Result<PreparedToken> prepareToken(std::string_view uid) override;

private:
    ICloudTokenClient& client_;
    std::string deviceId_;
    std::string readerId_;
};

class FallbackTokenProvider final : public ITokenProvider {
public:
    FallbackTokenProvider(ITokenProvider& primary, ITokenProvider& fallback);

    Result<PreparedToken> prepareToken(std::string_view uid) override;

private:
    ITokenProvider& primary_;
    ITokenProvider& fallback_;
};

} // namespace tcx::nfc
