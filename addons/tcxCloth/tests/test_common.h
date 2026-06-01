#pragma once

#include <tcxCloth.h>

#include <cmath>
#include <stdexcept>

inline void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void requireNear(float a, float b, float eps, const char* message) {
    if (std::fabs(a - b) > eps) {
        throw std::runtime_error(message);
    }
}

inline bool finite(const tc::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
