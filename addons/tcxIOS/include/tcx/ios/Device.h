#pragma once

#include "App.h"

namespace tcx::ios {

inline DeviceInfo deviceInfo() {
    return app().deviceInfo();
}

inline ScreenInfo mainScreen() {
    return app().mainScreen();
}

inline SafeAreaInsets safeAreaInsets() {
    return app().safeAreaInsets();
}

inline Orientation orientation() {
    return app().orientation();
}

} // namespace tcx::ios
