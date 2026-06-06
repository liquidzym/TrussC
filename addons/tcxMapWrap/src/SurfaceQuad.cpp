// =============================================================================
// tcxMapWrap — SurfaceQuad.cpp Implementation
// =============================================================================
// Subdivided quad mesh with perspective-correct (homography) or bilinear
// interpolation. Hit-testing with point-in-polygon, vertex/edge proximity.

#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/WarpPerspective.h"
#include "tcxMapWrap/GeometryValidation.h"

#include <cmath>
#include <algorithm>

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool isNanVec2(Vec2 p) { return std::isnan(p.x) || std::isnan(p.y); }

/// Apply homography to a 2D point (assumes row-major Mat3)
static Vec2 applyHomography(const Mat3& H, Vec2 p) {
    float w = H.m[6] * p.x + H.m[7] * p.y + H.m[8];
    if (std::fabs(w) < 1e-12f) return Vec2(0, 0);
    float x = (H.m[0] * p.x + H.m[1] * p.y + H.m[2]) / w;
    float y = (H.m[3] * p.x + H.m[4] * p.y + H.m[5]) / w;
    return Vec2(x, y);
}

/// Signed area of quadrilateral (using shoelace on the 4 points)
static float quadSignedArea(const std::array<Vec2, 4>& pts) {
    float area = 0.0f;
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    return area * 0.5f;
}

