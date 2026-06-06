#pragma once

#include <functional>
#include <string>
#include <utility>

namespace tcx::ios {

enum class ErrorCode {
    None,
    Unavailable,
    NotImplemented,
    PermissionDenied,
    PermissionRestricted,
    Cancelled,
    Timeout,
    NativeError,
    InvalidState,
    InvalidArgument
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
    int nativeCode = 0;
    std::string nativeDomain;
};

template <typename T>
struct Result {
    bool ok = false;
    T value{};
    Error error;

    static Result success(T v) {
        Result r;
        r.ok = true;
        r.value = std::move(v);
        return r;
    }

    static Result failure(Error e) {
        Result r;
        r.ok = false;
        r.error = std::move(e);
        return r;
    }
};

template <>
struct Result<void> {
    bool ok = false;
    Error error;

    static Result success() {
        Result r;
        r.ok = true;
        return r;
    }

    static Result failure(Error e) {
        Result r;
        r.ok = false;
        r.error = std::move(e);
        return r;
    }
};

template <typename T>
using Completion = std::function<void(Result<T>)>;

Error unavailableError(const std::string& feature);
Error notImplementedError(const std::string& feature);
std::string toString(ErrorCode code);

} // namespace tcx::ios
