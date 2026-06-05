// =============================================================================
// tcxMapWrap — GeometryValidation.cpp Implementation
// =============================================================================
// Full geometry validation utilities: segment intersection, winding, NaN,
// polygon area (shoelace), and size checks.

#include "tcxMapWrap/GeometryValidation.h"

#include <cmath>
#include <algorithm>

namespace tcx {
namespace mapwrap {

namespace geometry {

// ---------------------------------------------------------------------------
// Internal: 2D cross product of vectors OA and OB
// ---------------------------------------------------------------------------
static float cross2d(Vec2 o, Vec2 a, Vec2 b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// ---------------------------------------------------------------------------
// Internal: Test if point C is on segment AB (given collinearity)
// ---------------------------------------------------------------------------
static bool onSegment(Vec2 a, Vec2 b, Vec2 c) {
    return c.x <= std::max(a.x, b.x) + 1e-10f &&
           c.x >= std::min(a.x, b.x) - 1e-10f &&
           c.y <= std::max(a.y, b.y) + 1e-10f &&
           c.y >= std::min(a.y, b.y) - 1e-10f;
}

// ---------------------------------------------------------------------------
// Internal: Segment-segment intersection test
// ---------------------------------------------------------------------------
// Returns true if segment (a1,a2) intersects segment (b1,b2), including
// proper crossings and touching endpoints.
static bool segmentsIntersect(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2) {
    float d1 = cross2d(b1, b2, a1);
    float d2 = cross2d(b1, b2, a2);
    float d3 = cross2d(a1, a2, b1);
    float d4 = cross2d(a1, a2, b2);

    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
        return true;
    }

    // Check collinear cases
    if (std::fabs(d1) < 1e-10f && onSegment(b1, b2, a1)) return true;
    if (std::fabs(d2) < 1e-10f && onSegment(b1, b2, a2)) return true;
    if (std::fabs(d3) < 1e-10f && onSegment(a1, a2, b1)) return true;
    if (std::fabs(d4) < 1e-10f && onSegment(a1, a2, b2)) return true;

    return false;
}

// ===========================================================================
// isSelfIntersecting
// ===========================================================================
bool isSelfIntersecting(const std::vector<Vec2>& polygon) {
    size_t n = polygon.size();
    if (n < 4) return false; // Triangle or less can't self-intersect

    // Test every pair of non-adjacent edges
    for (size_t i = 0; i < n; ++i) {
        size_t i2 = (i + 1) % n;
        for (size_t j = i + 2; j < n; ++j) {
            size_t j2 = (j + 1) % n;

            // Skip if edges share an endpoint (adjacent edges)
            if (i == j2 || i2 == j) continue;

            if (segmentsIntersect(polygon[i], polygon[i2],
                                  polygon[j], polygon[j2])) {
                return true;
            }
        }
    }
    return false;
}

// ===========================================================================
// isWindingCCW
// ===========================================================================
bool isWindingCCW(const std::vector<Vec2>& polygon) {
    return polygonArea(polygon) > 0.0f;
}

// ===========================================================================
// hasNaN
// ===========================================================================
bool hasNaN(const std::vector<Vec2>& points) {
    for (const auto& p : points) {
        if (std::isnan(p.x) || std::isnan(p.y)) return true;
    }
    return false;
}

// ===========================================================================
// polygonArea (Shoelace formula)
// ===========================================================================
float polygonArea(const std::vector<Vec2>& polygon) {
    float area = 0.0f;
    size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += polygon[i].x * polygon[j].y;
        area -= polygon[j].x * polygon[i].y;
    }
    return area * 0.5f;
}

// ===========================================================================
// isTooSmall
// ===========================================================================
bool isTooSmall(const std::vector<Vec2>& polygon, float minArea) {
    float a = polygonArea(polygon);
    float absArea = a < 0 ? -a : a;
    return absArea < minArea;
}

} // namespace geometry

} // namespace mapwrap
} // namespace tcx
