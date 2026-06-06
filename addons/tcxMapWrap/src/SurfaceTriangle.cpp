// =============================================================================
// tcxMapWrap — SurfaceTriangle.cpp Implementation
// =============================================================================
// Single triangle or subdivided mesh with UV. Point-in-triangle test,
// vertex/edge proximity, degenerate triangle validation.

#include "tcxMapWrap/SurfaceTriangle.h"
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

/// Signed area of triangle (2x cross product / 2)
static float triSignedArea(Vec2 a, Vec2 b, Vec2 c) {
    return 0.5f * ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
}

/// Point-in-triangle using barycentric coordinates
static bool pointInTriangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

// ===========================================================================
// SurfaceTriangle
// ===========================================================================

SurfaceTriangle::SurfaceTriangle() { name_ = "Triangle"; }

std::unique_ptr<Surface> SurfaceTriangle::clone() const {
    return std::make_unique<SurfaceTriangle>(*this);
}

std::array<Vec2, 3>& SurfaceTriangle::destinationPoints() {
    markDirty();
    return dest_;
}
const std::array<Vec2, 3>& SurfaceTriangle::destinationPoints() const { return dest_; }
void SurfaceTriangle::setDestinationPoint(int index, Vec2 pos) {
    if (index < 0 || index >= 3) return;
    dest_[index] = pos;
    markDirty();
}
void SurfaceTriangle::setDestinationPoints(const std::array<Vec2, 3>& points) {
    dest_ = points;
    markDirty();
}
std::array<Vec2, 3>& SurfaceTriangle::uvPoints() {
    markDirty();
    return uv_;
}
const std::array<Vec2, 3>& SurfaceTriangle::uvPoints() const { return uv_; }
void SurfaceTriangle::setUvPoint(int index, Vec2 uv) {
    if (index < 0 || index >= 3) return;
    uv_[index] = uv;
    markDirty();
}
void SurfaceTriangle::setUvPoints(const std::array<Vec2, 3>& points) {
    uv_ = points;
    markDirty();
}

// ===========================================================================
// buildMesh
// ===========================================================================
MeshBuildResult SurfaceTriangle::buildMesh(const MeshBuildContext& ctx) {
    MeshBuildResult result;
    MeshData& mesh = result.mesh;

    int subdiv = std::max(1, ctx.meshSubdivision);
    size_t vertexCount = size_t(subdiv + 1) * size_t(subdiv + 2) / 2;
    size_t triangleCount = size_t(subdiv) * size_t(subdiv);
    mesh.vertices.reserve(vertexCount * 2);
    mesh.uvs.reserve(vertexCount * 2);
    mesh.indices.reserve(triangleCount * 3);

    if (subdiv <= 1) {
        // --- Simple triangle (3 vertices, 1 triangle) ---
        for (int i = 0; i < 3; ++i) {
            mesh.addVertex(dest_[i].x, dest_[i].y, uv_[i].x, uv_[i].y);
        }
        mesh.addTriangle(0, 1, 2);
    } else {
        // --- Subdivided triangle mesh ---
        // Use barycentric subdivision: for each (i,j,k) where i+j+k = subdiv,
        // create a vertex at barycentric coordinates (i/subdiv, j/subdiv, k/subdiv).

        // Build vertex grid using barycentric coordinates
        int vertCount = 0;
        // Map from (i,j) to vertex index
        std::vector<int> vertIndex((subdiv + 1) * (subdiv + 1), -1);

        for (int i = 0; i <= subdiv; ++i) {
            for (int j = 0; j <= subdiv - i; ++j) {
                int k = subdiv - i - j;
                float u = float(i) / float(subdiv);
                float v = float(j) / float(subdiv);
                float w = float(k) / float(subdiv);

                // Position: barycentric interpolation of destination points
                float px = u * dest_[0].x + v * dest_[1].x + w * dest_[2].x;
                float py = u * dest_[0].y + v * dest_[1].y + w * dest_[2].y;

                // UV: barycentric interpolation of UV points
                float su = u * uv_[0].x + v * uv_[1].x + w * uv_[2].x;
                float sv = u * uv_[0].y + v * uv_[1].y + w * uv_[2].y;

                mesh.addVertex(px, py, su, sv);
                vertIndex[i * (subdiv + 1) + j] = vertCount;
                vertCount++;
            }
        }

        // Generate triangles
        for (int i = 0; i <= subdiv; ++i) {
            for (int j = 0; j <= subdiv - i - 1; ++j) {
                int idx00 = vertIndex[i * (subdiv + 1) + j];
                int idx10 = vertIndex[(i + 1) * (subdiv + 1) + j];
                int idx01 = vertIndex[i * (subdiv + 1) + (j + 1)];

                if (idx00 >= 0 && idx10 >= 0 && idx01 >= 0) {
                    mesh.addTriangle(uint32_t(idx00), uint32_t(idx10), uint32_t(idx01));
                }

                // Second triangle in the parallelogram (if it exists)
                if (i + j + 2 <= subdiv) {
                    int idx11 = vertIndex[(i + 1) * (subdiv + 1) + (j + 1)];
                    if (idx10 >= 0 && idx11 >= 0 && idx01 >= 0) {
                        mesh.addTriangle(uint32_t(idx10), uint32_t(idx11), uint32_t(idx01));
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
HitResult SurfaceTriangle::hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = canvasNormPos;

    float radius = options.radiusPixels / 1000.0f;

    // --- Check vertex proximity ---
    for (int i = 0; i < 3; ++i) {
        if (distVec2(canvasNormPos, dest_[i]) < radius) {
            hr.hit = true;
            hr.surfaceId = id_;
            hr.handleKind = HandleKind::Vertex;
            hr.handleIndex = i;
            return hr;
        }
    }

    // --- Check edge proximity ---
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        if (distToSegment(canvasNormPos, dest_[i], dest_[j]) < radius) {
            hr.hit = true;
            hr.surfaceId = id_;
            hr.handleKind = HandleKind::Edge;
            hr.handleIndex = i;
            return hr;
        }
    }

    // --- Check body (point-in-triangle) ---
    if (pointInTriangle(canvasNormPos, dest_[0], dest_[1], dest_[2])) {
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
GeometryValidation SurfaceTriangle::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    // NaN check
    for (int i = 0; i < 3; ++i) {
        if (isNanVec2(dest_[i])) {
            v.valid = false;
            v.hasNaN = true;
            v.message = "Triangle has NaN coordinates";
            return v;
        }
    }

    // Area check
    float area = triSignedArea(dest_[0], dest_[1], dest_[2]);
    float absArea = std::fabs(area);
    if (absArea < 1e-8f) {
        v.valid = false;
        v.tooSmall = true;
        v.message = "Triangle is degenerate (area too small or collinear points)";
        return v;
    }

    // Collinearity is already caught by area check, but explicit
    // Cross product == 0 means collinear
    float cross = (dest_[1].x - dest_[0].x) * (dest_[2].y - dest_[0].y) -
                  (dest_[1].y - dest_[0].y) * (dest_[2].x - dest_[0].x);
    if (std::fabs(cross) < 1e-10f) {
        v.valid = false;
        v.message = "Triangle points are collinear";
        return v;
    }

    return v;
}

} // namespace mapwrap
} // namespace tcx
