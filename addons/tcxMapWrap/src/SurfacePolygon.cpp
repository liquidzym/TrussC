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

static std::vector<Vec2> defaultUvsForPoints(const std::vector<Vec2>& points) {
    std::vector<Vec2> uvs;
    uvs.reserve(points.size());
    if (points.empty()) return uvs;

    float minX = points[0].x;
    float maxX = points[0].x;
    float minY = points[0].y;
    float maxY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    if (rangeX < 1e-10f) rangeX = 1.0f;
    if (rangeY < 1e-10f) rangeY = 1.0f;

    for (const auto& p : points) {
        uvs.push_back(Vec2((p.x - minX) / rangeX,
                           (p.y - minY) / rangeY));
    }
    return uvs;
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

std::unique_ptr<Surface> SurfacePolygon::clone() const {
    return std::make_unique<SurfacePolygon>(*this);
}

std::vector<Vec2>& SurfacePolygon::destinationPoints() {
    markDirty();
    return destPoints_;
}
const std::vector<Vec2>& SurfacePolygon::destinationPoints() const { return destPoints_; }
void SurfacePolygon::setDestinationPoints(const std::vector<Vec2>& points) {
    destPoints_ = points;
    if (!customUv_ || uvPoints_.size() != destPoints_.size()) {
        uvPoints_ = defaultUvsForPoints(destPoints_);
        customUv_ = false;
    }
    triangles_.clear();
    markDirty();
}
std::vector<Vec2>& SurfacePolygon::uvPoints() {
    markDirty();
    return uvPoints_;
}
const std::vector<Vec2>& SurfacePolygon::uvPoints() const { return uvPoints_; }
void SurfacePolygon::setUvPoints(const std::vector<Vec2>& points) {
    uvPoints_ = points;
    if (uvPoints_.size() != destPoints_.size()) {
        std::vector<Vec2> defaults = defaultUvsForPoints(destPoints_);
        uvPoints_.resize(destPoints_.size());
        for (size_t i = points.size(); i < uvPoints_.size() && i < defaults.size(); ++i) {
            uvPoints_[i] = defaults[i];
        }
    }
    customUv_ = true;
    markDirty();
}
bool SurfacePolygon::closed() const { return closed_; }
void SurfacePolygon::setClosed(bool c) { closed_ = c; markDirty(); }

void SurfacePolygon::addPoint(Vec2 p) {
    destPoints_.push_back(p);
    if (customUv_) {
        std::vector<Vec2> defaults = defaultUvsForPoints(destPoints_);
        uvPoints_.push_back(defaults.empty() ? Vec2(0, 0) : defaults.back());
    } else {
        uvPoints_ = defaultUvsForPoints(destPoints_);
    }
    triangles_.clear();
    markDirty();
}

void SurfacePolygon::removePoint(size_t i) {
    if (i < destPoints_.size()) {
        destPoints_.erase(destPoints_.begin() + i);
        if (i < uvPoints_.size()) {
            uvPoints_.erase(uvPoints_.begin() + i);
        }
        if (!customUv_) {
            uvPoints_ = defaultUvsForPoints(destPoints_);
        }
        triangles_.clear();
        markDirty();
    }
}

void SurfacePolygon::movePoint(size_t i, Vec2 p) {
    if (i < destPoints_.size()) {
        destPoints_[i] = p;
        if (!customUv_) {
            uvPoints_ = defaultUvsForPoints(destPoints_);
        }
        triangles_.clear();
        markDirty();
    }
}

// ===========================================================================
// triangulate — Ear-clipping algorithm for simple polygons
// ===========================================================================
bool SurfacePolygon::triangulate() {
    int n = (int)destPoints_.size();
    if (n < 3) return false;

    triangles_.clear();
    std::vector<Vec2> poly = destPoints_;
    std::vector<uint32_t> originalIndices;
    originalIndices.reserve(size_t(n));
    for (int i = 0; i < n; ++i) originalIndices.push_back(uint32_t(i));

    float area = signedArea(poly);
    if (std::fabs(area) < 1e-10f) return false;
    if (area < 0.0f) {
        std::reverse(poly.begin(), poly.end());
        std::reverse(originalIndices.begin(), originalIndices.end());
    }

    std::vector<bool> removed(n, false);
    int remaining = n;
    std::vector<int> prevIdx(n), nextIdx(n);
    for (int i = 0; i < n; ++i) {
        prevIdx[i] = (i - 1 + n) % n;
        nextIdx[i] = (i + 1) % n;
    }

    int maxIter = n * n;
    int iter = 0;

    while (remaining > 3 && iter < maxIter) {
        bool earFound = false;
        for (int i = 0; i < n; ++i) {
            if (removed[i]) continue;

            int p = prevIdx[i];
            int nx = nextIdx[i];

            if (isEarTip(poly, removed, p, i, nx, true)) {
                triangles_.push_back(originalIndices[p]);
                triangles_.push_back(originalIndices[i]);
                triangles_.push_back(originalIndices[nx]);
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

    if (remaining == 3) {
        int v[3], vi = 0;
        for (int i = 0; i < n && vi < 3; ++i) {
            if (!removed[i]) v[vi++] = i;
        }
        if (vi == 3) {
            triangles_.push_back(originalIndices[v[0]]);
            triangles_.push_back(originalIndices[v[1]]);
            triangles_.push_back(originalIndices[v[2]]);
        }
    }

    return triangles_.size() == size_t(n - 2) * 3u;
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

    GeometryValidation validation = validateGeometry();
    if (!validation.valid) {
        result.ok = false;
        result.message = validation.message.empty() ? "Invalid polygon geometry" : validation.message;
        return result;
    }

    if (uvPoints_.size() != destPoints_.size()) {
        uvPoints_ = defaultUvsForPoints(destPoints_);
        customUv_ = false;
    }

    if (!triangulate()) {
        result.ok = false;
        result.message = "Polygon triangulation failed";
        return result;
    }

    int subdiv = std::max(1, ctx.meshSubdivision);
    if (subdiv <= 1) {
        mesh.vertices.reserve(size_t(n) * 2);
        mesh.uvs.reserve(size_t(n) * 2);
        mesh.indices.reserve(size_t(std::max(0, n - 2)) * 3);

        for (int i = 0; i < n; ++i) {
            mesh.addVertex(destPoints_[i].x, destPoints_[i].y,
                           uvPoints_[i].x, uvPoints_[i].y);
        }

        for (size_t i = 0; i + 2 < triangles_.size(); i += 3) {
            if (!mesh.addTriangle(triangles_[i], triangles_[i + 1], triangles_[i + 2])) {
                result.ok = false;
                result.message = "Polygon triangulation produced out-of-range index";
                return result;
            }
        }
    } else {
        size_t sourceTriangleCount = triangles_.size() / 3;
        size_t verticesPerTriangle = size_t(subdiv + 1) * size_t(subdiv + 2) / 2;
        mesh.vertices.reserve(sourceTriangleCount * verticesPerTriangle * 2);
        mesh.uvs.reserve(sourceTriangleCount * verticesPerTriangle * 2);
        mesh.indices.reserve(sourceTriangleCount * size_t(subdiv) * size_t(subdiv) * 3);

        for (size_t ti = 0; ti + 2 < triangles_.size(); ti += 3) {
            uint32_t ia = triangles_[ti];
            uint32_t ib = triangles_[ti + 1];
            uint32_t ic = triangles_[ti + 2];
            if (ia >= destPoints_.size() || ib >= destPoints_.size() || ic >= destPoints_.size()) {
                result.ok = false;
                result.message = "Polygon triangulation produced out-of-range index";
                return result;
            }

            Vec2 pa = destPoints_[ia];
            Vec2 pb = destPoints_[ib];
            Vec2 pc = destPoints_[ic];
            Vec2 ua = uvPoints_[ia];
            Vec2 ub = uvPoints_[ib];
            Vec2 uc = uvPoints_[ic];

            std::vector<int> vertIndex((subdiv + 1) * (subdiv + 1), -1);
            int localVertex = 0;
            uint32_t base = static_cast<uint32_t>(mesh.vertexCount());
            for (int i = 0; i <= subdiv; ++i) {
                for (int j = 0; j <= subdiv - i; ++j) {
                    int k = subdiv - i - j;
                    float wa = float(i) / float(subdiv);
                    float wb = float(j) / float(subdiv);
                    float wc = float(k) / float(subdiv);

                    Vec2 p(wa * pa.x + wb * pb.x + wc * pc.x,
                           wa * pa.y + wb * pb.y + wc * pc.y);
                    Vec2 uv(wa * ua.x + wb * ub.x + wc * uc.x,
                            wa * ua.y + wb * ub.y + wc * uc.y);
                    mesh.addVertex(p.x, p.y, uv.x, uv.y);
                    vertIndex[i * (subdiv + 1) + j] = int(base) + localVertex++;
                }
            }

            for (int i = 0; i <= subdiv; ++i) {
                for (int j = 0; j <= subdiv - i - 1; ++j) {
                    int idx00 = vertIndex[i * (subdiv + 1) + j];
                    int idx10 = vertIndex[(i + 1) * (subdiv + 1) + j];
                    int idx01 = vertIndex[i * (subdiv + 1) + (j + 1)];

                    if (idx00 >= 0 && idx10 >= 0 && idx01 >= 0) {
                        mesh.addTriangle(uint32_t(idx00), uint32_t(idx10), uint32_t(idx01));
                    }

                    if (i + j + 2 <= subdiv) {
                        int idx11 = vertIndex[(i + 1) * (subdiv + 1) + (j + 1)];
                        if (idx10 >= 0 && idx11 >= 0 && idx01 >= 0) {
                            mesh.addTriangle(uint32_t(idx10), uint32_t(idx11), uint32_t(idx01));
                        }
                    }
                }
            }
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
