// =============================================================================
// tcxMapWrap — SourceImage.cpp Implementation
// =============================================================================
// SourceImage stores a file path and, when compiled inside a TrussC app,
// owns the TrussC Image/Texture runtime used by MapWrapRenderer.
// =============================================================================

#include "tcxMapWrap/SourceImage.h"
#include "tcxMapWrap/MapWrapI18n.h"

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
#include <TrussC.h>
#endif

namespace tcx {
namespace mapwrap {

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
struct SourceImage::Runtime {
    trussc::Image image;
    std::string loadedPath;
    bool loaded = false;
};
#else
struct SourceImage::Runtime {};
#endif

SourceImage::SourceImage()
    : runtime_(std::make_unique<Runtime>())
{}

SourceImage::~SourceImage() = default;

// ---------------------------------------------------------------------------
// Source interface
// ---------------------------------------------------------------------------

SourceId SourceImage::id() const {
    return id_;
}

std::string SourceImage::name() const {
    return name_;
}

Vec2 SourceImage::size() const {
    return size_;
}

// ---------------------------------------------------------------------------
// Path
// ---------------------------------------------------------------------------

void SourceImage::setPath(const std::string& path) {
    if (path_ != path) {
        path_ = path;
        unload();
        // Reset error state when a new path is assigned
        hasError_ = false;
        error_.clear();
    }
}

const std::string& SourceImage::path() const {
    return path_;
}

// ---------------------------------------------------------------------------
// Error state
// ---------------------------------------------------------------------------

bool SourceImage::hasError() const {
    return hasError_;
}

std::string SourceImage::error() const {
    return error_;
}

void SourceImage::setError(const std::string& error) {
    hasError_ = true;
    error_ = error;
}

void SourceImage::clearError() {
    hasError_ = false;
    error_.clear();
}

bool SourceImage::ensureLoaded() {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_) runtime_ = std::make_unique<Runtime>();
    if (path_.empty()) {
        setError("Image source path is empty");
        return false;
    }
    if (runtime_->loaded && runtime_->loadedPath == path_) {
        return true;
    }

    runtime_->image.clear();
    runtime_->loaded = false;
    runtime_->loadedPath.clear();

    if (!runtime_->image.load(path_)) {
        setError("Failed to load image: " + path_);
        return false;
    }

    size_ = Vec2(static_cast<float>(runtime_->image.getWidth()),
                 static_cast<float>(runtime_->image.getHeight()));
    runtime_->loaded = true;
    runtime_->loadedPath = path_;
    clearError();
    return true;
#else
    setError("SourceImage runtime requires TrussC");
    return false;
#endif
}

bool SourceImage::isLoaded() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return runtime_ && runtime_->loaded;
#else
    return false;
#endif
}

void SourceImage::unload() {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_) {
        runtime_->image.clear();
        runtime_->loaded = false;
        runtime_->loadedPath.clear();
    }
#endif
}

void* SourceImage::textureHandle() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_ || !runtime_->loaded) return nullptr;
    return const_cast<trussc::Texture*>(&runtime_->image.getTexture());
#else
    return nullptr;
#endif
}

const unsigned char* SourceImage::pixelsData() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_ || !runtime_->loaded) return nullptr;
    return runtime_->image.getPixelsData();
#else
    return nullptr;
#endif
}

int SourceImage::pixelsWidth() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? runtime_->image.getWidth() : 0;
#else
    return 0;
#endif
}

int SourceImage::pixelsHeight() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? runtime_->image.getHeight() : 0;
#else
    return 0;
#endif
}

int SourceImage::pixelsChannels() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? runtime_->image.getChannels() : 0;
#else
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// kindName
// ---------------------------------------------------------------------------

std::string SourceImage::kindName() const {
    return MapWrapI18n::instance().tr("source.image");
}

} // namespace mapwrap
} // namespace tcx
