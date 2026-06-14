#pragma once

#include "tcx/nfc/Common.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tcx::nfc {

struct NdefUriBuild {
    std::vector<uint8_t> tlvBytes;
    std::vector<uint8_t> paddedBytes;
    int pageCount = 0;
};

class NdefUriBuilder {
public:
    static Result<NdefUriBuild> buildHttpsUriTlv(std::string_view url, size_t maxUserBytes);
};

} // namespace tcx::nfc
