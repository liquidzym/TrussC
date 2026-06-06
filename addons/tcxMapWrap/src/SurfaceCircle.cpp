// =============================================================================
// tcxMapWrap — SurfaceCircle.cpp Implementation
// =============================================================================
// Ellipse (circle with independent rx/ry and rotation) surface.
// Triangle-fan mesh with configurable segments. Ellipse point test,
// center/radius handle proximity.

#include "tcxMapWrap/SurfaceCircle.h"
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

/// Transform a point into the ellipse's local coordinate system
/// (centered at origin, axis-aligned). Returns (localX, localY).
static Vec2 toEllipseLocal(Vec2 p, Vec2 center, float rotation) {
    float dx = p.x - center.x;
    float dy = p.y - center.y;
    float cosR = std::cos(-rotation * (float)M_PI / 180.0f);
    float sinR = std::sin(-rotation * (float)M_PI / 180.0f);
    return Vec2(dx * cosR - dy * sinR, dx * sinR + dy * cosR);
}

/// Test if a point is inside the ellipse
static bool pointInEllipse(Vec2 p, Vec2 center, float rx, float ry, float rotation) {
    Vec2 local = toEllipseLocal(p, center, rotation);
    if (rx < 1e-10f || ry < 1e-10f) return false;
    float nx = local.x / rx;
    float ny = local.y / ry;
    return (nx * nx + ny * ny) <= 1.0f;
}

// ===========================================================================
// SurfaceCircle
// ===========================================================================

SurfaceCircle::SurfaceCircle() { name_ = "Circle"; }

std::unique_ptr<Surface> SurfaceCircle::clone() const {
    return std::make_unique<SurfaceCircle>(*this);
}

Vec2 SurfaceCircle::center() const { return center_; }
void SurfaceCircle::setCenter(Vec2 c) { center_ = c; markDirty(); }
float SurfaceCircle::radiusX() const { return radiusX_; }
void SurfaceCircle::setRadiusX(float r) { radiusX_ = r; markDirty(); }
float SurfaceCircle::radiusY() const { return radiusY_; }
void SurfaceCircle::setRadiusY(float r) { radiusY_ = r; markDirty(); }
float SurfaceCircle::rotation() const { return rotation_; }
void SurfaceCircle::setRotation(float d) { rotation_ = d; markDirty(); }
int SurfaceCircle::segments() const { return segments_; }
void SurfaceCircle::setSegments(int s) {
    segments_ = std::max(kMinSegments, std::min(kMaxSegments, s));
    markDirty();
}

// ===========================================================================
// buildMesh — triangle fan
// ===========================================================================
MeshBuildResult SurfaceCircle::buildMesh(const MeshBuildContext& ctx) {
    MeshBuildResult result;
    MeshData& mesh = result.mesh;

    int segs = std::max(kMinSegments, std::min(kMaxSegments, segments_));
    mesh.vertices.reserve(size_t(segs + 2) * 2);
    mesh.uvs.reserve(size_t(segs + 2) * 2);
    mesh.indices.reserve(size_t(segs) * 3);
    float rotRad = rotation_ * (float)M_PI / 180.0f;
    float cosR = std::cos(rotRad);
    float sinR = std::sin(rotRad);

    // Center vertex (index 0)
    mesh.addVertex(center_.x, center_.y, 0.5f, 0.5f);

    // Perimeter vertices (indices 1..segs)
    for (int i = 0; i <= segs; ++i) {
        float angle = 2.0f * (float)M_PI * float(i) / float(segs);
        float lx = radiusX_ * std::cos(angle);
        float ly = radiusY_ * std::sin(angle);

        // Rotate by ellipse rotation
        float wx = center_.x + lx * cosR - ly * sinR;
        float wy = center_.y + lx * sinR + ly * cosR;

        // UV: map from [-rx,rx] x [-ry,ry] to [0,1]x[0,1]
        float u = 0.5f + 0.5f * std::cos(angle);
        float v = 0.5f + 0.5f * std::sin(angle);

        mesh.addVertex(wx, wy, u, v);
    }

    // Triangle indices: fan from center
    for (int i = 0; i < segs; ++i) {
        uint32_t i0 = 0;              // center
        uint32_t i1 = uint32_t(i + 1);     // current perimeter
        uint32_t i2 = uint32_t(i + 2);     // next perimeter
        mesh.addTriangle(i0, i1, i2);
    }

    result.ok = true;
    return result;
}

// ===========================================================================
// hitTest
// ===========================================================================
HitResult SurfaceCircle::hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = canvasNormPos;

    float radius = options.radiusPixels / 1000.0f;

    // --- Check center handle proximity ---
    if (distVec2(canvasNormPos, center_) < radius) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Vertex;
        hr.handleIndex = 0;
        return hr;
    }

    // --- Check radius handle proximity ---
    // Right edge handle: center + (rx, 0) rotated
    float rotRad = rotation_ * (float)M_PI / 180.0f;
    float cosR = std::cos(rotRad);
    float sinR = std::sin(rotRad);
    Vec2 rxHandle(center_.x + radiusX_ * cosR, center_.y + radiusX_ * sinR);
    Vec2 ryHandle(center_.x - radiusY_ * sinR, center_.y + radiusY_ * cosR);

    if (distVec2(canvasNormPos, rxHandle) < radius) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Vertex;
        hr.handleIndex = 1;
        return hr;
    }
    if (distVec2(canvasNormPos, ryHandle) < radius) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Vertex;
        hr.handleIndex = 2;
        return hr;
    }

    // --- Check body (ellipse point test) ---
    if (pointInEllipse(canvasNormPos, center_, radiusX_, radiusY_, rotation_)) {
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
GeometryValidation SurfaceCircle::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    // NaN check
    if (isNanVec2(center_) || std::isnan(radiusX_) || std::isnan(radiusY_) ||
        std::isnan(rotation_)) {
        v.valid = false;
        v.hasNaN = true;
        v.message = "Circle has NaN coordinates or parameters";
        return v;
    }

    // Radius check
    if (radiusX_ < 0.001f || radiusY_ < 0.001f) {
        v.valid = false;
        v.tooSmall = true;
        v.message = "Circle radius too small";
        return v;
    }

    // Segment count check
    if (segments_ < 3) {
        v.valid = false;
        v.message = "Circle needs at least 3 segments";
        return v;
    }

    // Check if ellipse is inside reasonable bounds
    float maxR = std::max(radiusX_, radiusY_);
    if (center_.x - maxR < -10.0f || center_.x + maxR > 10.0f ||
        center_.y - maxR < -10.0f || center_.y + maxR > 10.0f) {
        v.message = "Circle extends far outside canvas bounds";
        // Not necessarily invalid, just a warning
    }

    return v;
}

} // namespace mapwrap
} // namespace tcx
