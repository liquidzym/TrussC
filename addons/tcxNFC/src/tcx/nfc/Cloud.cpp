#include "tcx/nfc/Cloud.h"

#include <utility>

namespace tcx::nfc {

FixedUrlTokenProvider::FixedUrlTokenProvider(std::string fixedUrl)
    : fixedUrl_(std::move(fixedUrl)) {}

Result<PreparedToken> FixedUrlTokenProvider::prepareToken(std::string_view) {
    if (fixedUrl_.empty()) {
        return Result<PreparedToken>::failure("fixed fallback URL is empty");
    }

    PreparedToken token;
    token.url = fixedUrl_;
    token.urlMode = "fixed";
    return Result<PreparedToken>::success(std::move(token));
}

CloudTokenProvider::CloudTokenProvider(ICloudTokenClient& client, std::string deviceId, std::string readerId)
    : client_(client)
    , deviceId_(std::move(deviceId))
    , readerId_(std::move(readerId)) {}

Result<PreparedToken> CloudTokenProvider::prepareToken(std::string_view uid) {
    TokenRequest request;
    request.uid = std::string(uid);
    request.deviceId = deviceId_;
    request.readerId = readerId_;
    auto token = client_.prepare(request);
    if (!token.ok) {
        return token;
    }
    if (token.value.url.empty()) {
        return Result<PreparedToken>::failure("cloud token URL is empty");
    }
    if (token.value.urlMode.empty()) {
        token.value.urlMode = "cloud";
    }
    return token;
}

FallbackTokenProvider::FallbackTokenProvider(ITokenProvider& primary, ITokenProvider& fallback)
    : primary_(primary)
    , fallback_(fallback) {}

Result<PreparedToken> FallbackTokenProvider::prepareToken(std::string_view uid) {
    auto primary = primary_.prepareToken(uid);
    if (primary.ok) {
        return primary;
    }
    return fallback_.prepareToken(uid);
}

} // namespace tcx::nfc
