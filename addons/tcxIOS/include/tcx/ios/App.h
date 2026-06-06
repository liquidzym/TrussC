#pragma once

#include "Types.h"

#include <functional>
#include <string>

namespace tcx::ios {

struct SafeAreaInsets {
    float top = 0.0f;
    float left = 0.0f;
    float bottom = 0.0f;
    float right = 0.0f;
};

struct ScreenInfo {
    int pixelWidth = 0;
    int pixelHeight = 0;
    float scale = 1.0f;
    int maximumFramesPerSecond = 60;
};

enum class AppState {
    Inactive,
    Active,
    Background
};

enum class Orientation {
    Unknown,
    Portrait,
    PortraitUpsideDown,
    LandscapeLeft,
    LandscapeRight
};

struct DeviceInfo {
    std::string model;
    std::string systemName;
    std::string systemVersion;
    std::string preferredLanguage;
    bool lowPowerModeEnabled = false;
};

class App {
public:
    AppState state() const;
    ScreenInfo mainScreen() const;
    SafeAreaInsets safeAreaInsets() const;
    Orientation orientation() const;
    DeviceInfo deviceInfo() const;

    std::function<void(AppState)> onStateChanged;
    std::function<void(ScreenInfo)> onScreenChanged;
    std::function<void(SafeAreaInsets)> onSafeAreaChanged;
    std::function<void(Orientation)> onOrientationChanged;
};

App& app();

std::string toString(AppState state);
std::string toString(Orientation orientation);

} // namespace tcx::ios
