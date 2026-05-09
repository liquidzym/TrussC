#pragma once
// Small vector helpers. These are written as simple contiguous loops so Clang/GCC
// can autovectorize them on SSE/AVX/NEON targets without adding platform-specific
// intrinsics to the addon API.

#include <algorithm>

namespace tcx::pdsp::simd {

inline void clear(float* dst, int frames) {
    std::fill(dst, dst + frames, 0.0f);
}

inline void copy(const float* src, float* dst, int frames) {
    std::copy_n(src, frames, dst);
}

inline void add(const float* src, float* dst, int frames) {
    for (int i = 0; i < frames; i++) dst[i] += src[i];
}

inline void multiply(float* dst, float gain, int frames) {
    for (int i = 0; i < frames; i++) dst[i] *= gain;
}

inline void multiplyAdd(const float* src, float gain, float* dst, int frames) {
    for (int i = 0; i < frames; i++) dst[i] += src[i] * gain;
}

} // namespace tcx::pdsp::simd
