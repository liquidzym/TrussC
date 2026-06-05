// =============================================================================
// tcxMapWrap — EditorViewport.cpp Implementation
// =============================================================================

#include "tcxMapWrap/EditorViewport.h"

namespace tcx {
namespace mapwrap {

Vec2 EditorViewport::screenToCanvasNorm(Vec2 screenPixels) const {
    Vec2 canvasPixels(
        canvasSizePixels.x * zoom,
        canvasSizePixels.y * zoom
    );
    float nx = (screenPixels.x - panPixels.x) / canvasPixels.x;
    float ny = (screenPixels.y - panPixels.y) / canvasPixels.y;
    return Vec2(nx, ny);
}

Vec2 EditorViewport::canvasNormToScreen(Vec2 canvasNorm) const {
    Vec2 canvasPixels(
        canvasSizePixels.x * zoom,
        canvasSizePixels.y * zoom
    );
    float sx = canvasNorm.x * canvasPixels.x + panPixels.x;
    float sy = canvasNorm.y * canvasPixels.y + panPixels.y;
    return Vec2(sx, sy);
}

void EditorViewport::reset() {
    panPixels = Vec2(0, 0);
    zoom = 1.0f;
}

void EditorViewport::fitCanvasToView(Vec2 viewPixels) {
    viewSizePixels = viewPixels;
    float scaleX = viewPixels.x / canvasSizePixels.x;
    float scaleY = viewPixels.y / canvasSizePixels.y;
    zoom = scaleX < scaleY ? scaleX : scaleY;
    panPixels = Vec2(
        (viewPixels.x - canvasSizePixels.x * zoom) * 0.5f,
        (viewPixels.y - canvasSizePixels.y * zoom) * 0.5f
    );
}

void EditorViewport::panBy(Vec2 deltaPixels) {
    panPixels = Vec2(panPixels.x + deltaPixels.x, panPixels.y + deltaPixels.y);
}

void EditorViewport::zoomAt(Vec2 screenPixels, float scale) {
    Vec2 before = screenToCanvasNorm(screenPixels);
    zoom *= scale;
    zoom = zoom < 0.1f ? 0.1f : (zoom > 10.0f ? 10.0f : zoom);
    Vec2 after = screenToCanvasNorm(screenPixels);
    panPixels = Vec2(
        panPixels.x + (after.x - before.x) * canvasSizePixels.x * zoom,
        panPixels.y + (after.y - before.y) * canvasSizePixels.y * zoom
    );
}

} // namespace mapwrap
} // namespace tcx
