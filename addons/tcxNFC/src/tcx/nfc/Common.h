#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tcx::nfc {

template <typename T>
struct Result {
    bool ok = false;
    T value {};
    std::string error;

    static Result success(T v) {
        Result result;
        result.ok = true;
        result.value = std::move(v);
        return result;
    }

    static Result failure(std::string message) {
        Result result;
        result.ok = false;
        result.error = std::move(message);
        return result;
    }
};

struct CardUid {
    uint8_t cardType = 0;
    std::vector<uint8_t> uidBytes;
    std::string uidHex;
};

std::string formatUidHex(const std::vector<uint8_t>& uidBytes);

} // namespace tcx::nfc
