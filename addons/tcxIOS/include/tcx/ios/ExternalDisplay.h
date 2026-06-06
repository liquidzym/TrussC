#pragma once

#include "Types.h"

#include <functional>
#include <string>
#include <vector>

namespace tcx::ios {

struct ExternalScreenInfo {
    std::string identifier;
    int pixelWidth = 0;
    int pixelHeight = 0;
    float scale = 1.0f;
    int maximumFramesPerSecond = 0;
};

struct ExternalDisplayRequest {
    std::string screenIdentifier;
    std::string title = "tcxIOS External Display";
};

struct ExternalDisplayPresentation {
    std::string screenIdentifier;
    bool visible = false;
};

using ExternalDisplayChangeHandler = std::function<void(const std::vector<ExternalScreenInfo>&)>;

class ExternalDisplay {
public:
    std::vector<ExternalScreenInfo> screens() const;
    bool hasExternalScreen() const;
    void setChangeHandler(ExternalDisplayChangeHandler handler);
    void clearChangeHandler();
    void show(const ExternalDisplayRequest& request, Completion<ExternalDisplayPresentation> done);
    void dismiss(const std::string& screenIdentifier);
    void dismissAll();
};

ExternalDisplay& externalDisplay();

} // namespace tcx::ios
