// =============================================================================
// tcxMapWrap — SourceVideo.cpp Implementation
// =============================================================================
// SourceVideo stores playback metadata and, when compiled inside a TrussC app,
// owns the TrussC VideoPlayer used by MapWrapRenderer. Multiple surfaces can
// share one SourceVideo ID without creating duplicate decoders.
// =============================================================================

#include "tcxMapWrap/SourceVideo.h"
#include "tcxMapWrap/MapWrapI18n.h"

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
#include <TrussC.h>
#endif

namespace tcx {
namespace mapwrap {

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
struct SourceVideo::Runtime {
    trussc::VideoPlayer player;
    std::string loadedPath;
    bool loaded = false;
    bool pausedForVisibility = false;
};
#else
struct SourceVideo::Runtime {};
#endif

SourceVideo::SourceVideo()
    : runtime_(std::make_unique<Runtime>())
{}

SourceVideo::~SourceVideo() = default;

// ---------------------------------------------------------------------------
// Source interface
// ---------------------------------------------------------------------------

SourceId SourceVideo::id() const {
    return id_;
}

std::string SourceVideo::name() const {
    return name_;
}

Vec2 SourceVideo::size() const {
    return size_;
}

// ---------------------------------------------------------------------------
// Path
// ---------------------------------------------------------------------------

void SourceVideo::setPath(const std::string& path) {
    if (path_ != path) {
        path_ = path;
        unload();
        // Reset state when a new path is assigned — the old error
        // and playback state are no longer relevant.
        hasError_ = false;
        error_.clear();
        playing_ = false;
        seekPosition_ = 0.0;
    }
}

const std::string& SourceVideo::path() const {
    return path_;
}

// ---------------------------------------------------------------------------
// Playback control — these store intent for the external VideoPlayer
// ---------------------------------------------------------------------------

void SourceVideo::play() {
    playing_ = true;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (ensureLoaded() && runtime_) {
        runtime_->pausedForVisibility = false;
        runtime_->player.play();
    }
#endif
}

void SourceVideo::pause() {
    playing_ = false;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_ && runtime_->loaded) {
        runtime_->pausedForVisibility = false;
        runtime_->player.setPaused(true);
    }
#endif
}

void SourceVideo::stop() {
    playing_ = false;
    seekPosition_ = 0.0;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_ && runtime_->loaded) {
        runtime_->pausedForVisibility = false;
        runtime_->player.stop();
    }
#endif
}

void SourceVideo::setLoop(bool loop) {
    loop_ = loop;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_ && runtime_->loaded) runtime_->player.setLoop(loop_);
#endif
}

void SourceVideo::seekSeconds(double seconds) {
    seekPosition_ = (seconds < 0.0) ? 0.0 : seconds;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_ && runtime_->loaded) {
        runtime_->player.setCurrentTime(static_cast<float>(seekPosition_));
    }
#endif
}

void SourceVideo::setVolume(float volume) {
    volume_ = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_ && runtime_->loaded) runtime_->player.setVolume(volume_);
#endif
}

void SourceVideo::setPlaybackMode(SourcePlaybackMode mode) {
    playbackMode_ = mode;
}

void SourceVideo::update(float dt) {
    (void)dt;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!ensureLoaded() || !runtime_ || !runtime_->loaded) return;

    if (playing_ && renderVisible_) {
        if (runtime_->player.isPaused()) {
            runtime_->player.setPaused(false);
        }
    }

    runtime_->player.update();
    if (runtime_->player.getWidth() > 0 && runtime_->player.getHeight() > 0) {
        size_ = Vec2(runtime_->player.getWidth(), runtime_->player.getHeight());
    }
#endif
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool SourceVideo::isPlaying() const {
    return playing_;
}

bool SourceVideo::isLoaded() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return runtime_ && runtime_->loaded;
#else
    return false;
#endif
}

