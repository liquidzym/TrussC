// =============================================================================
// tcxMapWrap — SurfaceGrid.cpp Implementation
// =============================================================================
// Grid surface with (cols+1)*(rows+1) control points. Supports bilinear and
// Catmull-Rom interpolation for mesh generation. addColumn/Row interpolates
// from neighbors to preserve shape.

#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/WarpGrid.h"
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

static Vec2 lerpVec2(Vec2 a, Vec2 b, float t) {
    return Vec2(a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t);
}

/// Point-in-polygon test (ray casting)
static bool pointInPolygon(const std::vector<Vec2>& poly, Vec2 p) {
    int n = (int)poly.size();
    if (n < 3) return false;
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float yi = poly[i].y, yj = poly[j].y;
        float xi = poly[i].x, xj = poly[j].x;
        if (((yi > p.y) != (yj > p.y)) &&
            (p.x < (xj - xi) * (p.y - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

// ===========================================================================
// SurfaceGrid
// ===========================================================================

SurfaceGrid::SurfaceGrid(int cols, int rows) : cols_(cols), rows_(rows) {
    name_ = "Grid";
    points_.resize((cols_ + 1) * (rows_ + 1));
    for (int r = 0; r <= rows_; ++r)
        for (int c = 0; c <= cols_; ++c)
            points_[r * (cols_ + 1) + c] = Vec2(float(c) / float(cols_), float(r) / float(rows_));
}

int SurfaceGrid::cols() const { return cols_; }
int SurfaceGrid::rows() const { return rows_; }

void SurfaceGrid::setCols(int c) {
    if (c == cols_) return;
    int newCols = std::max(1, c);
    std::vector<Vec2> newPoints((newCols + 1) * (rows_ + 1));

    for (int r = 0; r <= rows_; ++r) {
        for (int cc = 0; cc <= newCols; ++cc) {
            float oldCol = (float(cc) / float(newCols)) * float(cols_);
            int c0 = std::min(int(std::floor(oldCol)), cols_);
            int c1 = std::min(c0 + 1, cols_);
            float t = oldCol - float(c0);
            Vec2 left = points_[r * (cols_ + 1) + c0];
            Vec2 right = points_[r * (cols_ + 1) + c1];
            newPoints[r * (newCols + 1) + cc] = lerpVec2(left, right, t);
        }
    }

    cols_ = newCols;
    points_ = std::move(newPoints);
    markDirty();
}

void SurfaceGrid::setRows(int r) {
    if (r == rows_) return;
    int newRows = std::max(1, r);
    std::vector<Vec2> newPoints((cols_ + 1) * (newRows + 1));

    for (int rr = 0; rr <= newRows; ++rr) {
        float oldRow = (float(rr) / float(newRows)) * float(rows_);
        int r0 = std::min(int(std::floor(oldRow)), rows_);
        int r1 = std::min(r0 + 1, rows_);
        float t = oldRow - float(r0);
        for (int c = 0; c <= cols_; ++c) {
            Vec2 top = points_[r0 * (cols_ + 1) + c];
            Vec2 bottom = points_[r1 * (cols_ + 1) + c];
            newPoints[rr * (cols_ + 1) + c] = lerpVec2(top, bottom, t);
        }
    }

    rows_ = newRows;
    points_ = std::move(newPoints);
    markDirty();
}

Vec2 SurfaceGrid::gridPoint(int c, int r) const {
    return points_[r * (cols_ + 1) + c];
}

void SurfaceGrid::setGridPoint(int c, int r, Vec2 p) {
    points_[r * (cols_ + 1) + c] = p;
    markDirty();
}

// ---------------------------------------------------------------------------
// addColumn — insert a column between each existing pair, interpolating
// ---------------------------------------------------------------------------
void SurfaceGrid::addColumn() {
    int newCols = cols_ + 1;
    std::vector<Vec2> newPoints((newCols + 1) * (rows_ + 1));

    for (int r = 0; r <= rows_; ++r) {
        for (int c = 0; c <= newCols; ++c) {
            float oldCol = (float(c) / float(newCols)) * float(cols_);
            int c0 = std::min(int(std::floor(oldCol)), cols_);
            int c1 = std::min(c0 + 1, cols_);
            float t = oldCol - float(c0);
            Vec2 left = points_[r * (cols_ + 1) + c0];
            Vec2 right = points_[r * (cols_ + 1) + c1];
            newPoints[r * (newCols + 1) + c] = lerpVec2(left, right, t);
        }
    }

    cols_ = newCols;
    points_ = std::move(newPoints);
    markDirty();
}

// ---------------------------------------------------------------------------
// removeColumn — remove evenly-spaced columns, keeping shape
// ---------------------------------------------------------------------------
void SurfaceGrid::removeColumn() {
    if (cols_ <= 2) return; // minimum 2 columns (3 points across)
    int newCols = cols_ - 1;
    std::vector<Vec2> newPoints((newCols + 1) * (rows_ + 1));

    for (int r = 0; r <= rows_; ++r) {
        // Remove the last interior column (index cols_-1), keep boundaries
        int newC = 0;
        for (int c = 0; c <= cols_; ++c) {
            // Skip the second-to-last column (index cols_-1)
            if (c == cols_ - 1) continue;
            newPoints[r * (newCols + 1) + newC] = points_[r * (cols_ + 1) + c];
            newC++;
        }
    }

    cols_ = newCols;
    points_ = std::move(newPoints);
    markDirty();
}

// ---------------------------------------------------------------------------
// addRow — insert a row between each existing pair, interpolating
// ---------------------------------------------------------------------------
void SurfaceGrid::addRow() {
    int newRows = rows_ + 1;
    std::vector<Vec2> newPoints((cols_ + 1) * (newRows + 1));

    for (int r = 0; r <= newRows; ++r) {
        float oldRow = (float(r) / float(newRows)) * float(rows_);
        int r0 = std::min(int(std::floor(oldRow)), rows_);
        int r1 = std::min(r0 + 1, rows_);
        float t = oldRow - float(r0);
        for (int c = 0; c <= cols_; ++c) {
            Vec2 top = points_[r0 * (cols_ + 1) + c];
            Vec2 bottom = points_[r1 * (cols_ + 1) + c];
            newPoints[r * (cols_ + 1) + c] = lerpVec2(top, bottom, t);
        }
    }

    rows_ = newRows;
    points_ = std::move(newPoints);
    markDirty();
}

// ---------------------------------------------------------------------------
// removeRow — remove evenly-spaced rows, keeping shape
// ---------------------------------------------------------------------------
void SurfaceGrid::removeRow() {
    if (rows_ <= 2) return; // minimum 2 rows (3 points down)
    int newRows = rows_ - 1;
    std::vector<Vec2> newPoints((cols_ + 1) * (newRows + 1));

    for (int c = 0; c <= cols_; ++c) {
        int newR = 0;
        for (int r = 0; r <= rows_; ++r) {
            // Skip the second-to-last row
            if (r == rows_ - 1) continue;
            newPoints[newR * (cols_ + 1) + c] = points_[r * (cols_ + 1) + c];
            newR++;
        }
    }

    rows_ = newRows;
    points_ = std::move(newPoints);
    markDirty();
}

bool SurfaceGrid::curvedInterpolation() const { return curved_; }
void SurfaceGrid::setCurvedInterpolation(bool c) { curved_ = c; markDirty(); }
int SurfaceGrid::meshResolution() const { return meshRes_; }
void SurfaceGrid::setMeshResolution(int r) {
    int clamped = std::max(1, std::min(64, r));
    if (clamped == meshRes_) return;
    meshRes_ = clamped;
    markDirty();
}

// ===========================================================================
// buildMesh
// ===========================================================================
MeshBuildResult SurfaceGrid::buildMesh(const MeshBuildContext& ctx) {
    MeshBuildResult result;
    MeshData& mesh = result.mesh;

    int res = std::max(1, meshRes_);
    int meshCols = cols_ * res;
    int meshRows = rows_ * res;
    int stride = meshCols + 1;
    size_t vertexCount = size_t(meshCols + 1) * size_t(meshRows + 1);
    mesh.vertices.reserve(vertexCount * 2);
    mesh.uvs.reserve(vertexCount * 2);
    mesh.indices.reserve(size_t(meshCols) * size_t(meshRows) * 6);

    if (curved_) {
        // --- Catmull-Rom interpolation ---
        for (int iy = 0; iy <= meshRows; ++iy) {
            float rowF = float(iy) / float(meshRows); // 0..1 across the grid
            int rowIdx = std::min(int(rowF * rows_), rows_ - 1);
            float rowFrac = rowF * rows_ - rowIdx;

            for (int ix = 0; ix <= meshCols; ++ix) {
                float colF = float(ix) / float(meshCols);
                int colIdx = std::min(int(colF * cols_), cols_ - 1);
                float colFrac = colF * cols_ - colIdx;

                // Get 4x4 neighborhood of control points for Catmull-Rom
                Vec2 colPts[4], resultRow[4];
                for (int dr = -1; dr <= 2; ++dr) {
                    int rr = rowIdx + dr;
                    rr = std::max(0, std::min(rr, rows_)); // clamp
                    for (int dc = -1; dc <= 2; ++dc) {
                        int cc = colIdx + dc;
                        cc = std::max(0, std::min(cc, cols_)); // clamp
                        colPts[dc + 1] = points_[rr * (cols_ + 1) + cc];
                    }
                    // Interpolate across columns (Catmull-Rom in u direction)
                    resultRow[dr + 1] = catmullRom(colPts[0], colPts[1], colPts[2], colPts[3], colFrac);
                }
                // Interpolate across rows (Catmull-Rom in v direction)
                Vec2 pt = catmullRom(resultRow[0], resultRow[1], resultRow[2], resultRow[3], rowFrac);

                mesh.addVertex(pt.x, pt.y, colF, rowF);
            }
        }
    } else {
        // --- Bilinear interpolation ---
        for (int iy = 0; iy <= meshRows; ++iy) {
            float rowF = float(iy) / float(meshRows);
            int rowIdx = std::min(int(rowF * rows_), rows_ - 1);
            float rowFrac = rowF * rows_ - rowIdx;

            for (int ix = 0; ix <= meshCols; ++ix) {
                float colF = float(ix) / float(meshCols);
                int colIdx = std::min(int(colF * cols_), cols_ - 1);
                float colFrac = colF * cols_ - colIdx;

                // Four corners of the current grid cell
                Vec2 p00 = points_[(rowIdx)     * (cols_ + 1) + colIdx];
                Vec2 p10 = points_[(rowIdx)     * (cols_ + 1) + colIdx + 1];
                Vec2 p01 = points_[(rowIdx + 1) * (cols_ + 1) + colIdx];
                Vec2 p11 = points_[(rowIdx + 1) * (cols_ + 1) + colIdx + 1];

                Vec2 pt = bilinearInterpolate(p00, p10, p01, p11, colFrac, rowFrac);
                mesh.addVertex(pt.x, pt.y, colF, rowF);
            }
        }
    }

    // --- Generate triangle indices ---
    for (int iy = 0; iy < meshRows; ++iy) {
        for (int ix = 0; ix < meshCols; ++ix) {
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

// ===========================================================================
// hitTest
// ===========================================================================
HitResult SurfaceGrid::hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = canvasNormPos;

    float radius = options.radiusPixels / 1000.0f;

    // --- Check grid point proximity ---
    float bestDist = radius;
    int bestIdx = -1;
    for (int i = 0; i < (int)points_.size(); ++i) {
        float d = distVec2(canvasNormPos, points_[i]);
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

    // --- Check body (point-in-polygon of boundary) ---
    // Build boundary polygon from outer grid points
    std::vector<Vec2> boundary;
    // Top row
    for (int c = 0; c <= cols_; ++c) boundary.push_back(gridPoint(c, 0));
    // Right column (excluding top-right corner)
    for (int r = 1; r <= rows_; ++r) boundary.push_back(gridPoint(cols_, r));
    // Bottom row (excluding bottom-right corner)
    for (int c = cols_ - 1; c >= 0; --c) boundary.push_back(gridPoint(c, rows_));
    // Left column (excluding bottom-left and top-left corners)
    for (int r = rows_ - 1; r >= 1; --r) boundary.push_back(gridPoint(0, r));

    if (pointInPolygon(boundary, canvasNormPos)) {
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
GeometryValidation SurfaceGrid::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    // NaN check
    for (int i = 0; i < (int)points_.size(); ++i) {
        if (isNanVec2(points_[i])) {
            v.valid = false;
            v.hasNaN = true;
            v.message = "Grid has NaN coordinates at point " + std::to_string(i);
            return v;
        }
    }

    // Out-of-bounds check (normalized 0..1 is typical, but allow some margin)
    for (int i = 0; i < (int)points_.size(); ++i) {
        if (points_[i].x < -10.0f || points_[i].x > 10.0f ||
            points_[i].y < -10.0f || points_[i].y > 10.0f) {
            v.valid = false;
            v.message = "Grid point out of reasonable bounds at index " + std::to_string(i);
            return v;
        }
    }

    // Check grid dimensions
    if (cols_ < 1 || rows_ < 1) {
        v.valid = false;
        v.message = "Grid has invalid dimensions";
        return v;
    }

    if ((int)points_.size() != (cols_ + 1) * (rows_ + 1)) {
        v.valid = false;
        v.message = "Grid point count mismatch";
        return v;
    }

    // Check boundary self-intersection
    std::vector<Vec2> boundary;
    for (int c = 0; c <= cols_; ++c) boundary.push_back(gridPoint(c, 0));
    for (int r = 1; r <= rows_; ++r) boundary.push_back(gridPoint(cols_, r));
    for (int c = cols_ - 1; c >= 0; --c) boundary.push_back(gridPoint(c, rows_));
    for (int r = rows_ - 1; r >= 1; --r) boundary.push_back(gridPoint(0, r));

    if (geometry::isSelfIntersecting(boundary)) {
        v.valid = false;
        v.selfIntersecting = true;
        v.message = "Grid boundary is self-intersecting";
        return v;
    }

    if (geometry::isTooSmall(boundary)) {
        v.tooSmall = true;
        v.message = "Grid area is very small";
    }

    return v;
}

} // namespace mapwrap
} // namespace tcx
