#pragma once

#include "Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tcx::ios {

struct AlertRequest {
    std::string title;
    std::string message;
    std::vector<std::string> buttons = {"OK"};
    int cancelButtonIndex = -1;
};

struct AlertResult {
    int buttonIndex = -1;
};

struct ShareRequest {
    std::vector<std::filesystem::path> files;
    std::vector<std::string> texts;
    std::string subject;
    bool excludeAirDrop = false;
    bool excludePrint = false;
};

struct ShareResult {
    bool completed = false;
};

class NativeUI {
public:
    void showAlert(const AlertRequest& request, Completion<AlertResult> done);
    void share(const ShareRequest& request, Completion<ShareResult> done);
    void openSettings();
    void openURL(const std::string& url, Completion<void> done);
};

NativeUI& nativeUI();

} // namespace tcx::ios