bool SourceVideo::ensureLoaded() {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_) runtime_ = std::make_unique<Runtime>();
    if (path_.empty()) {
        setError("Video source path is empty");
        return false;
    }
    if (runtime_->loaded && runtime_->loadedPath == path_) {
        return true;
    }

    runtime_->player.close();
    runtime_->loaded = false;
    runtime_->loadedPath.clear();
    runtime_->pausedForVisibility = false;

    runtime_->player.setLoop(loop_);
    runtime_->player.setVolume(volume_);
    if (!runtime_->player.load(path_)) {
        setError("Failed to load video: " + path_);
        return false;
    }

    runtime_->player.setLoop(loop_);
    runtime_->player.setVolume(volume_);
    if (seekPosition_ > 0.0) {
        runtime_->player.setCurrentTime(static_cast<float>(seekPosition_));
    }
    if (playing_ && renderVisible_) {
        runtime_->player.play();
    }

    size_ = Vec2(runtime_->player.getWidth(), runtime_->player.getHeight());
    runtime_->loaded = true;
    runtime_->loadedPath = path_;
    clearError();
    return true;
#else
    setError("SourceVideo runtime requires TrussC");
    return false;
#endif
}

void SourceVideo::unload() {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (runtime_) {
        runtime_->player.close();
        runtime_->loaded = false;
        runtime_->loadedPath.clear();
        runtime_->pausedForVisibility = false;
    }
#endif
}

void SourceVideo::setRenderVisible(bool visible, bool pauseWhenHidden) {
    renderVisible_ = visible;
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_ || !runtime_->loaded) return;
    if (pauseWhenHidden && !visible) {
        if (playing_ && !runtime_->player.isPaused()) {
            runtime_->player.setPaused(true);
            runtime_->pausedForVisibility = true;
        }
    } else if (visible && runtime_->pausedForVisibility && playing_) {
        runtime_->player.setPaused(false);
        runtime_->pausedForVisibility = false;
    }
#else
    (void)pauseWhenHidden;
#endif
}

bool SourceVideo::renderVisible() const {
    return renderVisible_;
}

void* SourceVideo::textureHandle() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_ || !runtime_->loaded) return nullptr;
    return const_cast<trussc::Texture*>(&runtime_->player.getTexture());
#else
    return nullptr;
#endif
}

const unsigned char* SourceVideo::pixelsData() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    if (!runtime_ || !runtime_->loaded) return nullptr;
    return runtime_->player.getPixels();
#else
    return nullptr;
#endif
}

int SourceVideo::pixelsWidth() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? static_cast<int>(runtime_->player.getWidth()) : 0;
#else
    return 0;
#endif
}

int SourceVideo::pixelsHeight() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? static_cast<int>(runtime_->player.getHeight()) : 0;
#else
    return 0;
#endif
}

int SourceVideo::pixelsChannels() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded && runtime_->player.getPixels()) ? 4 : 0;
#else
    return 0;
#endif
}

double SourceVideo::durationSeconds() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? runtime_->player.getDuration() : 0.0;
#else
    return 0.0;
#endif
}

double SourceVideo::currentTimeSeconds() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return (runtime_ && runtime_->loaded) ? runtime_->player.getCurrentTime() : seekPosition_;
#else
    return seekPosition_;
#endif
}

bool SourceVideo::hasError() const {
    return hasError_;
}

std::string SourceVideo::error() const {
    return error_;
}

// ---------------------------------------------------------------------------
// Error state management — called by the external VideoPlayer
// ---------------------------------------------------------------------------

void SourceVideo::setError(const std::string& error) {
    hasError_ = true;
    error_ = error;
    playing_ = false;
}

void SourceVideo::clearError() {
    hasError_ = false;
    error_.clear();
}

// ---------------------------------------------------------------------------
// kindName
// ---------------------------------------------------------------------------

std::string SourceVideo::kindName() const {
    return MapWrapI18n::instance().tr("source.video");
}

} // namespace mapwrap
} // namespace tcx
