#pragma once

#include <tcxNFC.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void requireEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* message) {
    if (actual != expected) {
        throw std::runtime_error(message);
    }
}

inline void requireEqual(const std::vector<uint16_t>& actual, const std::vector<uint16_t>& expected, const char* message) {
    if (actual != expected) {
        throw std::runtime_error(message);
    }
}

void test_ndef_https_uri_tlv();
void test_bks710i_frames();
void test_activation_runtime();
