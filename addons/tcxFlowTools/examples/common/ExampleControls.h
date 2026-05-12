#pragma once

namespace tcx::flow::example {

constexpr float kHudScale = 1.08f;
constexpr float kHudLine = 28.0f;

inline bool keyIs(int key, char c) {
    if (key == static_cast<int>(c)) {
        return true;
    }
    if (c >= 'a' && c <= 'z') {
        return key == static_cast<int>(c - 'a' + 'A');
    }
    if (c >= 'A' && c <= 'Z') {
        return key == static_cast<int>(c - 'A' + 'a');
    }
    return false;
}

inline int digitKey(int key) {
    if (key >= static_cast<int>('0') && key <= static_cast<int>('9')) {
        return key - static_cast<int>('0');
    }
    return -1;
}

} // namespace tcx::flow::example
