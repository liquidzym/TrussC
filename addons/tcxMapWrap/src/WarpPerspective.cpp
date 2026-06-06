// =============================================================================
// tcxMapWrap — WarpPerspective.cpp Implementation
// =============================================================================
// Four-point homography computation with a compact 8x8 linear solve.

#include "tcxMapWrap/WarpPerspective.h"

#include <cmath>
#include <algorithm>

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Internal: NaN/collinearity checks
// ---------------------------------------------------------------------------

static bool isNan(Vec2 p) {
    return std::isnan(p.x) || std::isnan(p.y);
}

/// Signed area of triangle formed by 3 points (2x cross product / 2)
static float triangleArea2(Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

/// Check if any 3 of the 4 points are collinear at the point-set scale.
static bool hasCollinearTriplet(const std::array<Vec2, 4>& pts, float eps = 1e-5f) {
    float minX = pts[0].x, maxX = pts[0].x;
    float minY = pts[0].y, maxY = pts[0].y;
    for (const auto& p : pts) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    float scale = std::max(maxX - minX, maxY - minY);
    float threshold = eps * std::max(1.0f, scale * scale);

    for (int a = 0; a < 4; ++a) {
        for (int b = a + 1; b < 4; ++b) {
            for (int c = b + 1; c < 4; ++c) {
                if (std::fabs(triangleArea2(pts[a], pts[b], pts[c])) <= threshold) {
                    return true;
                }
            }
        }
    }
    return false;
}

/// Check if the quadrilateral area is too small
static bool quadAreaTooSmall(const std::array<Vec2, 4>& pts, float minArea = 1e-8f) {
    float area = 0.0f;
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    return std::fabs(area * 0.5f) < minArea;
}

static bool solve8x8(double a[8][9], double out[8]) {
    constexpr double kPivotEps = 1e-12;

    for (int col = 0; col < 8; ++col) {
        int pivot = col;
        double best = std::fabs(a[col][col]);
        for (int row = col + 1; row < 8; ++row) {
            double candidate = std::fabs(a[row][col]);
            if (candidate > best) {
                best = candidate;
                pivot = row;
            }
        }

        if (best < kPivotEps) {
            return false;
        }

        if (pivot != col) {
            for (int k = col; k < 9; ++k) {
                std::swap(a[col][k], a[pivot][k]);
            }
        }

        double pivotValue = a[col][col];
        for (int k = col; k < 9; ++k) {
            a[col][k] /= pivotValue;
        }

        for (int row = 0; row < 8; ++row) {
            if (row == col) continue;

            double factor = a[row][col];
            if (std::fabs(factor) < kPivotEps) continue;

            for (int k = col; k < 9; ++k) {
                a[row][k] -= factor * a[col][k];
            }
        }
    }

    for (int i = 0; i < 8; ++i) {
        out[i] = a[i][8];
    }
    return true;
}

// ===========================================================================
// computeHomography — DLT with 4 point correspondences
// ===========================================================================

ResultT<Mat3> computeHomography(const std::array<Vec2, 4>& src,
                                 const std::array<Vec2, 4>& dst) {
    // --- Check for NaN ---
    for (int i = 0; i < 4; ++i) {
        if (isNan(src[i]) || isNan(dst[i]))
            return ResultT<Mat3>::error("NaN in point correspondences");
    }

    // --- Check for collinear points ---
    if (hasCollinearTriplet(src))
        return ResultT<Mat3>::error("Source points have collinear triplet");
    if (hasCollinearTriplet(dst))
        return ResultT<Mat3>::error("Destination points have collinear triplet");

    // --- Check area ---
    if (quadAreaTooSmall(src))
        return ResultT<Mat3>::error("Source quad area too small");
    if (quadAreaTooSmall(dst))
        return ResultT<Mat3>::error("Destination quad area too small");

    // Solve the 8 unknowns with h[8] fixed to 1:
    //   x*h0 + y*h1 + h2 - u*x*h6 - u*y*h7 = u
    //   x*h3 + y*h4 + h5 - v*x*h6 - v*y*h7 = v
    double A[8][9] = {};
    for (int i = 0; i < 4; ++i) {
        double x = src[i].x;
        double y = src[i].y;
        double u = dst[i].x;
        double v = dst[i].y;

        int row0 = 2 * i;
        A[row0][0] = x;
        A[row0][1] = y;
        A[row0][2] = 1.0;
        A[row0][6] = -u * x;
        A[row0][7] = -u * y;
        A[row0][8] = u;

        int row1 = row0 + 1;
        A[row1][3] = x;
        A[row1][4] = y;
        A[row1][5] = 1.0;
        A[row1][6] = -v * x;
        A[row1][7] = -v * y;
        A[row1][8] = v;
    }

    double h[8];
    if (!solve8x8(A, h)) {
        return ResultT<Mat3>::error("Homography system is singular");
    }

    Mat3 result;
    result.m[0] = static_cast<float>(h[0]);
    result.m[1] = static_cast<float>(h[1]);
    result.m[2] = static_cast<float>(h[2]);
    result.m[3] = static_cast<float>(h[3]);
    result.m[4] = static_cast<float>(h[4]);
    result.m[5] = static_cast<float>(h[5]);
    result.m[6] = static_cast<float>(h[6]);
    result.m[7] = static_cast<float>(h[7]);
    result.m[8] = 1.0f;

    // Final sanity check
    bool allFinite = true;
    for (int i = 0; i < 9; ++i) {
        if (std::isnan(result.m[i]) || std::isinf(result.m[i])) {
            allFinite = false;
            break;
        }
    }
    if (!allFinite)
        return ResultT<Mat3>::error("Homography contains NaN or Inf");

    return ResultT<Mat3>::success(result);
}

// ===========================================================================
// WarpPerspective
// ===========================================================================

void WarpPerspective::reset() {
    src_ = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    dst_ = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
}

std::unique_ptr<Warp> WarpPerspective::clone() const {
    return std::make_unique<WarpPerspective>(*this);
}

std::array<Vec2, 4>& WarpPerspective::srcPoints() { return src_; }
const std::array<Vec2, 4>& WarpPerspective::srcPoints() const { return src_; }
std::array<Vec2, 4>& WarpPerspective::dstPoints() { return dst_; }
const std::array<Vec2, 4>& WarpPerspective::dstPoints() const { return dst_; }

ResultT<Mat3> WarpPerspective::computeMatrix() const {
    return computeHomography(src_, dst_);
}

} // namespace mapwrap
} // namespace tcx
