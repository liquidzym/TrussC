// =============================================================================
// tcxMapWrap — MapWrapMask.cpp Full Implementation
// =============================================================================
// Mask geometry validation, hit testing for point/edge/body,
// and localized name helpers.

#include "tcxMapWrap/MapWrapMask.h"
#include "tcxMapWrap/MapWrapI18n.h"

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

/// Ray-casting point-in-polygon test
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

/// Test if a point is inside an ellipse defined by rect (bounding box)
static bool pointInEllipse(Vec2 p, const Rect& r) {
    if (r.w < 1e-10f || r.h < 1e-10f) return false;
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float rx = r.w * 0.5f;
    float ry = r.h * 0.5f;
    float nx = (p.x - cx) / rx;
    float ny = (p.y - cy) / ry;
    return (nx * nx + ny * ny) <= 1.0f;
}

// ===========================================================================
// validateGeometry
// ===========================================================================
GeometryValidation MapWrapMask::validateGeometry() const {
    GeometryValidation v;
    v.valid = true;

    switch (kind) {
        case MaskKind::Polygon:
        case MaskKind::Bezier:
        case MaskKind::Freehand: {
            // Need at least 3 points for a valid polygon mask
            if (points.size() < 3) {
                v.valid = false;
                v.tooSmall = true;
                v.message = tr("geometry.too_small");
                return v;
            }
            // NaN check
            for (size_t i = 0; i < points.size(); ++i) {
                if (isNanVec2(points[i])) {
                    v.valid = false;
                    v.hasNaN = true;
                    v.message = tr("geometry.has_nan");
                    return v;
                }
            }
            break;
        }

        case MaskKind::Rectangle:
        case MaskKind::Ellipse: {
            // Rect must have positive width and height
            if (rect.w <= 0 || rect.h <= 0) {
                v.valid = false;
                v.tooSmall = true;
                v.message = tr("geometry.too_small");
                return v;
            }
            // NaN check
            if (std::isnan(rect.x) || std::isnan(rect.y) ||
                std::isnan(rect.w) || std::isnan(rect.h)) {
                v.valid = false;
                v.hasNaN = true;
                v.message = tr("geometry.has_nan");
                return v;
            }
            break;
        }

        case MaskKind::AlphaTexture: {
            // Alpha texture mask needs a valid source ID
            if (alphaTextureSource.empty()) {
                v.valid = false;
                v.message = tr("mask.no_alpha_source");
                return v;
            }
            break;
        }
    }

    return v;
}

