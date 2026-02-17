// =============================================================================
// iOS VideoGrabber stub implementation
// Full AVFoundation camera implementation planned for Phase 2
// =============================================================================

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS

#include "TrussC.h"

namespace trussc {

std::vector<VideoDeviceInfo> VideoGrabber::listDevicesPlatform() {
    logWarning() << "[VideoGrabber] Camera not yet supported on iOS";
    return {};
}

bool VideoGrabber::setupPlatform() {
    logWarning() << "[VideoGrabber] Camera not yet supported on iOS";
    return false;
}

void VideoGrabber::closePlatform() {
    // no-op
}

void VideoGrabber::updatePlatform() {
    // no-op
}

} // namespace trussc

#endif // TARGET_OS_IOS
#endif // __APPLE__
