#pragma once

#include "Types.h"

#include <functional>
#include <string>

namespace tcx::ios {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogRecord {
    LogLevel level = LogLevel::Info;
    std::string subsystem;
    std::string message;
    Error error;
    std::string nativeDomain;
    int nativeCode = 0;
};

using LogHandler = std::function<void(const LogRecord&)>;

class Logger {
public:
    void setHandler(LogHandler handler);
    void clearHandler();
    void log(LogRecord record);
    void debug(const std::string& subsystem, const std::string& message);
    void info(const std::string& subsystem, const std::string& message);
    void warning(const std::string& subsystem, const std::string& message);
    void error(const std::string& subsystem, const std::string& message, Error nativeError = {});
};

Logger& logger();

std::string toString(LogLevel level);

} // namespace tcx::ios
