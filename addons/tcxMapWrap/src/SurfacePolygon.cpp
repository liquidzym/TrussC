// =============================================================================
// tcxMapWrap — SurfacePolygon.cpp Implementation
// =============================================================================
// Arbitrary polygon surface with ear-clipping triangulation, point-in-polygon
// test, vertex/edge proximity, and self-intersection validation.

#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/GeometryValidation.h"

#include <cmath>
#include <algorithm>

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool isNanVec2(Vec2 p) { return std::isnan(p.x) || std::isnan(p.y); }

static float distVec2(Vec2 a, Vec2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

static float distToSegment(Vec2 p, Vec2 a, Vec2 b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-12f) {
        float ex = p.x - a.x, ey = p.y - a.y;
        return std::sqrt(ex * ex + ey * ey);
    }
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
    t = std::max(0.0f, std::min(1.0f, t));
    float projX = a.x + t * dx;
    float projY = a.y + t * dy;
    float ex = p.x - projX, ey = p.y - projY;
    return std::sqrt(ex * ex + ey * ey);
}

// ---------------------------------------------------------------------------
// Ear-clipping triangulation internals
// ---------------------------------------------------------------------------

/// Signed area (positive = CCW)
static float signedArea(const std::vector<Vec2>& poly) {
    float area = 0.0f;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += poly[i].x * poly[j].y;
        area -= poly[j].x * poly[i].y;
    }
    return area * 0.5f;
}

