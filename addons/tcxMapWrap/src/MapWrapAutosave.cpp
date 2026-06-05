// =============================================================================
// tcxMapWrap — MapWrapAutosave.cpp Implementation
// =============================================================================

#include "tcxMapWrap/MapWrapAutosave.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapSerialization.h"
#include "tcxMapWrap/MapWrapI18n.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <chrono>

namespace tcx {
namespace mapwrap {

// ===========================================================================
// Impl
// ===========================================================================

struct MapWrapAutosave::Impl {
    MapWrapDocument* document = nullptr;
    AutosaveSettings settings;
    float elapsedSinceSave = 0.0f;
    int saveCount = 0;
    std::vector<std::string> recentPaths;

    /// Build the autosave file path for the current save count.
    std::string buildAutosavePath() const {
        if (!document) return "";

        std::string folder = settings.autosaveFolder;
        if (folder.empty()) {
            folder = ".autosave";
        }

        std::filesystem::path dir(folder);
        std::filesystem::create_directories(dir);

        std::string filename = document->name() + "_autosave_" +
            std::to_string(saveCount) + ".tcxm.json";
        return (dir / filename).string();
    }

    /// Prune old autosave files, keeping at most maxBackups.
    void pruneOldBackups() {
        if (recentPaths.size() > static_cast<size_t>(settings.maxBackups)) {
            // Remove oldest files from disk
            size_t toRemove = recentPaths.size() - static_cast<size_t>(settings.maxBackups);
            for (size_t i = 0; i < toRemove; ++i) {
                std::filesystem::remove(recentPaths[i]);
            }
            recentPaths.erase(recentPaths.begin(),
                              recentPaths.begin() + static_cast<ptrdiff_t>(toRemove));
        }
    }
};

// ===========================================================================
// Setup
// ===========================================================================

void MapWrapAutosave::setup(MapWrapDocument* document, AutosaveSettings settings) {
    impl_ = std::make_unique<Impl>();
    impl_->document = document;
    impl_->settings = std::move(settings);
}

MapWrapAutosave::MapWrapAutosave() = default;
MapWrapAutosave::~MapWrapAutosave() = default;

// ===========================================================================
// Update — called every frame; accumulates time and triggers autosave
// ===========================================================================

void MapWrapAutosave::update(float dt) {
    if (!impl_ || !impl_->document) return;
    if (!impl_->settings.enabled) return;
    if (!impl_->document->isDirty()) return;  // no unsaved changes

    impl_->elapsedSinceSave += dt;

    if (impl_->elapsedSinceSave >= impl_->settings.intervalSeconds) {
        forceSave();
        impl_->elapsedSinceSave = 0.0f;
    }
}

// ===========================================================================
// Force save
// ===========================================================================

Result MapWrapAutosave::forceSave() {
    if (!impl_ || !impl_->document) {
        return Result::error("Autosave not configured — call setup() first");
    }

    std::string path = impl_->buildAutosavePath();
    if (path.empty()) {
        return Result::error("Could not determine autosave path");
    }

    // Serialize the document to the autosave file
    Result saveResult = MapWrapSerialization::saveToFile(*impl_->document, path);
    if (!saveResult.ok) {
        return Result::error("Autosave failed: " + saveResult.message);
    }

    // Track the path for pruning
    impl_->recentPaths.push_back(path);
    impl_->saveCount++;

    // Clear the dirty flag since we just saved
    impl_->document->clearDirty();

    // Prune old backups
    impl_->pruneOldBackups();

    return Result::success();
}

// ===========================================================================
// List recoverable files
// ===========================================================================

std::vector<std::string> MapWrapAutosave::listRecoverableFiles() const {
    if (!impl_) return {};

    std::vector<std::string> result;

    // First, include any paths we've tracked in this session
    for (const auto& p : impl_->recentPaths) {
        if (std::filesystem::exists(p)) {
            result.push_back(p);
        }
    }

    // Also scan the autosave folder for any existing files
    std::string folder = impl_->settings.autosaveFolder;
    if (folder.empty()) {
        folder = ".autosave";
    }

    if (std::filesystem::exists(folder) && std::filesystem::is_directory(folder)) {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                // Accept .json and .tcxm.json files
                if (ext == ".json") {
                    std::string stem = entry.path().stem().string();
                    // Only include autosave files (contain "autosave" in name)
                    if (stem.find("autosave") != std::string::npos) {
                        std::string path = entry.path().string();
                        // Avoid duplicates
                        if (std::find(result.begin(), result.end(), path) == result.end()) {
                            result.push_back(std::move(path));
                        }
                    }
                }
            }
        }
    }

    // Sort by modification time (newest first)
    std::sort(result.begin(), result.end(),
        [](const std::string& a, const std::string& b) {
            auto ta = std::filesystem::last_write_time(a);
            auto tb = std::filesystem::last_write_time(b);
            return ta > tb;  // newest first
        });

    return result;
}

} // namespace mapwrap
} // namespace tcx
