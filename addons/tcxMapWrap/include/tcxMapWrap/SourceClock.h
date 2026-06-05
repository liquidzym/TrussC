#pragma once
// =============================================================================
// tcxMapWrap — SourceClock
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SourceClock {
public:
    void play();
    void pause();
    void stop();
    void seekSeconds(double seconds);

    /// Advance the clock by dt seconds (respects speed and playing state).
    /// Call this each frame from your update loop.
    void update(float dt);

    bool isPlaying() const;
    double timeSeconds() const;
    double speed() const;
    void setSpeed(double speed);

private:
    bool playing_ = false;
    double time_ = 0.0;
    double speed_ = 1.0;
};

} // namespace mapwrap
} // namespace tcx