/// 2D cross product: (b-a) x (c-a)
static float cross2d(Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

/// Check if point p is inside triangle (a, b, c) — all same-winding assumed
static bool pointInTriangleTest(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    float d1 = cross2d(a, b, p);
    float d2 = cross2d(b, c, p);
    float d3 = cross2d(c, a, p);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

/// Test if any other polygon vertex lies inside triangle (a, b, c)
static bool anyVertexInTriangle(const std::vector<Vec2>& poly,
                                 const std::vector<bool>& removed,
                                 int a, int b, int c) {
    for (int i = 0; i < (int)poly.size(); ++i) {
        if (removed[i]) continue;
        if (i == a || i == b || i == c) continue;
        if (pointInTriangleTest(poly[i], poly[a], poly[b], poly[c]))
            return true;
    }
    return false;
}

/// Test if diagonal (a, c) is a valid ear tip, assuming CCW winding
static bool isEarTip(const std::vector<Vec2>& poly,
                      const std::vector<bool>& removed,
                      int prev, int curr, int next,
                      bool ccw) {
    float cross = cross2d(poly[prev], poly[curr], poly[next]);

    // For CCW polygon, ear tip has positive cross product
    // For CW polygon, ear tip has negative cross product
    if (ccw && cross <= 0) return false;
    if (!ccw && cross >= 0) return false;

    // Check that no other vertex lies inside the ear triangle
    return !anyVertexInTriangle(poly, removed, prev, curr, next);
}

// ===========================================================================
// SurfacePolygon
// ===========================================================================

SurfacePolygon::SurfacePolygon() { name_ = "Polygon"; }

std::vector<Vec2>& SurfacePolygon::destinationPoints() { return destPoints_; }
const std::vector<Vec2>& SurfacePolygon::destinationPoints() const { return destPoints_; }
std::vector<Vec2>& SurfacePolygon::uvPoints() { return uvPoints_; }
const std::vector<Vec2>& SurfacePolygon::uvPoints() const { return uvPoints_; }
bool SurfacePolygon::closed() const { return closed_; }
void SurfacePolygon::setClosed(bool c) { closed_ = c; markDirty(); }

void SurfacePolygon::addPoint(Vec2 p) {
    destPoints_.push_back(p);
    markDirty();
}

void SurfacePolygon::removePoint(size_t i) {
    if (i < destPoints_.size()) {
        destPoints_.erase(destPoints_.begin() + i);
        markDirty();
    }
}

void SurfacePolygon::movePoint(size_t i, Vec2 p) {
    if (i < destPoints_.size()) {
        destPoints_[i] = p;
        markDirty();
    }
}

// ===========================================================================
// triangulate — Ear-clipping algorithm for simple polygons
// ===========================================================================
bool SurfacePolygon::triangulate() {
    int n = (int)destPoints_.size();
    if (n < 3) return false;

    // Make a working copy (ensure CCW winding)
    std::vector<Vec2> poly = destPoints_;
    float area = signedArea(poly);
    bool ccw = area > 0;

    // If CW, reverse to CCW for the algorithm
    if (!ccw) {
        std::reverse(poly.begin(), poly.end());
    }

    std::vector<bool> removed(n, false);
    int remaining = n;
    uvPoints_.clear();

    // Generate UVs: simple normalized mapping
    // Find bounding box
    float minX = poly[0].x, maxX = poly[0].x;
    float minY = poly[0].y, maxY = poly[0].y;
    for (int i = 1; i < n; ++i) {
        minX = std::min(minX, poly[i].x); maxX = std::max(maxX, poly[i].x);
        minY = std::min(minY, poly[i].y); maxY = std::max(maxY, poly[i].y);
    }
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    if (rangeX < 1e-10f) rangeX = 1.0f;
    if (rangeY < 1e-10f) rangeY = 1.0f;

    for (int i = 0; i < n; ++i) {
        uvPoints_.push_back(Vec2(
            (poly[i].x - minX) / rangeX,
            (poly[i].y - minY) / rangeY
        ));
    }

    // Ear clipping
    // We'll track triangle indices into the original polygon
    // Build linked-list style prev/next
    std::vector<int> prevIdx(n), nextIdx(n);
    for (int i = 0; i < n; ++i) {
        prevIdx[i] = (i - 1 + n) % n;
        nextIdx[i] = (i + 1) % n;
    }

    // Safety: maximum iterations
    int maxIter = n * n;
    int iter = 0;

    // Store triangulated indices for buildMesh to use
    // We'll repurpose uvPoints_ as the working copy for UVs
    // and store triangulation result in a member we can access from buildMesh

    while (remaining > 3 && iter < maxIter) {
        bool earFound = false;
        for (int i = 0; i < n; ++i) {
            if (removed[i]) continue;

            int p = prevIdx[i];
            int nx = nextIdx[i];

            if (isEarTip(poly, removed, p, i, nx, true)) {
                removed[i] = true;
                remaining--;

                // Update linked list
                nextIdx[p] = nx;
                prevIdx[nx] = p;

                earFound = true;
                break;
            }
        }

        if (!earFound) break; // degenerate polygon
        iter++;
    }

    // The remaining 3 vertices form the last triangle
    return remaining >= 3;
}

// ===========================================================================
// buildMesh
// ===========================================================================
MeshBuildResult SurfacePolygon::buildMesh(const MeshBuildContext& ctx) {
    MeshBuildResult result;
    MeshData& mesh = result.mesh;

    int n = (int)destPoints_.size();
    if (n < 3) {
        result.ok = false;
        result.message = "Polygon needs at least 3 points";
        return result;
    }
    mesh.vertices.reserve(size_t(n) * 2);
    mesh.uvs.reserve(size_t(n) * 2);
    mesh.indices.reserve(size_t(std::max(0, n - 2)) * 3);

    // --- Ear-clipping triangulation ---
    std::vector<Vec2> poly = destPoints_;
    float area = signedArea(poly);
    bool ccw = area > 0;
    if (!ccw) {
        std::reverse(poly.begin(), poly.end());
    }

    // Find bounding box for UV generation
    float minX = poly[0].x, maxX = poly[0].x;
    float minY = poly[0].y, maxY = poly[0].y;
    for (int i = 1; i < n; ++i) {
        minX = std::min(minX, poly[i].x); maxX = std::max(maxX, poly[i].x);
        minY = std::min(minY, poly[i].y); maxY = std::max(maxY, poly[i].y);
    }
    float rangeX = maxX - minX; if (rangeX < 1e-10f) rangeX = 1.0f;
    float rangeY = maxY - minY; if (rangeY < 1e-10f) rangeY = 1.0f;

    // Add all vertices
    for (int i = 0; i < n; ++i) {
        float u = (poly[i].x - minX) / rangeX;
        float v = (poly[i].y - minY) / rangeY;
        mesh.addVertex(poly[i].x, poly[i].y, u, v);
    }

    // Linked-list ear clipping
    std::vector<bool> removed(n, false);
    std::vector<int> prevIdx(n), nextIdx(n);
    for (int i = 0; i < n; ++i) {
        prevIdx[i] = (i - 1 + n) % n;
        nextIdx[i] = (i + 1) % n;
    }

    int remaining = n;
    int maxIter = n * n;

    for (int iter = 0; iter < maxIter && remaining > 3; ++iter) {
        bool earFound = false;
        for (int i = 0; i < n; ++i) {
            if (removed[i]) continue;
            int p = prevIdx[i];
            int nx = nextIdx[i];

            if (isEarTip(poly, removed, p, i, nx, true)) {
                mesh.addTriangle(uint32_t(p), uint32_t(i), uint32_t(nx));
                removed[i] = true;
                remaining--;
                nextIdx[p] = nx;
                prevIdx[nx] = p;
                earFound = true;
                break;
            }
        }
        if (!earFound) break;
    }

    // Last triangle
    if (remaining == 3) {
        int v[3], vi = 0;
        for (int i = 0; i < n && vi < 3; ++i) {
            if (!removed[i]) v[vi++] = i;
        }
        if (vi == 3) {
            mesh.addTriangle(uint32_t(v[0]), uint32_t(v[1]), uint32_t(v[2]));
        }
    }

    result.ok = true;
    return result;
}

// ===========================================================================
// hitTest
// ===========================================================================
HitResult SurfacePolygon::hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = canvasNormPos;

    float radius = options.radiusPixels / 1000.0f;
    int n = (int)destPoints_.size();

    // --- Check vertex proximity ---
    for (int i = 0; i < n; ++i) {
        if (distVec2(canvasNormPos, destPoints_[i]) < radius) {
            hr.hit = true;
            hr.surfaceId = id_;
            hr.handleKind = HandleKind::Vertex;
            hr.handleIndex = i;
            return hr;
        }
    }

    // --- Check edge proximity ---
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        if (!closed_ && i == n - 1) break;
        if (distToSegment(canvasNormPos, destPoints_[i], destPoints_[j]) < radius) {
            hr.hit = true;
            hr.surfaceId = id_;
            hr.handleKind = HandleKind::Edge;
            hr.handleIndex = i;
            return hr;
        }
    }

    // --- Check body (point-in-polygon, ray casting) ---
    if (closed_ && n >= 3) {
        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            float yi = destPoints_[i].y, yj = destPoints_[j].y;
            float xi = destPoints_[i].x, xj = destPoints_[j].x;
            if (((yi > canvasNormPos.y) != (yj > canvasNormPos.y)) &&
                (canvasNormPos.x < (xj - xi) * (canvasNormPos.y - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        if (inside) {
            hr.hit = true;
            hr.surfaceId = id_;
            hr.handleKind = HandleKind::Body;
            return hr;
        }
    }

    return hr;
}

// ===========================================================================
// validateGeometry
// ===========================================================================
GeometryValidation SurfacePolygon::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    // NaN check
    for (int i = 0; i < (int)destPoints_.size(); ++i) {
        if (isNanVec2(destPoints_[i])) {
            v.valid = false;
            v.hasNaN = true;
            v.message = "Polygon has NaN coordinates at point " + std::to_string(i);
            return v;
        }
    }

    // Too few points
    if ((int)destPoints_.size() < 3) {
        v.valid = false;
        v.message = "Polygon needs at least 3 points";
        return v;
    }

    // Self-intersection
    if (geometry::isSelfIntersecting(destPoints_)) {
        v.valid = false;
        v.selfIntersecting = true;
        v.message = "Polygon is self-intersecting";
        return v;
    }

    // Area check
    if (geometry::isTooSmall(destPoints_)) {
        v.valid = false;
        v.tooSmall = true;
        v.message = "Polygon area too small";
        return v;
    }

    // Winding check
    if (!geometry::isWindingCCW(destPoints_)) {
        v.windingFlipped = true;
        v.message = "Polygon winding is CW (expected CCW)";
    }

    return v;
}

} // namespace mapwrap
} // namespace tcx
