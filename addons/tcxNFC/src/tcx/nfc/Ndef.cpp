#include "tcx/nfc/Ndef.h"

#include <string>

namespace tcx::nfc {
namespace {

constexpr std::string_view kHttpsPrefix = "https://";

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

} // namespace

Result<NdefUriBuild> NdefUriBuilder::buildHttpsUriTlv(std::string_view url, size_t maxUserBytes) {
    if (!startsWith(url, kHttpsPrefix)) {
        return Result<NdefUriBuild>::failure("NDEF URI must start with https://");
    }

    const std::string suffix(url.substr(kHttpsPrefix.size()));
    if (suffix.empty()) {
        return Result<NdefUriBuild>::failure("NDEF URI suffix is empty");
    }

    const size_t payloadLength = 1 + suffix.size();
    const size_t messageLength = 4 + payloadLength;
    if (messageLength > 255) {
        return Result<NdefUriBuild>::failure("NDEF URI is too long for short record encoding");
    }

    NdefUriBuild build;
    build.tlvBytes.reserve(2 + messageLength + 1);
    build.tlvBytes.push_back(0x03);
    build.tlvBytes.push_back(static_cast<uint8_t>(messageLength));
    build.tlvBytes.push_back(0xD1);
    build.tlvBytes.push_back(0x01);
    build.tlvBytes.push_back(static_cast<uint8_t>(payloadLength));
    build.tlvBytes.push_back(0x55);
    build.tlvBytes.push_back(0x04);
    build.tlvBytes.insert(build.tlvBytes.end(), suffix.begin(), suffix.end());
    build.tlvBytes.push_back(0xFE);

    if (build.tlvBytes.size() > maxUserBytes) {
        return Result<NdefUriBuild>::failure("NDEF URI exceeds configured NTAG user capacity");
    }

    build.paddedBytes = build.tlvBytes;
    while (build.paddedBytes.size() % 4 != 0) {
        build.paddedBytes.push_back(0x00);
    }
    build.pageCount = static_cast<int>(build.paddedBytes.size() / 4);

    return Result<NdefUriBuild>::success(std::move(build));
}

} // namespace tcx::nfc
