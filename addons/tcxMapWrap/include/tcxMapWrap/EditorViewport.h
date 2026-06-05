#pragma once
// =============================================================================
// tcxMapWrap — EditorViewport
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class EditorViewport {
public:
    Vec2 viewSizePixels = Vec2(0, 0);
    Vec2 canvasSizePixels = Vec2(1920, 1080);
    Vec2 panPixels = Vec2(0, 0);
    float zoom = 1.0f;

    Vec2 screenToCanvasNorm(Vec2 screenPixels) const;
    Vec2 canvasNormToScreen(Vec2 canvasNorm) const;

    void reset();
    void fitCanvasToView(Vec2 viewPixels);
    void panBy(Vec2 deltaPixels);
    void zoomAt(Vec2 screenPixels, float scale);
};

} // namespace mapwrap
} // namespace tcx
