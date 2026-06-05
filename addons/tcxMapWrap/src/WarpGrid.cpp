// =============================================================================
// tcxMapWrap — WarpGrid.cpp Implementation
// =============================================================================

#include "tcxMapWrap/WarpGrid.h"

namespace tcx {
namespace mapwrap {

// ===========================================================================
// bilinearInterpolate
// ===========================================================================
Vec2 bilinearInterpolate(Vec2 p00, Vec2 p10, Vec2 p01, Vec2 p11, float u, float v) {
    float invU = 1.0f - u;
    float invV = 1.0f - v;
    float x = invU * invV * p00.x + u * invV * p10.x + invU * v * p01.x + u * v * p11.x;
    float y = invU * invV * p00.y + u * invV * p10.y + invU * v * p01.y + u * v * p11.y;
    return Vec2(x, y);
}

// ===========================================================================
// catmullRom
// ===========================================================================
Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    float x = 0.5f * (
        (2.0f * p1.x) +
        (-p0.x + p2.x) * t +
        (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
        (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3
    );

    float y = 0.5f * (
        (2.0f * p1.y) +
        (-p0.y + p2.y) * t +
        (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
        (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3
    );

    return Vec2(x, y);
}

// ===========================================================================
// WarpGrid
// ===========================================================================
void WarpGrid::reset() {
    // No mutable state beyond what's in the derived WarpGrid class
    // The base class has no persistent grid points (those belong to SurfaceGrid)
}

} // namespace mapwrap
} // namespace tcx
