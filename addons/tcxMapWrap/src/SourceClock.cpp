// =============================================================================
// tcxMapWrap — SourceClock.cpp Implementation
// =============================================================================
// Simple time accumulator with play/pause/stop/seek/speed control.
// Used by SourceRegistry as the global clock and can be used per-source
// for independent timing.
// =============================================================================

#include "tcxMapWrap/SourceClock.h"

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

void SourceClock::play() {
    playing_ = true;
}

void SourceClock::pause() {
    playing_ = false;
}

void SourceClock::stop() {
    playing_ = false;
    time_ = 0.0;
}

void SourceClock::seekSeconds(double seconds) {
    time_ = (seconds < 0.0) ? 0.0 : seconds;
}

// ---------------------------------------------------------------------------
// Update — advance time accumulator
// ---------------------------------------------------------------------------

void SourceClock::update(float dt) {
    if (playing_ && dt > 0.0f) {
        time_ += static_cast<double>(dt) * speed_;
        if (time_ < 0.0) {
            time_ = 0.0;
        }
    }
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool SourceClock::isPlaying() const {
    return playing_;
}

double SourceClock::timeSeconds() const {
    return time_;
}

double SourceClock::speed() const {
    return speed_;
}

void SourceClock::setSpeed(double speed) {
    speed_ = speed;
}

} // namespace mapwrap
} // namespace tcx
