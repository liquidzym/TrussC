#include "tcx/ios/Types.h"

namespace tcx::ios {

Error unavailableError(const std::string& feature) {
    return {
        ErrorCode::Unavailable,
        feature + " is only available on iOS/iPadOS in tcxIOS.",
        0
    };
}

Error notImplementedError(const std::string& feature) {
    return {
        ErrorCode::NotImplemented,
        feature + " is not implemented in this tcxIOS build.",
        0
    };
}

std::string toString(ErrorCode code) {
    switch (code) {
        case ErrorCode::None: return "none";
        case ErrorCode::Unavailable: return "unavailable";
        case ErrorCode::NotImplemented: return "not implemented";
        case ErrorCode::PermissionDenied: return "permission denied";
        case ErrorCode::PermissionRestricted: return "permission restricted";
        case ErrorCode::Cancelled: return "cancelled";
        case ErrorCode::Timeout: return "timeout";
        case ErrorCode::NativeError: return "native error";
        case ErrorCode::InvalidState: return "invalid state";
        case ErrorCode::InvalidArgument: return "invalid argument";
    }
    return "unknown";
}

} // namespace tcx::ios