/// Cross product of vectors (b-a) and (c-a)
static float cross2d(Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

/// Point-in-convex-polygon test (works for convex quads)
static bool pointInConvexQuad(const std::array<Vec2, 4>& q, Vec2 p) {
    // For a convex quad with consistent winding, all cross products
    // should have the same sign
    float c0 = cross2d(q[0], q[1], p);
    float c1 = cross2d(q[1], q[2], p);
    float c2 = cross2d(q[2], q[3], p);
    float c3 = cross2d(q[3], q[0], p);

    bool allPos = (c0 >= 0) && (c1 >= 0) && (c2 >= 0) && (c3 >= 0);
    bool allNeg = (c0 <= 0) && (c1 <= 0) && (c2 <= 0) && (c3 <= 0);
    return allPos || allNeg;
}

/// Distance from point p to line segment (a, b)
static float distToSegment(Vec2 p, Vec2 a, Vec2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
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

/// Distance between two points in normalized coords
static float distanceVec2(Vec2 a, Vec2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ===========================================================================
// SurfaceQuad
// ===========================================================================

SurfaceQuad::SurfaceQuad() { name_ = "Quad"; }

std::unique_ptr<Surface> SurfaceQuad::clone() const {
    return std::make_unique<SurfaceQuad>(*this);
}

std::array<Vec2, 4>& SurfaceQuad::destinationPoints() {
    markDirty();
    return dest_;
}
const std::array<Vec2, 4>& SurfaceQuad::destinationPoints() const { return dest_; }
void SurfaceQuad::setDestinationPoint(int index, Vec2 pos) {
    if (index < 0 || index >= 4) return;
    dest_[index] = pos;
    markDirty();
}
void SurfaceQuad::setDestinationPoints(const std::array<Vec2, 4>& points) {
    dest_ = points;
    markDirty();
}
std::array<Vec2, 4>& SurfaceQuad::uvPoints() {
    markDirty();
    return uv_;
}
const std::array<Vec2, 4>& SurfaceQuad::uvPoints() const { return uv_; }
void SurfaceQuad::setUvPoint(int index, Vec2 uv) {
    if (index < 0 || index >= 4) return;
    uv_[index] = uv;
    markDirty();
}
void SurfaceQuad::setUvPoints(const std::array<Vec2, 4>& points) {
    uv_ = points;
    markDirty();
}
bool SurfaceQuad::perspectiveCorrection() const { return perspective_; }
void SurfaceQuad::setPerspectiveCorrection(bool e) { perspective_ = e; markDirty(); }
int SurfaceQuad::meshResolution() const { return meshResolution_; }
void SurfaceQuad::setMeshResolution(int resolution) {
    int clamped = std::max(1, std::min(96, resolution));
    if (clamped == meshResolution_) return;
    meshResolution_ = clamped;
    markDirty();
}

void SurfaceQuad::resetToCanvas() {
    dest_ = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    markDirty();
}

void SurfaceQuad::resetToRect(Rect r) {
    dest_ = {{ Vec2(r.x, r.y), Vec2(r.x + r.w, r.y),
               Vec2(r.x + r.w, r.y + r.h), Vec2(r.x, r.y + r.h) }};
    markDirty();
}

void SurfaceQuad::rotateCW() {
    // Rotate destination points clockwise: 0←1←2←3←0
    Vec2 tmp = dest_[3];
    dest_[3] = dest_[2];
    dest_[2] = dest_[1];
    dest_[1] = dest_[0];
    dest_[0] = tmp;
    markDirty();
}

void SurfaceQuad::rotateCCW() {
    // Rotate destination points counter-clockwise: 0→1→2→3→0
    Vec2 tmp = dest_[0];
    dest_[0] = dest_[1];
    dest_[1] = dest_[2];
    dest_[2] = dest_[3];
    dest_[3] = tmp;
    markDirty();
}

void SurfaceQuad::flipHorizontal() {
    // Swap left and right: 0↔1, 3↔2
    std::swap(dest_[0], dest_[1]);
    std::swap(dest_[3], dest_[2]);
    markDirty();
}

void SurfaceQuad::flipVertical() {
    // Swap top and bottom: 0↔3, 1↔2
    std::swap(dest_[0], dest_[3]);
    std::swap(dest_[1], dest_[2]);
    markDirty();
}

void SurfaceQuad::expandToCanvas() { resetToCanvas(); }

// ===========================================================================
// buildMesh
// ===========================================================================
MeshBuildResult SurfaceQuad::buildMesh(const MeshBuildContext& ctx) {
    MeshBuildResult result;
    MeshData& mesh = result.mesh;

    int subdiv = std::max(1, std::max(meshResolution_, ctx.meshSubdivision));
    size_t vertexCount = size_t(subdiv + 1) * size_t(subdiv + 1);
    mesh.vertices.reserve(vertexCount * 2);
    mesh.uvs.reserve(vertexCount * 2);
    mesh.indices.reserve(size_t(subdiv) * size_t(subdiv) * 6);

    bool usePerspective = perspective_;
    ResultT<Mat3> H;
    if (usePerspective) {
        std::array<Vec2, 4> srcArr = {{ Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1) }};
        H = computeHomography(srcArr, dest_);
        if (!H.ok) {
            result.message = H.message;
            usePerspective = false;
        }
    }

    if (usePerspective) {
        // --- Perspective-correct: use homography to warp UVs ---
        // For each grid point (u,v) in [0,1]x[0,1], compute the
        // destination position via homography
        for (int iy = 0; iy <= subdiv; ++iy) {
            float v = float(iy) / float(subdiv);
            for (int ix = 0; ix <= subdiv; ++ix) {
                float u = float(ix) / float(subdiv);

                // Source UV position (bilinear interpolation of UV corners)
                float su = (1 - u) * (1 - v) * uv_[0].x + u * (1 - v) * uv_[1].x +
                           u * v * uv_[2].x + (1 - u) * v * uv_[3].x;
                float sv = (1 - u) * (1 - v) * uv_[0].y + u * (1 - v) * uv_[1].y +
                           u * v * uv_[2].y + (1 - u) * v * uv_[3].y;

                // Destination geometry is controlled by canonical surface-local
                // coordinates. uv_ only remaps texture sampling.
                Vec2 dstPt = applyHomography(H.value, Vec2(u, v));

                mesh.addVertex(dstPt.x, dstPt.y, su, sv);
            }
        }
    } else {
        // --- Bilinear: simple interpolation ---
        for (int iy = 0; iy <= subdiv; ++iy) {
            float v = float(iy) / float(subdiv);
            for (int ix = 0; ix <= subdiv; ++ix) {
                float u = float(ix) / float(subdiv);

                // Bilinear interpolation of destination corners
                float dx = (1 - u) * (1 - v) * dest_[0].x + u * (1 - v) * dest_[1].x +
                           u * v * dest_[2].x + (1 - u) * v * dest_[3].x;
                float dy = (1 - u) * (1 - v) * dest_[0].y + u * (1 - v) * dest_[1].y +
                           u * v * dest_[2].y + (1 - u) * v * dest_[3].y;

                // Source UV (bilinear from uv corners)
                float su = (1 - u) * (1 - v) * uv_[0].x + u * (1 - v) * uv_[1].x +
                           u * v * uv_[2].x + (1 - u) * v * uv_[3].x;
                float sv = (1 - u) * (1 - v) * uv_[0].y + u * (1 - v) * uv_[1].y +
                           u * v * uv_[2].y + (1 - u) * v * uv_[3].y;

                mesh.addVertex(dx, dy, su, sv);
            }
        }
    }

    // --- Generate triangle indices ---
    int stride = subdiv + 1;
    for (int iy = 0; iy < subdiv; ++iy) {
        for (int ix = 0; ix < subdiv; ++ix) {
            uint32_t i00 = uint32_t(iy * stride + ix);
            uint32_t i10 = uint32_t(iy * stride + ix + 1);
            uint32_t i01 = uint32_t((iy + 1) * stride + ix);
            uint32_t i11 = uint32_t((iy + 1) * stride + ix + 1);

            // Two triangles per quad cell
            mesh.addTriangle(i00, i10, i11);
            mesh.addTriangle(i00, i11, i01);
        }
    }

    result.ok = true;
    return result;
}

// ===========================================================================
// hitTest
// ===========================================================================
HitResult SurfaceQuad::hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = canvasNormPos;

    // --- Check vertex proximity ---
    float bestVertDist = options.radiusPixels / 1000.0f; // convert pixel radius to norm approx
    int bestVertIdx = -1;
    for (int i = 0; i < 4; ++i) {
        float d = distanceVec2(canvasNormPos, dest_[i]);
        if (d < bestVertDist) {
            bestVertDist = d;
            bestVertIdx = i;
        }
    }
    if (bestVertIdx >= 0) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Vertex;
        hr.handleIndex = bestVertIdx;
        return hr;
    }

    // --- Check edge proximity ---
    float bestEdgeDist = options.radiusPixels / 1000.0f;
    int bestEdgeIdx = -1;
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        float d = distToSegment(canvasNormPos, dest_[i], dest_[j]);
        if (d < bestEdgeDist) {
            bestEdgeDist = d;
            bestEdgeIdx = i;
        }
    }
    if (bestEdgeIdx >= 0) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Edge;
        hr.handleIndex = bestEdgeIdx;
        return hr;
    }

    // --- Check body (point-in-polygon) ---
    if (pointInConvexQuad(dest_, canvasNormPos)) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Body;
        return hr;
    }

    return hr;
}

// ===========================================================================
// validateGeometry
// ===========================================================================
GeometryValidation SurfaceQuad::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    // NaN check
    for (int i = 0; i < 4; ++i) {
        if (isNanVec2(dest_[i])) {
            v.valid = false;
            v.hasNaN = true;
            v.message = "Quad has NaN coordinates";
            return v;
        }
    }

    // Convert to vector for geometry utilities
    std::vector<Vec2> poly(dest_.begin(), dest_.end());

    // Self-intersection check
    if (geometry::isSelfIntersecting(poly)) {
        v.valid = false;
        v.selfIntersecting = true;
        v.message = "Quad is self-intersecting";
        return v;
    }

    // Area check
    if (geometry::isTooSmall(poly)) {
        v.valid = false;
        v.tooSmall = true;
        v.message = "Quad area too small";
        return v;
    }

    // Winding check
    float area = geometry::polygonArea(poly);
    if (area < 0) {
        v.windingFlipped = true;
        v.message = "Quad winding is CW (expected CCW)";
        // Not necessarily invalid, just a warning
    }

    return v;
}

} // namespace mapwrap
} // namespace tcx
