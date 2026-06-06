#pragma once

#include "App.h"

#include <mutex>
#include <string>
#include <vector>

namespace tcx::ios {

struct SceneContext {
    std::string identifier;
    SafeAreaInsets safeArea;
    bool active = false;
};

class Scene {
public:
    void upsertContext(SceneContext context);
    void removeContext(const std::string& identifier);
    std::vector<SceneContext> contexts() const;
    void setActiveContext(SceneContext context);
    void setActiveIdentifier(const std::string& identifier);
    SceneContext activeContext() const;
    std::string activeIdentifier() const;
    bool hasActiveScene() const;

private:
    mutable std::mutex mutex_;
    std::vector<SceneContext> contexts_;
    SceneContext active_;
};

Scene& scene();

} // namespace tcx::ios