// ===========================================================================
// hitTest
// ===========================================================================
HitResult MapWrapMask::hitTest(const Vec2& pos, const HitTestOptions& options) const {
    HitResult hr;
    hr.canvasNormPos = pos;

    if (!enabled) return hr;

    // Radius in the mask's coordinate space (approximate conversion from pixels)
    float radius = options.radiusPixels / 1000.0f;

    switch (kind) {
        // ----- Polygon / Bezier / Freehand -----
        case MaskKind::Polygon:
        case MaskKind::Bezier:
        case MaskKind::Freehand: {
            int n = (int)points.size();
            if (n < 1) return hr;

            // Check point proximity (MaskPoint)
            float bestDist = radius;
            int bestIdx = -1;
            for (int i = 0; i < n; ++i) {
                float d = distVec2(pos, points[i]);
                if (d < bestDist) {
                    bestDist = d;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0) {
                hr.hit = true;
                hr.surfaceId = id;  // reuse surfaceId for mask id
                hr.handleKind = HandleKind::MaskPoint;
                hr.handleIndex = bestIdx;
                return hr;
            }

            // Check edge proximity (MaskEdge)
            bestDist = radius;
            bestIdx = -1;
            for (int i = 0; i < n; ++i) {
                int j = (i + 1) % n;
                float d = distToSegment(pos, points[i], points[j]);
                if (d < bestDist) {
                    bestDist = d;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0) {
                hr.hit = true;
                hr.surfaceId = id;
                hr.handleKind = HandleKind::MaskEdge;
                hr.handleIndex = bestIdx;
                return hr;
            }

            // Check body (inside polygon)
            if (n >= 3 && pointInPolygon(points, pos)) {
                hr.hit = true;
                hr.surfaceId = id;
                hr.handleKind = HandleKind::Body;
                return hr;
            }

            break;
        }

        // ----- Rectangle -----
        case MaskKind::Rectangle: {
            if (rect.w <= 0 || rect.h <= 0) return hr;

            // Check corner proximity (4 corners as MaskPoint handles)
            Vec2 corners[4] = {
                Vec2(rect.x, rect.y),
                Vec2(rect.x + rect.w, rect.y),
                Vec2(rect.x + rect.w, rect.y + rect.h),
                Vec2(rect.x, rect.y + rect.h)
            };
            for (int i = 0; i < 4; ++i) {
                if (distVec2(pos, corners[i]) < radius) {
                    hr.hit = true;
                    hr.surfaceId = id;
                    hr.handleKind = HandleKind::MaskPoint;
                    hr.handleIndex = i;
                    return hr;
                }
            }

            // Check edge proximity
            for (int i = 0; i < 4; ++i) {
                int j = (i + 1) % 4;
                if (distToSegment(pos, corners[i], corners[j]) < radius) {
                    hr.hit = true;
                    hr.surfaceId = id;
                    hr.handleKind = HandleKind::MaskEdge;
                    hr.handleIndex = i;
                    return hr;
                }
            }

            // Check body (inside rectangle)
            if (pos.x >= rect.x && pos.x <= rect.x + rect.w &&
                pos.y >= rect.y && pos.y <= rect.y + rect.h) {
                hr.hit = true;
                hr.surfaceId = id;
                hr.handleKind = HandleKind::Body;
                return hr;
            }

            break;
        }

        // ----- Ellipse -----
        case MaskKind::Ellipse: {
            if (rect.w <= 0 || rect.h <= 0) return hr;

            float cx = rect.x + rect.w * 0.5f;
            float cy = rect.y + rect.h * 0.5f;
            float rx = rect.w * 0.5f;
            float ry = rect.h * 0.5f;

            // Control points: center + 4 cardinal points
            Vec2 handles[5] = {
                Vec2(cx, cy),                 // center
                Vec2(cx + rx, cy),            // right
                Vec2(cx, cy + ry),            // bottom
                Vec2(cx - rx, cy),            // left
                Vec2(cx, cy - ry)             // top
            };
            for (int i = 0; i < 5; ++i) {
                if (distVec2(pos, handles[i]) < radius) {
                    hr.hit = true;
                    hr.surfaceId = id;
                    hr.handleKind = HandleKind::MaskPoint;
                    hr.handleIndex = i;
                    return hr;
                }
            }

            // Check body (inside ellipse)
            if (pointInEllipse(pos, rect)) {
                hr.hit = true;
                hr.surfaceId = id;
                hr.handleKind = HandleKind::Body;
                return hr;
            }

            break;
        }

        // ----- Alpha Texture -----
        case MaskKind::AlphaTexture: {
            // Alpha texture masks don't have editable control points.
            // Body hit only (full bounding rect)
            if (rect.w > 0 && rect.h > 0 &&
                pos.x >= rect.x && pos.x <= rect.x + rect.w &&
                pos.y >= rect.y && pos.y <= rect.y + rect.h) {
                hr.hit = true;
                hr.surfaceId = id;
                hr.handleKind = HandleKind::Body;
                return hr;
            }
            break;
        }
    }

    return hr;
}

// ===========================================================================
// Localized name helpers
// ===========================================================================

std::string MapWrapMask::kindName() const {
    switch (kind) {
        case MaskKind::Rectangle:   return MapWrapI18n::instance().tr("mask.rectangle");
        case MaskKind::Ellipse:     return MapWrapI18n::instance().tr("mask.ellipse");
        case MaskKind::Polygon:     return MapWrapI18n::instance().tr("mask.polygon");
        case MaskKind::Bezier:      return MapWrapI18n::instance().tr("mask.bezier");
        case MaskKind::Freehand:    return MapWrapI18n::instance().tr("mask.freehand");
        case MaskKind::AlphaTexture: return MapWrapI18n::instance().tr("mask.alpha_texture");
    }
    return "";
}

std::string MapWrapMask::operationName() const {
    switch (operation) {
        case MaskOperation::Add:       return MapWrapI18n::instance().tr("mask.add");
        case MaskOperation::Subtract:  return MapWrapI18n::instance().tr("mask.subtract");
        case MaskOperation::Intersect: return MapWrapI18n::instance().tr("mask.intersect");
    }
    return "";
}

std::string MapWrapMask::spaceName() const {
    switch (space) {
        case MaskSpace::SurfaceLocal: return MapWrapI18n::instance().tr("mask.space.surface_local");
        case MaskSpace::SourceUV:     return MapWrapI18n::instance().tr("mask.space.source_uv");
        case MaskSpace::Canvas:       return MapWrapI18n::instance().tr("mask.space.canvas");
        case MaskSpace::Output:       return MapWrapI18n::instance().tr("mask.space.output");
    }
    return "";
}

} // namespace mapwrap
} // namespace tcx
