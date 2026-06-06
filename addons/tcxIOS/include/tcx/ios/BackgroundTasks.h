#pragma once

#include "Types.h"

#include <functional>
#include <string>

namespace tcx::ios {

enum class BackgroundTaskKind {
    AppRefresh,
    Processing
};

struct BackgroundTaskRegistration {
    std::string identifier;
    BackgroundTaskKind kind = BackgroundTaskKind::AppRefresh;
};

struct BackgroundTaskRequest {
    std::string identifier;
    BackgroundTaskKind kind = BackgroundTaskKind::AppRefresh;
    double earliestBeginSecondsFromNow = 0.0;
    bool requiresNetworkConnectivity = false;
    bool requiresExternalPower = false;
};

struct BackgroundTaskContext {
    std::string identifier;
    BackgroundTaskKind kind = BackgroundTaskKind::AppRefresh;
    bool expired = false;
    std::string expirationReason;
};

using BackgroundTaskHandler = std::function<bool(const BackgroundTaskContext&)>;

class BackgroundTasks {
public:
    void registerHandler(const BackgroundTaskRegistration& registration,
                         BackgroundTaskHandler handler,
                         Completion<void> done);
    void schedule(const BackgroundTaskRequest& request, Completion<void> done);
    void cancel(const std::string& identifier);
    void cancelAll();
};

BackgroundTasks& backgroundTasks();

std::string toString(BackgroundTaskKind kind);

} // namespace tcx::ios
