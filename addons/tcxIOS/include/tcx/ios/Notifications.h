#pragma once

#include "Types.h"

#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

enum class NotificationSettingState {
    NotSupported,
    Disabled,
    Enabled
};

struct NotificationSettings {
    PermissionState authorizationStatus = PermissionState::Unknown;
    NotificationSettingState alert = NotificationSettingState::NotSupported;
    NotificationSettingState sound = NotificationSettingState::NotSupported;
    NotificationSettingState badge = NotificationSettingState::NotSupported;
    NotificationSettingState notificationCenter = NotificationSettingState::NotSupported;
    NotificationSettingState lockScreen = NotificationSettingState::NotSupported;
    NotificationSettingState carPlay = NotificationSettingState::NotSupported;
    NotificationSettingState criticalAlert = NotificationSettingState::NotSupported;
    NotificationSettingState timeSensitive = NotificationSettingState::NotSupported;
    bool providesAppNotificationSettings = false;
    bool scheduledDeliveryEnabled = false;
};

struct LocalNotificationRequest {
    std::string identifier;
    std::string title;
    std::string body;
    double delaySeconds = 1.0;
    bool repeats = false;
    std::string categoryIdentifier;
};

struct NotificationAction {
    std::string identifier;
    std::string title;
    bool foreground = false;
    bool destructive = false;
};

struct NotificationCategory {
    std::string identifier;
    std::vector<NotificationAction> actions;
    bool hiddenPreviewsShowTitle = false;
};

struct NotificationResponse {
    std::string notificationIdentifier;
    std::string actionIdentifier;
    std::string categoryIdentifier;
    std::string title;
    std::string body;
};

using NotificationResponseHandler = std::function<void(const NotificationResponse&)>;

class Notifications {
public:
    void settings(Completion<NotificationSettings> done);
    void setCategories(const std::vector<NotificationCategory>& categories, Completion<void> done);
    void schedule(const LocalNotificationRequest& request, Completion<std::string> done);
    void cancel(const std::string& identifier);
    void cancelAll();
    void setResponseHandler(NotificationResponseHandler handler);
    void clearResponseHandler();
};

Notifications& notifications();

std::string toString(NotificationSettingState state);

} // namespace tcx::ios
