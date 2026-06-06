// =============================================================================
// tcxMapWrap — SurfaceBezier.cpp Implementation
// =============================================================================
// Bezier surface patch with a 2D control lattice. Mesh vertices are generated
// by evaluating Bernstein basis functions in u and v.

#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/GeometryValidation.h"

#include <algorithm>
#include <cmath>

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool isNanVec2(Vec2 p) { return std::isnan(p.x) || std::isnan(p.y); }

static float distVec2(Vec2 a, Vec2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

static bool pointInPolygon(const std::vector<Vec2>& poly, Vec2 p) {
    int n = static_cast<int>(poly.size());
    if (n < 3) return false;

    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float yi = poly[i].y;
        float yj = poly[j].y;
        float xi = poly[i].x;
        float xj = poly[j].x;
        if (((yi > p.y) != (yj > p.y)) &&
            (p.x < (xj - xi) * (p.y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

static double binomial(int n, int k) {
    if (k < 0 || k > n) return 0.0;
    if (k == 0 || k == n) return 1.0;
    k = std::min(k, n - k);
    double result = 1.0;
    for (int i = 1; i <= k; ++i) {
        result *= double(n - (k - i));
        result /= double(i);
    }
    return result;
}

static double bernstein(int i, int n, double t) {
    if (n <= 0) return 1.0;
    if (t <= 0.0) return i == 0 ? 1.0 : 0.0;
    if (t >= 1.0) return i == n ? 1.0 : 0.0;
    return binomial(n, i) * std::pow(t, i) * std::pow(1.0 - t, n - i);
}

static std::vector<Vec2> sampledBoundary(const SurfaceBezier& surface, int samplesPerEdge) {
    samplesPerEdge = std::max(4, samplesPerEdge);

    std::vector<Vec2> boundary;
    boundary.reserve(samplesPerEdge * 4);

    for (int i = 0; i < samplesPerEdge; ++i) {
        float u = float(i) / float(samplesPerEdge - 1);
        boundary.push_back(surface.evaluate(u, 0.0f));
    }
    for (int i = 1; i < samplesPerEdge; ++i) {
        float v = float(i) / float(samplesPerEdge - 1);
        boundary.push_back(surface.evaluate(1.0f, v));
    }
    for (int i = 1; i < samplesPerEdge; ++i) {
        float u = 1.0f - float(i) / float(samplesPerEdge - 1);
        boundary.push_back(surface.evaluate(u, 1.0f));
    }
    for (int i = 1; i + 1 < samplesPerEdge; ++i) {
        float v = 1.0f - float(i) / float(samplesPerEdge - 1);
        boundary.push_back(surface.evaluate(0.0f, v));
    }

    return boundary;
}

// ===========================================================================
// SurfaceBezier
// ===========================================================================

SurfaceBezier::SurfaceBezier(int controlCols, int controlRows)
    : controlCols_(std::max(2, controlCols)),
      controlRows_(std::max(2, controlRows))
{
    name_ = "Bezier";
    resetControlPoints();
}

int SurfaceBezier::controlCols() const { return controlCols_; }
int SurfaceBezier::controlRows() const { return controlRows_; }

std::unique_ptr<Surface> SurfaceBezier::clone() const {
    return std::make_unique<SurfaceBezier>(*this);
}

void SurfaceBezier::setControlDimensions(int cols, int rows) {
    cols = std::max(2, cols);
    rows = std::max(2, rows);
    if (cols == controlCols_ && rows == controlRows_) return;

    SurfaceBezier old(controlCols_, controlRows_);
    old.controlPoints_ = controlPoints_;
    old.meshResolution_ = meshResolution_;

    controlCols_ = cols;
    controlRows_ = rows;
    controlPoints_.assign(size_t(controlCols_ * controlRows_), Vec2());

    for (int r = 0; r < controlRows_; ++r) {
        float v = controlRows_ == 1 ? 0.0f : float(r) / float(controlRows_ - 1);
        for (int c = 0; c < controlCols_; ++c) {
            float u = controlCols_ == 1 ? 0.0f : float(c) / float(controlCols_ - 1);
            controlPoints_[r * controlCols_ + c] = old.evaluate(u, v);
        }
    }

    markDirty();
}

Vec2 SurfaceBezier::controlPoint(int col, int row) const {
    col = std::max(0, std::min(col, controlCols_ - 1));
    row = std::max(0, std::min(row, controlRows_ - 1));
    return controlPoints_[row * controlCols_ + col];
}

void SurfaceBezier::setControlPoint(int col, int row, Vec2 pos) {
    if (col < 0 || col >= controlCols_ || row < 0 || row >= controlRows_) return;
    controlPoints_[row * controlCols_ + col] = pos;
    markDirty();
}

std::vector<Vec2>& SurfaceBezier::controlPoints() {
    markDirty();
    return controlPoints_;
}
const std::vector<Vec2>& SurfaceBezier::controlPoints() const { return controlPoints_; }
void SurfaceBezier::setControlPoints(const std::vector<Vec2>& points) {
    if (points.size() != size_t(controlCols_ * controlRows_)) return;
    controlPoints_ = points;
    markDirty();
}

int SurfaceBezier::meshResolution() const { return meshResolution_; }

void SurfaceBezier::setMeshResolution(int resolution) {
    int clamped = std::max(2, std::min(96, resolution));
    if (clamped == meshResolution_) return;
    meshResolution_ = clamped;
    markDirty();
}

void SurfaceBezier::resetControlPoints() {
    controlPoints_.assign(size_t(controlCols_ * controlRows_), Vec2());

    for (int r = 0; r < controlRows_; ++r) {
        float v = controlRows_ == 1 ? 0.0f : float(r) / float(controlRows_ - 1);
        for (int c = 0; c < controlCols_; ++c) {
            float u = controlCols_ == 1 ? 0.0f : float(c) / float(controlCols_ - 1);
            controlPoints_[r * controlCols_ + c] = Vec2(
                0.12f + u * 0.76f,
                0.12f + v * 0.76f);
        }
    }
    markDirty();
}

Vec2 SurfaceBezier::evaluate(float u, float v) const {
    if (controlPoints_.empty()) return Vec2();

    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));

    int degreeU = controlCols_ - 1;
    int degreeV = controlRows_ - 1;
    double x = 0.0;
    double y = 0.0;

    for (int r = 0; r < controlRows_; ++r) {
        double bv = bernstein(r, degreeV, v);
        for (int c = 0; c < controlCols_; ++c) {
            double bu = bernstein(c, degreeU, u);
            double weight = bu * bv;
            Vec2 p = controlPoints_[r * controlCols_ + c];
            x += double(p.x) * weight;
            y += double(p.y) * weight;
        }
    }

    return Vec2(static_cast<float>(x), static_cast<float>(y));
}

MeshBuildResult SurfaceBezier::buildMesh(const MeshBuildContext& ctx) {
    (void)ctx;

    MeshBuildResult result;
    MeshData& mesh = result.mesh;

    if (controlCols_ < 2 || controlRows_ < 2 ||
        controlPoints_.size() != size_t(controlCols_ * controlRows_)) {
        result.ok = false;
        result.message = "Bezier surface has invalid control dimensions";
        return result;
    }

    int res = std::max(2, meshResolution_);
    int stride = res + 1;
    size_t vertexCount = size_t(stride * stride);
    mesh.vertices.reserve(vertexCount * 2);
    mesh.uvs.reserve(vertexCount * 2);
    mesh.indices.reserve(size_t(res * res * 6));

    for (int iy = 0; iy <= res; ++iy) {
        float v = float(iy) / float(res);
        for (int ix = 0; ix <= res; ++ix) {
            float u = float(ix) / float(res);
            Vec2 pt = evaluate(u, v);
            mesh.addVertex(pt.x, pt.y, u, v);
        }
    }

    for (int iy = 0; iy < res; ++iy) {
        for (int ix = 0; ix < res; ++ix) {
            uint32_t i00 = uint32_t(iy * stride + ix);
            uint32_t i10 = uint32_t(iy * stride + ix + 1);
            uint32_t i01 = uint32_t((iy + 1) * stride + ix);
            uint32_t i11 = uint32_t((iy + 1) * stride + ix + 1);
            mesh.addTriangle(i00, i10, i11);
            mesh.addTriangle(i00, i11, i01);
        }
    }

    result.ok = true;
    return result;
}

HitResult SurfaceBezier::hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = canvasNormPos;

    float radius = options.radiusPixels / 1000.0f;
    float bestDist = radius;
    int bestIdx = -1;

    for (int i = 0; i < static_cast<int>(controlPoints_.size()); ++i) {
        float d = distVec2(canvasNormPos, controlPoints_[i]);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }

    if (bestIdx >= 0) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::GridPoint;
        hr.handleIndex = bestIdx;
        return hr;
    }

    auto boundary = sampledBoundary(*this, std::max(8, meshResolution_ / 2));
    if (pointInPolygon(boundary, canvasNormPos)) {
        hr.hit = true;
        hr.surfaceId = id_;
        hr.handleKind = HandleKind::Body;
        return hr;
    }

    return hr;
}

GeometryValidation SurfaceBezier::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    if (controlCols_ < 2 || controlRows_ < 2 ||
        controlPoints_.size() != size_t(controlCols_ * controlRows_)) {
        v.valid = false;
        v.tooSmall = true;
        v.message = "Bezier surface has invalid control dimensions";
        return v;
    }

    for (int i = 0; i < static_cast<int>(controlPoints_.size()); ++i) {
        if (isNanVec2(controlPoints_[i])) {
            v.valid = false;
            v.hasNaN = true;
            v.message = "Bezier surface has NaN coordinates at point " + std::to_string(i);
            return v;
        }
    }

    auto boundary = sampledBoundary(*this, std::max(8, meshResolution_ / 2));
    if (geometry::isTooSmall(boundary)) {
        v.valid = false;
        v.tooSmall = true;
        v.message = "Bezier surface area too small";
        return v;
    }

    if (geometry::isSelfIntersecting(boundary)) {
        v.valid = false;
        v.selfIntersecting = true;
        v.message = "Bezier surface boundary is self-intersecting";
        return v;
    }

    return v;
}

} // namespace mapwrap
} // namespace tcx
