// =============================================================================
// tcxMapWrap — MapWrapEditor.cpp Full Implementation
// =============================================================================
// Interactive editor for projection mapping surfaces. Handles pointer events,
// hit testing, drag-to-move/resize, selection, undo/redo, nudge/align/fit,
// clipboard, and property inspection.

#include "tcxMapWrap/MapWrapEditor.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/SurfaceQuad.h"
#include "tcxMapWrap/SurfaceGrid.h"
#include "tcxMapWrap/SurfaceBezier.h"
#include "tcxMapWrap/SurfaceTriangle.h"
#include "tcxMapWrap/SurfaceCircle.h"
#include "tcxMapWrap/SurfacePolygon.h"
#include "tcxMapWrap/UndoStack.h"
#include "tcxMapWrap/HitTestIndex.h"
#include "tcxMapWrap/MapWrapI18n.h"

#include <cmath>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace tcx {
namespace mapwrap {

// ===========================================================================
// Section 2: Internal types
// ===========================================================================

/// Captures the editable geometry of any surface type for undo/redo.
struct SurfaceSnapshot {
    std::vector<Vec2> destPoints;   // Quad:4, Triangle:3, Polygon:N
    std::vector<Vec2> uvPoints;     // same count as destPoints
    bool perspectiveCorrection = false;
    // Circle-specific
    Vec2 center;
    float radiusX = 0;
    float radiusY = 0;
    float rotation = 0;
    // Grid/Bezier-specific
    int cols = 0;
    int rows = 0;
    std::vector<Vec2> gridPoints;
    int meshResolution = 0;
};

// Forward declarations for helpers used by command classes
static SurfaceSnapshot takeSnapshot(const Surface& surface);
static void restoreSnapshot(Surface& surface, const SurfaceSnapshot& snap);
static bool snapshotEqualsApprox(const SurfaceSnapshot& a, const SurfaceSnapshot& b, float eps = 1e-6f);
static std::string applyProperty(Surface& surface, const std::string& path,
                                  const std::string& value);

static std::shared_ptr<Surface> cloneSurfaceShared(const std::shared_ptr<Surface>& surface) {
    if (!surface) return nullptr;
    std::unique_ptr<Surface> cloned = surface->clone();
    return std::shared_ptr<Surface>(std::move(cloned));
}

static bool isEffectivelyLocked(const MapWrapDocument& document, const Surface& surface) {
    if (surface.isLocked()) return true;
    for (const auto& group : document.groups()) {
        if (!group) continue;
        if (!group->locked) continue;
        if (std::find(group->surfaceIds.begin(), group->surfaceIds.end(), surface.id()) !=
            group->surfaceIds.end()) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Undo commands
// ---------------------------------------------------------------------------

class SurfaceEditCommand : public Command {
public:
    SurfaceEditCommand(MapWrapDocument* doc, const SurfaceId& id,
                       SurfaceSnapshot before, SurfaceSnapshot after,
                       const std::string& desc)
        : doc_(doc), id_(id), before_(std::move(before)),
          after_(std::move(after)), desc_(desc) {}

    void execute() override {
        auto s = doc_->getSurface(id_);
        if (s) {
            restoreSnapshot(*s, after_);
            doc_->markDirty();
        }
    }

    void undo() override {
        auto s = doc_->getSurface(id_);
        if (s) {
            restoreSnapshot(*s, before_);
            doc_->markDirty();
        }
    }

    std::string description() const override { return desc_; }

private:
    MapWrapDocument* doc_;
    SurfaceId id_;
    SurfaceSnapshot before_;
    SurfaceSnapshot after_;
    std::string desc_;
};

class EditorDeleteSurfaceCommand : public Command {
public:
    EditorDeleteSurfaceCommand(MapWrapDocument* doc,
                               std::shared_ptr<Surface> surface, int originalIndex)
        : doc_(doc), surface_(cloneSurfaceShared(surface)), originalIndex_(originalIndex) {}

    void execute() override {
        if (surface_) doc_->removeSurface(surface_->id());
    }

    void undo() override {
        if (surface_) doc_->insertSurface(surface_, originalIndex_);
    }

    std::string description() const override {
        return tr("command.delete_surface");
    }

private:
    MapWrapDocument* doc_;
    std::shared_ptr<Surface> surface_;
    int originalIndex_;
};

class CreateSurfaceCommand : public Command {
public:
    CreateSurfaceCommand(MapWrapDocument* doc,
                        std::shared_ptr<Surface> surface, int insertIndex)
        : doc_(doc), surface_(std::move(surface)), insertIndex_(insertIndex) {}

    void execute() override {
        if (surface_) doc_->insertSurface(surface_, insertIndex_);
    }

    void undo() override {
        if (surface_) doc_->removeSurface(surface_->id());
    }

    std::string description() const override {
        return tr("command.create_surface");
    }

private:
    MapWrapDocument* doc_;
    std::shared_ptr<Surface> surface_;
    int insertIndex_;
};

class ReorderCommand : public Command {
public:
    ReorderCommand(MapWrapDocument* doc, const SurfaceId& id,
                   int fromIndex, int toIndex, const std::string& desc)
        : doc_(doc), id_(id), fromIndex_(fromIndex),
          toIndex_(toIndex), desc_(desc) {}

    void execute() override {
        doc_->reorderSurface(id_, toIndex_);
    }

    void undo() override {
        doc_->reorderSurface(id_, fromIndex_);
    }

    std::string description() const override { return desc_; }

private:
    MapWrapDocument* doc_;
    SurfaceId id_;
    int fromIndex_;
    int toIndex_;
    std::string desc_;
};

class ReplaceSurfaceCommand : public Command {
public:
    ReplaceSurfaceCommand(MapWrapDocument* doc,
                          std::shared_ptr<Surface> before,
                          std::shared_ptr<Surface> after,
                          int index,
                          std::string desc)
        : doc_(doc), before_(std::move(before)), after_(std::move(after)),
          index_(index), desc_(std::move(desc)) {}

    void execute() override {
        if (!doc_ || !after_) return;
        if (before_) doc_->removeSurface(before_->id());
        doc_->insertSurface(after_, index_);
    }

    void undo() override {
        if (!doc_ || !before_) return;
        if (after_) doc_->removeSurface(after_->id());
        doc_->insertSurface(before_, index_);
    }

    std::string description() const override { return desc_; }

private:
    MapWrapDocument* doc_;
    std::shared_ptr<Surface> before_;
    std::shared_ptr<Surface> after_;
    int index_ = 0;
    std::string desc_;
};

class PropertyEditCommand : public Command {
public:
    PropertyEditCommand(MapWrapDocument* doc, const SurfaceId& id,
                        const std::string& path,
                        const std::string& oldValue, const std::string& newValue,
                        const std::string& desc)
        : doc_(doc), id_(id), path_(path),
          oldValue_(oldValue), newValue_(newValue), desc_(desc) {}

    void execute() override;
    void undo() override;

    std::string description() const override { return desc_; }

private:
    MapWrapDocument* doc_;
    SurfaceId id_;
    std::string path_;
    std::string oldValue_;
    std::string newValue_;
    std::string desc_;
};

// ===========================================================================
// Section 3: Helper functions
// ===========================================================================

static SurfaceSnapshot takeSnapshot(const Surface& surface) {
    SurfaceSnapshot snap;
    switch (surface.kind()) {
        case SurfaceKind::Quad: {
            auto& q = static_cast<const SurfaceQuad&>(surface);
            auto& dp = q.destinationPoints();
            auto& uv = q.uvPoints();
            for (int i = 0; i < 4; ++i) {
                snap.destPoints.push_back(dp[i]);
                snap.uvPoints.push_back(uv[i]);
            }
            snap.perspectiveCorrection = q.perspectiveCorrection();
            snap.meshResolution = q.meshResolution();
            break;
        }
        case SurfaceKind::Triangle: {
            auto& t = static_cast<const SurfaceTriangle&>(surface);
            auto& dp = t.destinationPoints();
            auto& uv = t.uvPoints();
            for (int i = 0; i < 3; ++i) {
                snap.destPoints.push_back(dp[i]);
                snap.uvPoints.push_back(uv[i]);
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<const SurfaceCircle&>(surface);
            snap.center = c.center();
            snap.radiusX = c.radiusX();
            snap.radiusY = c.radiusY();
            snap.rotation = c.rotation();
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<const SurfaceGrid&>(surface);
            snap.cols = g.cols();
            snap.rows = g.rows();
            snap.meshResolution = g.meshResolution();
            for (int r = 0; r <= g.rows(); ++r)
                for (int c = 0; c <= g.cols(); ++c)
                    snap.gridPoints.push_back(g.gridPoint(c, r));
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<const SurfaceBezier&>(surface);
            snap.cols = b.controlCols();
            snap.rows = b.controlRows();
            snap.meshResolution = b.meshResolution();
            for (int r = 0; r < b.controlRows(); ++r)
                for (int c = 0; c < b.controlCols(); ++c)
                    snap.gridPoints.push_back(b.controlPoint(c, r));
            break;
        }
        case SurfaceKind::Polygon: {
            auto& p = static_cast<const SurfacePolygon&>(surface);
            snap.destPoints = p.destinationPoints();
            snap.uvPoints = p.uvPoints();
            break;
        }
    }
    return snap;
}

static void restoreSnapshot(Surface& surface, const SurfaceSnapshot& snap) {
    switch (surface.kind()) {
        case SurfaceKind::Quad: {
            auto& q = static_cast<SurfaceQuad&>(surface);
            auto& dp = q.destinationPoints();
            auto& uv = q.uvPoints();
            for (int i = 0; i < 4 && i < (int)snap.destPoints.size(); ++i)
                dp[i] = snap.destPoints[i];
            for (int i = 0; i < 4 && i < (int)snap.uvPoints.size(); ++i)
                uv[i] = snap.uvPoints[i];
            q.setPerspectiveCorrection(snap.perspectiveCorrection);
            if (snap.meshResolution > 0)
                q.setMeshResolution(snap.meshResolution);
            break;
        }
        case SurfaceKind::Triangle: {
            auto& t = static_cast<SurfaceTriangle&>(surface);
            auto& dp = t.destinationPoints();
            auto& uv = t.uvPoints();
            for (int i = 0; i < 3 && i < (int)snap.destPoints.size(); ++i)
                dp[i] = snap.destPoints[i];
            for (int i = 0; i < 3 && i < (int)snap.uvPoints.size(); ++i)
                uv[i] = snap.uvPoints[i];
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(surface);
            c.setCenter(snap.center);
            c.setRadiusX(snap.radiusX);
            c.setRadiusY(snap.radiusY);
            c.setRotation(snap.rotation);
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<SurfaceGrid&>(surface);
            if (snap.cols > 0 && g.cols() != snap.cols)
                g.setCols(snap.cols);
            if (snap.rows > 0 && g.rows() != snap.rows)
                g.setRows(snap.rows);
            int idx = 0;
            for (int r = 0; r <= snap.rows; ++r)
                for (int cc = 0; cc <= snap.cols; ++cc, ++idx)
                    if (idx < (int)snap.gridPoints.size())
                        g.setGridPoint(cc, r, snap.gridPoints[idx]);
            if (snap.meshResolution > 0)
                g.setMeshResolution(snap.meshResolution);
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<SurfaceBezier&>(surface);
            if (snap.cols > 1 && snap.rows > 1)
                b.setControlDimensions(snap.cols, snap.rows);
            int idx = 0;
            for (int r = 0; r < snap.rows; ++r)
                for (int cc = 0; cc < snap.cols; ++cc, ++idx)
                    if (idx < (int)snap.gridPoints.size())
                        b.setControlPoint(cc, r, snap.gridPoints[idx]);
            if (snap.meshResolution > 0)
                b.setMeshResolution(snap.meshResolution);
            break;
        }
        case SurfaceKind::Polygon: {
            auto& p = static_cast<SurfacePolygon&>(surface);
            p.setDestinationPoints(snap.destPoints);
            p.setUvPoints(snap.uvPoints);
            break;
        }
    }
    surface.markDirty();
}

static bool vecListEqualsApprox(const std::vector<Vec2>& a, const std::vector<Vec2>& b, float eps) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!nearlyEqual(a[i], b[i], eps)) return false;
    }
    return true;
}

static bool snapshotEqualsApprox(const SurfaceSnapshot& a, const SurfaceSnapshot& b, float eps) {
    return vecListEqualsApprox(a.destPoints, b.destPoints, eps) &&
           vecListEqualsApprox(a.uvPoints, b.uvPoints, eps) &&
           a.perspectiveCorrection == b.perspectiveCorrection &&
           nearlyEqual(a.center, b.center, eps) &&
           nearlyEqual(a.radiusX, b.radiusX, eps) &&
           nearlyEqual(a.radiusY, b.radiusY, eps) &&
           nearlyEqual(a.rotation, b.rotation, eps) &&
           a.cols == b.cols &&
           a.rows == b.rows &&
           vecListEqualsApprox(a.gridPoints, b.gridPoints, eps) &&
           a.meshResolution == b.meshResolution;
}

/// Compute axis-aligned bounding box of a surface in canvas-normalized coords.
static Rect surfaceBounds(const Surface& surface) {
    switch (surface.kind()) {
        case SurfaceKind::Quad: {
            auto& pts = static_cast<const SurfaceQuad&>(surface).destinationPoints();
            float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
            for (int i = 1; i < 4; ++i) {
                minX = std::min(minX, pts[i].x); maxX = std::max(maxX, pts[i].x);
                minY = std::min(minY, pts[i].y); maxY = std::max(maxY, pts[i].y);
            }
            return Rect(minX, minY, maxX - minX, maxY - minY);
        }
        case SurfaceKind::Triangle: {
            auto& pts = static_cast<const SurfaceTriangle&>(surface).destinationPoints();
            float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
            for (int i = 1; i < 3; ++i) {
                minX = std::min(minX, pts[i].x); maxX = std::max(maxX, pts[i].x);
                minY = std::min(minY, pts[i].y); maxY = std::max(maxY, pts[i].y);
            }
            return Rect(minX, minY, maxX - minX, maxY - minY);
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<const SurfaceCircle&>(surface);
            return Rect(c.center().x - c.radiusX(), c.center().y - c.radiusY(),
                        c.radiusX() * 2.0f, c.radiusY() * 2.0f);
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<const SurfaceGrid&>(surface);
            if (g.cols() < 1 || g.rows() < 1) return Rect(0, 0, 0, 0);
            Vec2 p0 = g.gridPoint(0, 0);
            float minX = p0.x, maxX = p0.x, minY = p0.y, maxY = p0.y;
            for (int r = 0; r <= g.rows(); ++r) {
                for (int c = 0; c <= g.cols(); ++c) {
                    Vec2 p = g.gridPoint(c, r);
                    minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                    minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
                }
            }
            return Rect(minX, minY, maxX - minX, maxY - minY);
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<const SurfaceBezier&>(surface);
            if (b.controlPoints().empty()) return Rect(0, 0, 0, 0);
            Vec2 p0 = b.controlPoints().front();
            float minX = p0.x, maxX = p0.x, minY = p0.y, maxY = p0.y;
            for (const auto& p : b.controlPoints()) {
                minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
            }
            return Rect(minX, minY, maxX - minX, maxY - minY);
        }
        case SurfaceKind::Polygon: {
            auto& pts = static_cast<const SurfacePolygon&>(surface).destinationPoints();
            if (pts.empty()) return Rect(0, 0, 0, 0);
            float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
            for (size_t i = 1; i < pts.size(); ++i) {
                minX = std::min(minX, pts[i].x); maxX = std::max(maxX, pts[i].x);
                minY = std::min(minY, pts[i].y); maxY = std::max(maxY, pts[i].y);
            }
            return Rect(minX, minY, maxX - minX, maxY - minY);
        }
    }
    return Rect(0, 0, 0, 0);
}

static std::string lowerAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

static const char* surfaceKindToPropertyString(SurfaceKind kind) {
    switch (kind) {
        case SurfaceKind::Quad:     return "quad";
        case SurfaceKind::Grid:     return "grid";
        case SurfaceKind::Bezier:   return "bezier";
        case SurfaceKind::Triangle: return "triangle";
        case SurfaceKind::Circle:   return "circle";
        case SurfaceKind::Polygon:  return "polygon";
    }
    return "quad";
}

static bool surfaceKindFromPropertyString(const std::string& value, SurfaceKind& kind) {
    std::string s = lowerAscii(value);
    if (s == "quad" || s == "perspective") { kind = SurfaceKind::Quad; return true; }
    if (s == "grid" || s == "bilinear") { kind = SurfaceKind::Grid; return true; }
    if (s == "bezier" || s == "bezier_surface") { kind = SurfaceKind::Bezier; return true; }
    if (s == "triangle") { kind = SurfaceKind::Triangle; return true; }
    if (s == "circle") { kind = SurfaceKind::Circle; return true; }
    if (s == "polygon") { kind = SurfaceKind::Polygon; return true; }
    return false;
}

static Vec2 bilinearPoint(const std::array<Vec2, 4>& corners, float u, float v) {
    return Vec2(
        (1 - u) * (1 - v) * corners[0].x + u * (1 - v) * corners[1].x +
        u * v * corners[2].x + (1 - u) * v * corners[3].x,
        (1 - u) * (1 - v) * corners[0].y + u * (1 - v) * corners[1].y +
        u * v * corners[2].y + (1 - u) * v * corners[3].y);
}

static Vec2 sampleGrid(const SurfaceGrid& grid, float u, float v) {
    u = std::max(0.0f, std::min(1.0f, u));
    v = std::max(0.0f, std::min(1.0f, v));
    float gx = u * float(grid.cols());
    float gy = v * float(grid.rows());
    int c0 = std::min(int(std::floor(gx)), grid.cols() - 1);
    int r0 = std::min(int(std::floor(gy)), grid.rows() - 1);
    int c1 = std::min(c0 + 1, grid.cols());
    int r1 = std::min(r0 + 1, grid.rows());
    float tx = gx - float(c0);
    float ty = gy - float(r0);
    std::array<Vec2, 4> cell = {{
        grid.gridPoint(c0, r0),
        grid.gridPoint(c1, r0),
        grid.gridPoint(c1, r1),
        grid.gridPoint(c0, r1)
    }};
    return bilinearPoint(cell, tx, ty);
}

static std::array<Vec2, 4> surfaceCornerPoints(const Surface& surface) {
    switch (surface.kind()) {
        case SurfaceKind::Quad:
            return static_cast<const SurfaceQuad&>(surface).destinationPoints();
        case SurfaceKind::Grid: {
            const auto& g = static_cast<const SurfaceGrid&>(surface);
            return {{ g.gridPoint(0, 0),
                      g.gridPoint(g.cols(), 0),
                      g.gridPoint(g.cols(), g.rows()),
                      g.gridPoint(0, g.rows()) }};
        }
        case SurfaceKind::Bezier: {
            const auto& b = static_cast<const SurfaceBezier&>(surface);
            return {{ b.controlPoint(0, 0),
                      b.controlPoint(b.controlCols() - 1, 0),
                      b.controlPoint(b.controlCols() - 1, b.controlRows() - 1),
                      b.controlPoint(0, b.controlRows() - 1) }};
        }
        default: {
            Rect r = surfaceBounds(surface);
            return {{ Vec2(r.x, r.y),
                      Vec2(r.x + r.w, r.y),
                      Vec2(r.x + r.w, r.y + r.h),
                      Vec2(r.x, r.y + r.h) }};
        }
    }
}

static Vec2 sampleSurfaceControl(const Surface& surface, float u, float v) {
    switch (surface.kind()) {
        case SurfaceKind::Grid:
            return sampleGrid(static_cast<const SurfaceGrid&>(surface), u, v);
        case SurfaceKind::Bezier:
            return static_cast<const SurfaceBezier&>(surface).evaluate(u, v);
        default:
            return bilinearPoint(surfaceCornerPoints(surface), u, v);
    }
}

static void copyCommonSurfaceState(const Surface& src, Surface& dst) {
    dst.setId(src.id());
    dst.setName(src.name());
    dst.setVisible(src.isVisible());
    dst.setLocked(src.isLocked());
    dst.setOpacity(src.opacity());
    dst.setSource(src.source());
    dst.setSourceRect(src.sourceRect());
    dst.setBlend(src.blend());
    dst.setColorCorrection(src.colorCorrection());
    dst.masks() = src.masks();
}

static std::shared_ptr<Surface> convertSurfaceToKind(const Surface& src, SurfaceKind targetKind) {
    std::shared_ptr<Surface> converted;

    switch (targetKind) {
        case SurfaceKind::Quad: {
            auto q = std::make_shared<SurfaceQuad>();
            q->destinationPoints() = surfaceCornerPoints(src);
            if (src.kind() == SurfaceKind::Quad) {
                const auto& old = static_cast<const SurfaceQuad&>(src);
                q->uvPoints() = old.uvPoints();
                q->setPerspectiveCorrection(old.perspectiveCorrection());
                q->setMeshResolution(old.meshResolution());
            }
            converted = q;
            break;
        }
        case SurfaceKind::Grid: {
            int cols = 4;
            int rows = 4;
            int meshRes = 4;
            bool curved = false;
            if (src.kind() == SurfaceKind::Grid) {
                const auto& old = static_cast<const SurfaceGrid&>(src);
                cols = old.cols();
                rows = old.rows();
                meshRes = old.meshResolution();
                curved = old.curvedInterpolation();
            }
            auto g = std::make_shared<SurfaceGrid>(cols, rows);
            for (int row = 0; row <= rows; ++row) {
                float v = float(row) / float(rows);
                for (int col = 0; col <= cols; ++col) {
                    float u = float(col) / float(cols);
                    g->setGridPoint(col, row, sampleSurfaceControl(src, u, v));
                }
            }
            g->setCurvedInterpolation(curved);
            g->setMeshResolution(meshRes);
            converted = g;
            break;
        }
        case SurfaceKind::Bezier: {
            int cols = 4;
            int rows = 4;
            int meshRes = 28;
            if (src.kind() == SurfaceKind::Bezier) {
                const auto& old = static_cast<const SurfaceBezier&>(src);
                cols = old.controlCols();
                rows = old.controlRows();
                meshRes = old.meshResolution();
            }
            auto b = std::make_shared<SurfaceBezier>(cols, rows);
            for (int row = 0; row < rows; ++row) {
                float v = rows <= 1 ? 0.0f : float(row) / float(rows - 1);
                for (int col = 0; col < cols; ++col) {
                    float u = cols <= 1 ? 0.0f : float(col) / float(cols - 1);
                    b->setControlPoint(col, row, sampleSurfaceControl(src, u, v));
                }
            }
            b->setMeshResolution(meshRes);
            converted = b;
            break;
        }
        case SurfaceKind::Triangle: {
            auto t = std::make_shared<SurfaceTriangle>();
            auto corners = surfaceCornerPoints(src);
            auto& pts = t->destinationPoints();
            pts[0] = Vec2((corners[0].x + corners[1].x) * 0.5f,
                          (corners[0].y + corners[1].y) * 0.5f);
            pts[1] = corners[2];
            pts[2] = corners[3];
            converted = t;
            break;
        }
        case SurfaceKind::Circle: {
            Rect r = surfaceBounds(src);
            auto c = std::make_shared<SurfaceCircle>();
            c->setCenter(Vec2(r.x + r.w * 0.5f, r.y + r.h * 0.5f));
            c->setRadiusX(std::max(0.001f, r.w * 0.5f));
            c->setRadiusY(std::max(0.001f, r.h * 0.5f));
            converted = c;
            break;
        }
        case SurfaceKind::Polygon: {
            auto p = std::make_shared<SurfacePolygon>();
            for (const auto& point : surfaceCornerPoints(src)) {
                p->addPoint(point);
            }
            converted = p;
            break;
        }
    }

    if (converted) copyCommonSurfaceState(src, *converted);
    return converted;
}

/// Translate all points of a surface by a delta in canvas-normalized coords.
static void moveSurface(Surface& surface, Vec2 delta) {
    switch (surface.kind()) {
        case SurfaceKind::Quad: {
            auto& dp = static_cast<SurfaceQuad&>(surface).destinationPoints();
            for (int i = 0; i < 4; ++i)
                dp[i] = Vec2(dp[i].x + delta.x, dp[i].y + delta.y);
            break;
        }
        case SurfaceKind::Triangle: {
            auto& dp = static_cast<SurfaceTriangle&>(surface).destinationPoints();
            for (int i = 0; i < 3; ++i)
                dp[i] = Vec2(dp[i].x + delta.x, dp[i].y + delta.y);
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(surface);
            c.setCenter(Vec2(c.center().x + delta.x, c.center().y + delta.y));
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<SurfaceGrid&>(surface);
            for (int r = 0; r <= g.rows(); ++r)
                for (int c = 0; c <= g.cols(); ++c) {
                    Vec2 p = g.gridPoint(c, r);
                    g.setGridPoint(c, r, Vec2(p.x + delta.x, p.y + delta.y));
            }
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<SurfaceBezier&>(surface);
            for (int r = 0; r < b.controlRows(); ++r)
                for (int c = 0; c < b.controlCols(); ++c) {
                    Vec2 p = b.controlPoint(c, r);
                    b.setControlPoint(c, r, Vec2(p.x + delta.x, p.y + delta.y));
                }
            break;
        }
        case SurfaceKind::Polygon: {
            auto& dp = static_cast<SurfacePolygon&>(surface).destinationPoints();
            for (auto& p : dp)
                p = Vec2(p.x + delta.x, p.y + delta.y);
            break;
        }
    }
    surface.markDirty();
}

/// Move a single vertex handle by delta. If uvMode, move the UV point instead.
static void moveHandleVertex(Surface& surface, int index, Vec2 delta, bool uvMode) {
    switch (surface.kind()) {
        case SurfaceKind::Quad: {
            auto& q = static_cast<SurfaceQuad&>(surface);
            if (uvMode) {
                auto& uv = q.uvPoints();
                if (index >= 0 && index < 4)
                    uv[index] = Vec2(uv[index].x + delta.x, uv[index].y + delta.y);
            } else {
                auto& dp = q.destinationPoints();
                if (index >= 0 && index < 4)
                    dp[index] = Vec2(dp[index].x + delta.x, dp[index].y + delta.y);
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& t = static_cast<SurfaceTriangle&>(surface);
            if (uvMode) {
                auto& uv = t.uvPoints();
                if (index >= 0 && index < 3)
                    uv[index] = Vec2(uv[index].x + delta.x, uv[index].y + delta.y);
            } else {
                auto& dp = t.destinationPoints();
                if (index >= 0 && index < 3)
                    dp[index] = Vec2(dp[index].x + delta.x, dp[index].y + delta.y);
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(surface);
            if (!uvMode) {
                if (index == 0) {
                    c.setCenter(Vec2(c.center().x + delta.x,
                                     c.center().y + delta.y));
                } else if (index == 1) {
                    // RadiusX handle: project delta onto rotated X axis
                    float rotRad = degToRad(c.rotation());
                    float ax = std::cos(rotRad), ay = std::sin(rotRad);
                    float proj = delta.x * ax + delta.y * ay;
                    c.setRadiusX(std::max(0.001f, c.radiusX() + proj));
                } else if (index == 2) {
                    // RadiusY handle: project delta onto rotated Y axis
                    float rotRad = degToRad(c.rotation());
                    float ax = -std::sin(rotRad), ay = std::cos(rotRad);
                    float proj = delta.x * ax + delta.y * ay;
                    c.setRadiusY(std::max(0.001f, c.radiusY() + proj));
                }
            }
            break;
        }
        case SurfaceKind::Polygon: {
            auto& p = static_cast<SurfacePolygon&>(surface);
            if (uvMode) {
                auto& uv = p.uvPoints();
                if (index >= 0 && index < (int)uv.size())
                    uv[index] = Vec2(uv[index].x + delta.x, uv[index].y + delta.y);
            } else {
                auto& dp = p.destinationPoints();
                if (index >= 0 && index < (int)dp.size())
                    dp[index] = Vec2(dp[index].x + delta.x, dp[index].y + delta.y);
            }
            break;
        }
        case SurfaceKind::Grid:
            // Grid uses GridPoint handles, not Vertex
            break;
        case SurfaceKind::Bezier:
            // Bezier uses GridPoint handles for its control lattice.
            break;
    }
    surface.markDirty();
}

/// Move a grid point handle by delta.
static void moveGridPointHandle(Surface& surface, int index, Vec2 delta) {
    if (surface.kind() == SurfaceKind::Grid) {
        auto& g = static_cast<SurfaceGrid&>(surface);
        int stride = g.cols() + 1;
        int r = index / stride;
        int c = index % stride;
        if (r >= 0 && r <= g.rows() && c >= 0 && c <= g.cols()) {
            Vec2 p = g.gridPoint(c, r);
            g.setGridPoint(c, r, Vec2(p.x + delta.x, p.y + delta.y));
        }
    } else if (surface.kind() == SurfaceKind::Bezier) {
        auto& b = static_cast<SurfaceBezier&>(surface);
        int stride = b.controlCols();
        int r = index / stride;
        int c = index % stride;
        if (r >= 0 && r < b.controlRows() && c >= 0 && c < b.controlCols()) {
            Vec2 p = b.controlPoint(c, r);
            b.setControlPoint(c, r, Vec2(p.x + delta.x, p.y + delta.y));
        }
    }
}

/// Rotate a circle surface so it points toward canvasNorm.
static void rotateCircleSurface(Surface& surface, Vec2 canvasNorm) {
    if (surface.kind() != SurfaceKind::Circle) return;
    auto& c = static_cast<SurfaceCircle&>(surface);
    Vec2 ctr = c.center();
    float angle = std::atan2(canvasNorm.y - ctr.y, canvasNorm.x - ctr.x);
    c.setRotation(radToDeg(angle));
}

/// Apply snapping to a position in canvas-normalized coordinates.
static Vec2 applySnap(Vec2 pos, const SnapSettings& settings,
                      const EditorViewport& viewport) {
    if (!settings.enabled) return pos;

    Vec2 snapped = pos;
    float canvasScale = viewport.canvasSizePixels.x * viewport.zoom;
    float threshold = settings.thresholdPixels / canvasScale;

    if (settings.snapToGrid && settings.gridStepNorm > 0) {
        float step = settings.gridStepNorm;
        float gx = std::round(pos.x / step) * step;
        float gy = std::round(pos.y / step) * step;
        if (std::fabs(pos.x - gx) < threshold) snapped.x = gx;
        if (std::fabs(pos.y - gy) < threshold) snapped.y = gy;
    }

    if (settings.snapToCanvasEdges) {
        if (std::fabs(pos.x) < threshold)         snapped.x = 0.0f;
        if (std::fabs(pos.x - 1.0f) < threshold)  snapped.x = 1.0f;
        if (std::fabs(pos.y) < threshold)         snapped.y = 0.0f;
        if (std::fabs(pos.y - 1.0f) < threshold)  snapped.y = 1.0f;
    }

    if (settings.snapToCanvasCenter) {
        if (std::fabs(pos.x - 0.5f) < threshold) snapped.x = 0.5f;
        if (std::fabs(pos.y - 0.5f) < threshold) snapped.y = 0.5f;
    }

    return snapped;
}

/// Simple value parsing helpers for property system.
static float parseFloat(const std::string& s) {
    try { return std::stof(s); } catch (...) { return 0.0f; }
}
static int parseInt(const std::string& s) {
    try { return std::stoi(s); } catch (...) { return 0; }
}
static bool parseBool(const std::string& s) {
    return s == "true" || s == "1";
}
static Vec2 parseVec2(const std::string& s) {
    auto comma = s.find(',');
    if (comma == std::string::npos) return Vec2(parseFloat(s), 0);
    return Vec2(parseFloat(s.substr(0, comma)),
                parseFloat(s.substr(comma + 1)));
}

static std::string formatFloat(float v) {
    std::ostringstream ss;
    ss << v;
    return ss.str();
}
static std::string formatInt(int v) {
    return std::to_string(v);
}
static std::string formatBool(bool v) {
    return v ? "true" : "false";
}
static std::string formatVec2(Vec2 v) {
    return formatFloat(v.x) + "," + formatFloat(v.y);
}

/// Apply a single property change to a surface.  Returns the old value.
static std::string applyProperty(Surface& surface, const std::string& path,
                                  const std::string& value) {
    // Common properties
    if (path == "name") {
        std::string old = surface.name();
        surface.setName(value);
        return old;
    }
    if (path == "visible") {
        bool old = surface.isVisible();
        surface.setVisible(parseBool(value));
        return formatBool(old);
    }
    if (path == "locked") {
        bool old = surface.isLocked();
        surface.setLocked(parseBool(value));
        return formatBool(old);
    }
    if (path == "opacity") {
        float old = surface.opacity();
        surface.setOpacity(parseFloat(value));
        return formatFloat(old);
    }

    // Blend
    if (path == "blend.enabled") {
        bool old = surface.blend().enabled;
        auto b = surface.blend(); b.enabled = parseBool(value);
        surface.setBlend(b);
        return formatBool(old);
    }
    if (path == "blend.opacity") {
        float old = surface.blend().opacity;
        auto b = surface.blend(); b.opacity = parseFloat(value);
        surface.setBlend(b);
        return formatFloat(old);
    }
    if (path == "blend.brightness") {
        float old = surface.blend().brightness;
        auto b = surface.blend(); b.brightness = parseFloat(value);
        surface.setBlend(b);
        return formatFloat(old);
    }

    // Color correction
    if (path == "colorCorrection.enabled") {
        bool old = surface.colorCorrection().enabled;
        auto cc = surface.colorCorrection(); cc.enabled = parseBool(value);
        surface.setColorCorrection(cc);
        return formatBool(old);
    }
    if (path == "colorCorrection.brightness") {
        float old = surface.colorCorrection().brightness;
        auto cc = surface.colorCorrection(); cc.brightness = parseFloat(value);
        surface.setColorCorrection(cc);
        return formatFloat(old);
    }
    if (path == "colorCorrection.contrast") {
        float old = surface.colorCorrection().contrast;
        auto cc = surface.colorCorrection(); cc.contrast = parseFloat(value);
        surface.setColorCorrection(cc);
        return formatFloat(old);
    }
    if (path == "colorCorrection.saturation") {
        float old = surface.colorCorrection().saturation;
        auto cc = surface.colorCorrection(); cc.saturation = parseFloat(value);
        surface.setColorCorrection(cc);
        return formatFloat(old);
    }

    // Type-specific properties
    switch (surface.kind()) {
        case SurfaceKind::Quad: {
            auto& q = static_cast<SurfaceQuad&>(surface);
            if (path == "perspectiveCorrection") {
                bool old = q.perspectiveCorrection();
                q.setPerspectiveCorrection(parseBool(value));
                return formatBool(old);
            }
            if (path == "meshResolution") {
                int old = q.meshResolution();
                q.setMeshResolution(parseInt(value));
                return formatInt(old);
            }
            // dest.N.x / dest.N.y
            if (path.size() > 7 && path.substr(0, 5) == "dest.") {
                auto dot = path.find('.', 5);
                if (dot != std::string::npos) {
                    int idx = parseInt(path.substr(5, dot - 5));
                    bool isY = path.substr(dot + 1) == "y";
                    auto& dp = q.destinationPoints();
                    if (idx >= 0 && idx < 4) {
                        std::string old = formatFloat(isY ? dp[idx].y : dp[idx].x);
                        if (isY) dp[idx].y = parseFloat(value);
                        else     dp[idx].x = parseFloat(value);
                        q.markDirty();
                        return old;
                    }
                }
            }
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<SurfaceGrid&>(surface);
            if (path == "cols") {
                int old = g.cols(); g.setCols(parseInt(value)); return formatInt(old);
            }
            if (path == "rows") {
                int old = g.rows(); g.setRows(parseInt(value)); return formatInt(old);
            }
            if (path == "curvedInterpolation") {
                bool old = g.curvedInterpolation();
                g.setCurvedInterpolation(parseBool(value));
                return formatBool(old);
            }
            if (path == "meshResolution") {
                int old = g.meshResolution();
                g.setMeshResolution(parseInt(value));
                return formatInt(old);
            }
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<SurfaceBezier&>(surface);
            if (path == "controlCols") {
                int old = b.controlCols();
                b.setControlDimensions(parseInt(value), b.controlRows());
                return formatInt(old);
            }
            if (path == "controlRows") {
                int old = b.controlRows();
                b.setControlDimensions(b.controlCols(), parseInt(value));
                return formatInt(old);
            }
            if (path == "meshResolution") {
                int old = b.meshResolution();
                b.setMeshResolution(parseInt(value));
                return formatInt(old);
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& t = static_cast<SurfaceTriangle&>(surface);
            if (path.size() > 7 && path.substr(0, 5) == "dest.") {
                auto dot = path.find('.', 5);
                if (dot != std::string::npos) {
                    int idx = parseInt(path.substr(5, dot - 5));
                    bool isY = path.substr(dot + 1) == "y";
                    auto& dp = t.destinationPoints();
                    if (idx >= 0 && idx < 3) {
                        std::string old = formatFloat(isY ? dp[idx].y : dp[idx].x);
                        if (isY) dp[idx].y = parseFloat(value);
                        else     dp[idx].x = parseFloat(value);
                        t.markDirty();
                        return old;
                    }
                }
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(surface);
            if (path == "center") {
                Vec2 old = c.center(); c.setCenter(parseVec2(value));
                return formatVec2(old);
            }
            if (path == "center.x") {
                float old = c.center().x;
                c.setCenter(Vec2(parseFloat(value), c.center().y));
                return formatFloat(old);
            }
            if (path == "center.y") {
                float old = c.center().y;
                c.setCenter(Vec2(c.center().x, parseFloat(value)));
                return formatFloat(old);
            }
            if (path == "radiusX") {
                float old = c.radiusX(); c.setRadiusX(parseFloat(value));
                return formatFloat(old);
            }
            if (path == "radiusY") {
                float old = c.radiusY(); c.setRadiusY(parseFloat(value));
                return formatFloat(old);
            }
            if (path == "rotation") {
                float old = c.rotation(); c.setRotation(parseFloat(value));
                return formatFloat(old);
            }
            if (path == "segments") {
                int old = c.segments(); c.setSegments(parseInt(value));
                return formatInt(old);
            }
            break;
        }
        case SurfaceKind::Polygon: {
            auto& p = static_cast<SurfacePolygon&>(surface);
            if (path == "closed") {
                bool old = p.closed(); p.setClosed(parseBool(value));
                return formatBool(old);
            }
            break;
        }
    }
    return "";
}

// Deferred — PropertyEditCommand::execute / undo
void PropertyEditCommand::execute() {
    auto s = doc_->getSurface(id_);
    if (s) {
        applyProperty(*s, path_, newValue_);
        doc_->markDirty();
    }
}

void PropertyEditCommand::undo() {
    auto s = doc_->getSurface(id_);
    if (s) {
        applyProperty(*s, path_, oldValue_);
        doc_->markDirty();
    }
}

// ===========================================================================
// Section 4: Impl
// ===========================================================================

struct MapWrapEditor::Impl {
    EditMode mode = EditMode::SurfaceEdit;
    bool enabled = true;
    SurfaceId selectedSurface;
    EditorViewport viewport;
    SnapSettings snapSettings;
    OverlayOptions overlayOptions;

    MapWrapDocument* document = nullptr;
    UndoStack* undoStack = nullptr;
    HitTestIndex hitTestIndex;

    // Drag state
    bool dragging = false;
    Vec2 dragStartCanvasNorm;
    Vec2 dragLastCanvasNorm;
    SurfaceId dragSurfaceId;
    HandleKind dragHandleKind = HandleKind::None;
    int dragHandleIndex = -1;
    HandleKind selectedHandleKind = HandleKind::None;
    int selectedHandleIndex = -1;
    SurfaceSnapshot dragStartSnapshot;

    // Clipboard
    std::vector<Vec2> clipboardGeometry;
    std::vector<Vec2> clipboardUV;
    SurfaceKind clipboardKind = SurfaceKind::Quad;

    // Aspect ratio lock
    bool aspectRatioLocked = false;

    bool normalizeSelectedHandle();
    void selectDefaultHandle(const Surface& surface);
    bool commitSelectedGeometryEdit(const std::string& desc,
                                    const std::function<bool(Surface&)>& edit);
};

static bool isSelectablePointHandle(HandleKind kind) {
    return kind == HandleKind::Vertex ||
           kind == HandleKind::TextureVertex ||
           kind == HandleKind::GridPoint ||
           kind == HandleKind::RotationHandle;
}

static int handleCountForKind(const Surface& surface, HandleKind kind) {
    switch (kind) {
        case HandleKind::Vertex:
        case HandleKind::TextureVertex:
            switch (surface.kind()) {
                case SurfaceKind::Quad:     return 4;
                case SurfaceKind::Triangle: return 3;
                case SurfaceKind::Circle:   return kind == HandleKind::Vertex ? 3 : 0;
                case SurfaceKind::Polygon:
                    return static_cast<int>(static_cast<const SurfacePolygon&>(surface).destinationPoints().size());
                default: return 0;
            }
        case HandleKind::GridPoint:
            if (surface.kind() == SurfaceKind::Grid) {
                const auto& g = static_cast<const SurfaceGrid&>(surface);
                return (g.cols() + 1) * (g.rows() + 1);
            }
            if (surface.kind() == SurfaceKind::Bezier) {
                const auto& b = static_cast<const SurfaceBezier&>(surface);
                return b.controlCols() * b.controlRows();
            }
            return 0;
        case HandleKind::RotationHandle:
            return surface.kind() == SurfaceKind::Circle ? 1 : 0;
        default:
            return 0;
    }
}

static HandleKind defaultHandleKindForSurface(const Surface& surface, EditMode mode) {
    if (mode == EditMode::TextureEdit) {
        if (surface.kind() == SurfaceKind::Quad ||
            surface.kind() == SurfaceKind::Triangle ||
            surface.kind() == SurfaceKind::Polygon) {
            return HandleKind::TextureVertex;
        }
    }

    if (surface.kind() == SurfaceKind::Grid || surface.kind() == SurfaceKind::Bezier)
        return HandleKind::GridPoint;

    return HandleKind::Vertex;
}

static int wrapHandleIndex(int index, int count) {
    if (count <= 0) return -1;
    int wrapped = index % count;
    if (wrapped < 0) wrapped += count;
    return wrapped;
}

bool MapWrapEditor::Impl::normalizeSelectedHandle() {
    if (!document || selectedSurface.empty()) {
        selectedHandleKind = HandleKind::None;
        selectedHandleIndex = -1;
        return false;
    }

    auto surface = document->getSurface(selectedSurface);
    if (!surface) {
        selectedSurface.clear();
        selectedHandleKind = HandleKind::None;
        selectedHandleIndex = -1;
        return false;
    }

    if (!isSelectablePointHandle(selectedHandleKind) ||
        handleCountForKind(*surface, selectedHandleKind) <= 0) {
        selectedHandleKind = defaultHandleKindForSurface(*surface, mode);
    }

    int count = handleCountForKind(*surface, selectedHandleKind);
    if (count <= 0) {
        selectedHandleKind = HandleKind::None;
        selectedHandleIndex = -1;
        return false;
    }

    selectedHandleIndex = wrapHandleIndex(selectedHandleIndex < 0 ? 0 : selectedHandleIndex, count);
    return true;
}

void MapWrapEditor::Impl::selectDefaultHandle(const Surface& surface) {
    selectedHandleKind = defaultHandleKindForSurface(surface, mode);
    selectedHandleIndex = handleCountForKind(surface, selectedHandleKind) > 0 ? 0 : -1;
}

static bool selectedHandleGridDimensions(const Surface& surface, int& cols, int& rows) {
    if (surface.kind() == SurfaceKind::Grid) {
        const auto& g = static_cast<const SurfaceGrid&>(surface);
        cols = g.cols() + 1;
        rows = g.rows() + 1;
        return true;
    }
    if (surface.kind() == SurfaceKind::Bezier) {
        const auto& b = static_cast<const SurfaceBezier&>(surface);
        cols = b.controlCols();
        rows = b.controlRows();
        return true;
    }
    cols = rows = 0;
    return false;
}

bool MapWrapEditor::Impl::commitSelectedGeometryEdit(const std::string& desc,
                                                     const std::function<bool(Surface&)>& edit) {
    if (!document || selectedSurface.empty()) return false;

    auto surface = document->getSurface(selectedSurface);
    if (!surface || isEffectivelyLocked(*document, *surface)) return false;

    SurfaceSnapshot before = takeSnapshot(*surface);
    if (!edit(*surface)) return false;

    surface->markDirty();
    document->markDirty();
    hitTestIndex.markDirty();

    if (undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        if (!snapshotEqualsApprox(before, after)) {
            undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
                document, selectedSurface,
                std::move(before), std::move(after), desc));
        }
    }

    normalizeSelectedHandle();
    return true;
}

// ===========================================================================
// Section 5: Constructor / Destructor
// ===========================================================================

MapWrapEditor::MapWrapEditor() : impl_(std::make_unique<Impl>()) {}
MapWrapEditor::~MapWrapEditor() = default;

// ===========================================================================
// Section 6: Mode / State accessors
// ===========================================================================

void MapWrapEditor::setMode(EditMode m) {
    impl_->mode = m;
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return;

    HandleKind desired = defaultHandleKindForSurface(*surface, impl_->mode);
    if (impl_->selectedHandleKind != desired &&
        handleCountForKind(*surface, desired) > 0) {
        int current = std::max(0, impl_->selectedHandleIndex);
        impl_->selectedHandleKind = desired;
        impl_->selectedHandleIndex = wrapHandleIndex(
            current, handleCountForKind(*surface, desired));
    }
}
EditMode MapWrapEditor::mode() const { return impl_->mode; }
void MapWrapEditor::setEnabled(bool e) { impl_->enabled = e; }
bool MapWrapEditor::isEnabled() const { return impl_->enabled; }

void MapWrapEditor::setDocument(MapWrapDocument* doc) {
    impl_->document = doc;
    if (doc) {
        impl_->hitTestIndex.rebuild(*doc);
        impl_->normalizeSelectedHandle();
    }
}

void MapWrapEditor::setUndoStack(UndoStack* stack) {
    impl_->undoStack = stack;
}

// ===========================================================================
// Section 7: Pointer events
// ===========================================================================

void MapWrapEditor::pointerDown(const PointerEvent& e) {
    if (!impl_->enabled) return;
    if (impl_->mode == EditMode::Presentation) return;
    if (!impl_->document) return;

    // Convert screen pixels to canvas-normalized coordinates
    Vec2 canvasNorm = impl_->viewport.screenToCanvasNorm(e.positionPixels);

    // Determine handle radius based on pointer device
    float handleRadiusPx =
        (e.device == PointerEvent::Device::Touch)
            ? impl_->overlayOptions.touchHandleRadiusPixels
            : impl_->overlayOptions.mouseHandleRadiusPixels;

    // Convert pixel radius to the unit expected by surface hit tests.
    // Surface hit tests do:  normRadius = radiusPixels / 1000
    // We want normRadius = handleRadiusPx / (canvasSize * zoom)
    // So: radiusPixels = handleRadiusPx * 1000 / (canvasSize * zoom)
    float canvasScale = impl_->viewport.canvasSizePixels.x * impl_->viewport.zoom;
    float adjustedRadius = (canvasScale > 1e-6f)
        ? handleRadiusPx * 1000.0f / canvasScale
        : 8.0f;  // fallback

    HitTestOptions options;
    options.radiusPixels = adjustedRadius;
    options.includeLocked = (impl_->mode == EditMode::SurfaceEdit);
    options.includeInvisible = false;

    // Rebuild hit test index if dirty
    if (impl_->hitTestIndex.isDirty())
        impl_->hitTestIndex.rebuild(*impl_->document);

    HitResult hit = impl_->hitTestIndex.query(canvasNorm, options);

    // Extra check: selected circle rotation handle is an independent hit layer.
    auto checkCircleRotationHandle = [&](const SurfaceId& surfaceId) -> bool {
        auto surface = impl_->document->getSurface(surfaceId);
        if (!surface || surface->kind() != SurfaceKind::Circle ||
            isEffectivelyLocked(*impl_->document, *surface)) return false;
        if (canvasScale <= 1e-6f) return false;

        auto& circle = static_cast<SurfaceCircle&>(*surface);
        float rotRad = degToRad(circle.rotation());
        float offset = 0.04f;
        Vec2 rotHandle(
            circle.center().x + (-(circle.radiusY() + offset)) * (-std::sin(rotRad)),
            circle.center().y + (-(circle.radiusY() + offset)) *   std::cos(rotRad)
        );
        float d = std::sqrt((canvasNorm.x - rotHandle.x) * (canvasNorm.x - rotHandle.x) +
                            (canvasNorm.y - rotHandle.y) * (canvasNorm.y - rotHandle.y));
        float normRadius = handleRadiusPx / canvasScale;
        if (d >= normRadius) return false;

        hit.hit = true;
        hit.surfaceId = surfaceId;
        hit.handleKind = HandleKind::RotationHandle;
        hit.handleIndex = 0;
        hit.canvasNormPos = canvasNorm;
        return true;
    };

    if (impl_->mode == EditMode::SurfaceEdit) {
        if (impl_->selectedSurface.empty() ||
            !checkCircleRotationHandle(impl_->selectedSurface)) {
            if (hit.hit) {
                checkCircleRotationHandle(hit.surfaceId);
            }
        }
    }

    if (hit.hit) {
        auto surface = impl_->document->getSurface(hit.surfaceId);
        if (surface && isEffectivelyLocked(*impl_->document, *surface)) {
            impl_->selectedSurface = hit.surfaceId;
            impl_->selectDefaultHandle(*surface);
            impl_->dragging = false;
            impl_->dragSurfaceId.clear();
            impl_->dragHandleKind = HandleKind::None;
            impl_->dragHandleIndex = -1;
            return;
        }

        bool selectedSurfaceChanged = impl_->selectedSurface != hit.surfaceId;
        impl_->selectedSurface = hit.surfaceId;

        // Start drag
        impl_->dragging = true;
        impl_->dragStartCanvasNorm = canvasNorm;
        impl_->dragLastCanvasNorm = canvasNorm;
        impl_->dragSurfaceId = hit.surfaceId;
        impl_->dragHandleKind = hit.handleKind;
        impl_->dragHandleIndex = hit.handleIndex;

        if (surface)
            impl_->dragStartSnapshot = takeSnapshot(*surface);

        // In TextureEdit mode, treat Vertex handles as TextureVertex
        if (impl_->mode == EditMode::TextureEdit &&
            hit.handleKind == HandleKind::Vertex) {
            impl_->dragHandleKind = HandleKind::TextureVertex;
        }

        if (surface) {
            if (isSelectablePointHandle(impl_->dragHandleKind)) {
                impl_->selectedHandleKind = impl_->dragHandleKind;
                impl_->selectedHandleIndex = impl_->dragHandleIndex;
                impl_->normalizeSelectedHandle();
            } else if (selectedSurfaceChanged ||
                       impl_->selectedHandleKind == HandleKind::None) {
                impl_->selectDefaultHandle(*surface);
            }
        }
    } else {
        impl_->selectedSurface.clear();
        impl_->selectedHandleKind = HandleKind::None;
        impl_->selectedHandleIndex = -1;
    }
}

void MapWrapEditor::pointerMove(const PointerEvent& e) {
    if (!impl_->dragging || !impl_->document) return;

    Vec2 canvasNorm = impl_->viewport.screenToCanvasNorm(e.positionPixels);

    // Apply snapping for handle positions
    if (impl_->snapSettings.enabled &&
        impl_->dragHandleKind != HandleKind::Body) {
        canvasNorm = applySnap(canvasNorm, impl_->snapSettings, impl_->viewport);
    }

    Vec2 delta(canvasNorm.x - impl_->dragLastCanvasNorm.x,
               canvasNorm.y - impl_->dragLastCanvasNorm.y);

    auto surface = impl_->document->getSurface(impl_->dragSurfaceId);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    switch (impl_->dragHandleKind) {
        case HandleKind::Vertex:
            moveHandleVertex(*surface, impl_->dragHandleIndex, delta, false);
            break;
        case HandleKind::TextureVertex:
            moveHandleVertex(*surface, impl_->dragHandleIndex, delta, true);
            break;
        case HandleKind::GridPoint:
            moveGridPointHandle(*surface, impl_->dragHandleIndex, delta);
            break;
        case HandleKind::Body:
            moveSurface(*surface, delta);
            break;
        case HandleKind::RotationHandle:
            rotateCircleSurface(*surface, canvasNorm);
            break;
        default:
            break;
    }

    impl_->dragLastCanvasNorm = canvasNorm;
    impl_->document->markDirty();
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::pointerUp(const PointerEvent& e) {
    if (!impl_->dragging) return;

    // Create undo command if geometry actually changed
    if (impl_->document && impl_->undoStack) {
        auto surface = impl_->document->getSurface(impl_->dragSurfaceId);
        if (surface) {
            SurfaceSnapshot afterSnapshot = takeSnapshot(*surface);
            if (!snapshotEqualsApprox(impl_->dragStartSnapshot, afterSnapshot)) {
                std::string desc;
                switch (impl_->dragHandleKind) {
                    case HandleKind::Body:           desc = tr("command.move_surface"); break;
                    case HandleKind::Vertex:         desc = tr("command.move_vertex");  break;
                    case HandleKind::TextureVertex:  desc = tr("command.move_uv");      break;
                    case HandleKind::GridPoint:      desc = tr("command.move_grid");    break;
                    case HandleKind::RotationHandle: desc = tr("command.rotate");       break;
                    default:                         desc = tr("command.edit");         break;
                }
                impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
                    impl_->document, impl_->dragSurfaceId,
                    std::move(impl_->dragStartSnapshot), std::move(afterSnapshot),
                    desc));
            }
        }
    }

    impl_->dragging = false;
    impl_->dragHandleKind = HandleKind::None;
    impl_->dragHandleIndex = -1;
    impl_->dragSurfaceId.clear();
    if (impl_->document)
        impl_->document->markDirty();
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::pointerCancel(const PointerEvent& e) {
    if (!impl_->dragging) return;

    // Restore geometry to pre-drag state (cancel the drag)
    if (impl_->document) {
        auto surface = impl_->document->getSurface(impl_->dragSurfaceId);
        if (surface)
            restoreSnapshot(*surface, impl_->dragStartSnapshot);
    }

    impl_->dragging = false;
    impl_->dragHandleKind = HandleKind::None;
    impl_->dragHandleIndex = -1;
    impl_->dragSurfaceId.clear();
    if (impl_->document)
        impl_->document->markDirty();
    impl_->hitTestIndex.markDirty();
}

// ===========================================================================
// Section 8: Keyboard events
// ===========================================================================

void MapWrapEditor::keyPressed(int key) {
    if (!impl_->enabled || impl_->mode == EditMode::Presentation) return;

    if (key == 258) cycleSelectedHandle(1);           // Tab
    else if (key == 262) selectAdjacentHandle(1, 0);   // Right
    else if (key == 263) selectAdjacentHandle(-1, 0);  // Left
    else if (key == 264) selectAdjacentHandle(0, 1);   // Down
    else if (key == 265) selectAdjacentHandle(0, -1);  // Up
}

void MapWrapEditor::keyReleased(int key) {
    // No action needed on key release
}

// ===========================================================================
// Section 9: Selection
// ===========================================================================

void MapWrapEditor::selectSurface(const SurfaceId& id) {
    impl_->selectedSurface = id;
    if (impl_->document) {
        auto surface = impl_->document->getSurface(id);
        if (surface) impl_->selectDefaultHandle(*surface);
        else {
            impl_->selectedHandleKind = HandleKind::None;
            impl_->selectedHandleIndex = -1;
        }
    }
    if (impl_->document)
        impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::deselect() {
    impl_->selectedSurface.clear();
    impl_->selectedHandleKind = HandleKind::None;
    impl_->selectedHandleIndex = -1;
}

SurfaceId MapWrapEditor::selectedSurface() const {
    return impl_->selectedSurface;
}

HandleKind MapWrapEditor::selectedHandleKind() const {
    return impl_->selectedHandleKind;
}

int MapWrapEditor::selectedHandleIndex() const {
    return impl_->selectedHandleIndex;
}

void MapWrapEditor::selectHandle(HandleKind kind, int index) {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return;

    int count = handleCountForKind(*surface, kind);
    if (count <= 0) return;

    impl_->selectedHandleKind = kind;
    impl_->selectedHandleIndex = wrapHandleIndex(index, count);
}

bool MapWrapEditor::cycleSelectedHandle(int delta) {
    if (!impl_->normalizeSelectedHandle()) return false;
    if (!impl_->document || impl_->selectedSurface.empty()) return false;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return false;

    int count = handleCountForKind(*surface, impl_->selectedHandleKind);
    if (count <= 0) return false;

    impl_->selectedHandleIndex = wrapHandleIndex(impl_->selectedHandleIndex + delta, count);
    return true;
}

bool MapWrapEditor::selectAdjacentHandle(int dx, int dy) {
    if (!impl_->normalizeSelectedHandle()) return false;
    if (!impl_->document || impl_->selectedSurface.empty()) return false;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return false;

    if (impl_->selectedHandleKind == HandleKind::GridPoint) {
        int cols = 0, rows = 0;
        if (selectedHandleGridDimensions(*surface, cols, rows) && cols > 0 && rows > 0) {
            int index = std::max(0, impl_->selectedHandleIndex);
            int col = index % cols;
            int row = index / cols;
            col = wrapHandleIndex(col + dx, cols);
            row = wrapHandleIndex(row + dy, rows);
            impl_->selectedHandleIndex = row * cols + col;
            return true;
        }
    }

    int delta = (dx + dy) >= 0 ? 1 : -1;
    return cycleSelectedHandle(delta);
}

// ===========================================================================
// Section 10: Surface CRUD
// ===========================================================================

void MapWrapEditor::deleteSelected() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return;

    int idx = impl_->document->surfaceIndex(impl_->selectedSurface);
    SurfaceId deletedId = impl_->selectedSurface;

    if (impl_->undoStack) {
        impl_->undoStack->push(std::make_unique<EditorDeleteSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->removeSurface(deletedId);
    }

    impl_->selectedSurface.clear();
    impl_->selectedHandleKind = HandleKind::None;
    impl_->selectedHandleIndex = -1;
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::duplicateSelected() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;

    auto src = impl_->document->getSurface(impl_->selectedSurface);
    if (!src) return;

    // Offset for the duplicate
    const Vec2 offset(0.05f, 0.05f);

    std::shared_ptr<Surface> dup;
    switch (src->kind()) {
        case SurfaceKind::Quad: {
            auto s = std::make_shared<SurfaceQuad>();
            auto& dp = s->destinationPoints();
            auto& uv = s->uvPoints();
            const auto& srcQuad = static_cast<const SurfaceQuad&>(*src);
            const auto& srcDp = srcQuad.destinationPoints();
            const auto& srcUv = srcQuad.uvPoints();
            for (int i = 0; i < 4; ++i) {
                dp[i] = Vec2(srcDp[i].x + offset.x, srcDp[i].y + offset.y);
                uv[i] = srcUv[i];
            }
            s->setPerspectiveCorrection(srcQuad.perspectiveCorrection());
            s->setMeshResolution(srcQuad.meshResolution());
            dup = s;
            break;
        }
        case SurfaceKind::Grid: {
            const auto& sg = static_cast<const SurfaceGrid&>(*src);
            auto s = std::make_shared<SurfaceGrid>(sg.cols(), sg.rows());
            for (int r = 0; r <= sg.rows(); ++r)
                for (int c = 0; c <= sg.cols(); ++c) {
                    Vec2 p = sg.gridPoint(c, r);
                    s->setGridPoint(c, r, Vec2(p.x + offset.x, p.y + offset.y));
                }
            s->setCurvedInterpolation(sg.curvedInterpolation());
            s->setMeshResolution(sg.meshResolution());
            dup = s;
            break;
        }
        case SurfaceKind::Bezier: {
            const auto& sb = static_cast<const SurfaceBezier&>(*src);
            auto s = std::make_shared<SurfaceBezier>(sb.controlCols(), sb.controlRows());
            for (int r = 0; r < sb.controlRows(); ++r)
                for (int c = 0; c < sb.controlCols(); ++c) {
                    Vec2 p = sb.controlPoint(c, r);
                    s->setControlPoint(c, r, Vec2(p.x + offset.x, p.y + offset.y));
                }
            s->setMeshResolution(sb.meshResolution());
            dup = s;
            break;
        }
        case SurfaceKind::Triangle: {
            auto s = std::make_shared<SurfaceTriangle>();
            auto& dp = s->destinationPoints();
            auto& uv = s->uvPoints();
            const auto& srcTri = static_cast<const SurfaceTriangle&>(*src);
            const auto& srcDp = srcTri.destinationPoints();
            const auto& srcUv = srcTri.uvPoints();
            for (int i = 0; i < 3; ++i) {
                dp[i] = Vec2(srcDp[i].x + offset.x, srcDp[i].y + offset.y);
                uv[i] = srcUv[i];
            }
            dup = s;
            break;
        }
        case SurfaceKind::Circle: {
            const auto& sc = static_cast<const SurfaceCircle&>(*src);
            auto s = std::make_shared<SurfaceCircle>();
            s->setCenter(Vec2(sc.center().x + offset.x, sc.center().y + offset.y));
            s->setRadiusX(sc.radiusX());
            s->setRadiusY(sc.radiusY());
            s->setRotation(sc.rotation());
            s->setSegments(sc.segments());
            dup = s;
            break;
        }
        case SurfaceKind::Polygon: {
            const auto& sp = static_cast<const SurfacePolygon&>(*src);
            auto s = std::make_shared<SurfacePolygon>();
            s->setClosed(sp.closed());
            std::vector<Vec2> dest;
            dest.reserve(sp.destinationPoints().size());
            for (const auto& pt : sp.destinationPoints()) {
                dest.push_back(Vec2(pt.x + offset.x, pt.y + offset.y));
            }
            s->setDestinationPoints(dest);
            s->setUvPoints(sp.uvPoints());
            dup = s;
            break;
        }
    }

    if (!dup) return;

    // Copy common properties
    dup->setId(impl_->document->allocateSurfaceId(src->kind()));
    dup->setName(src->name() + " copy");
    dup->setVisible(src->isVisible());
    dup->setLocked(src->isLocked());
    dup->setOpacity(src->opacity());
    dup->setSource(src->source());
    dup->setSourceRect(src->sourceRect());
    dup->setBlend(src->blend());
    dup->setColorCorrection(src->colorCorrection());
    dup->setMasks(static_cast<const Surface&>(*src).masks());

    int insertIndex = impl_->document->surfaceIndex(src->id()) + 1;

    if (impl_->undoStack) {
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, dup, insertIndex));
    } else {
        impl_->document->insertSurface(dup, insertIndex);
    }

    impl_->selectedSurface = dup->id();
    impl_->selectDefaultHandle(*dup);
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::bringForward() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;

    int idx = impl_->document->surfaceIndex(impl_->selectedSurface);
    auto& surfaces = impl_->document->surfaces();
    if (idx < 0 || idx >= (int)surfaces.size() - 1) return;

    if (impl_->undoStack) {
        impl_->undoStack->push(std::make_unique<ReorderCommand>(
            impl_->document, impl_->selectedSurface, idx, idx + 1,
            tr("command.bring_forward")));
    } else {
        impl_->document->reorderSurface(impl_->selectedSurface, idx + 1);
    }

    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::sendBackward() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;

    int idx = impl_->document->surfaceIndex(impl_->selectedSurface);
    if (idx <= 0) return;

    if (impl_->undoStack) {
        impl_->undoStack->push(std::make_unique<ReorderCommand>(
            impl_->document, impl_->selectedSurface, idx, idx - 1,
            tr("command.send_backward")));
    } else {
        impl_->document->reorderSurface(impl_->selectedSurface, idx - 1);
    }

    impl_->hitTestIndex.markDirty();
}

Result MapWrapEditor::convertSelectedTo(SurfaceKind kind) {
    if (!impl_->document || impl_->selectedSurface.empty())
        return Result::error("No surface selected");

    auto source = impl_->document->getSurface(impl_->selectedSurface);
    if (!source) return Result::error("Surface not found");
    if (source->kind() == kind) return Result::success();
    if (isEffectivelyLocked(*impl_->document, *source)) return Result::error("Selected surface is locked");

    int index = impl_->document->surfaceIndex(impl_->selectedSurface);
    if (index < 0) return Result::error("Surface is not in the document");

    auto converted = convertSurfaceToKind(*source, kind);
    if (!converted) return Result::error("Unsupported surface conversion");

    impl_->document->removeSurface(source->id());
    impl_->document->insertSurface(converted, index);
    impl_->selectedSurface = converted->id();
    impl_->selectDefaultHandle(*converted);
    impl_->hitTestIndex.markDirty();

    if (impl_->undoStack) {
        impl_->undoStack->push(std::make_unique<ReplaceSurfaceCommand>(
            impl_->document, source, converted, index, tr("command.convert_surface")));
    }

    return Result::success();
}

bool MapWrapEditor::addColumnToSelected() {
    return impl_->commitSelectedGeometryEdit(tr("command.add_column"), [](Surface& surface) {
        if (surface.kind() == SurfaceKind::Grid) {
            auto& g = static_cast<SurfaceGrid&>(surface);
            if (g.cols() >= 20) return false;
            int old = g.cols();
            g.addColumn();
            return g.cols() != old;
        }
        if (surface.kind() == SurfaceKind::Bezier) {
            auto& b = static_cast<SurfaceBezier&>(surface);
            if (b.controlCols() >= 12) return false;
            int old = b.controlCols();
            b.setControlDimensions(old + 1, b.controlRows());
            return b.controlCols() != old;
        }
        return false;
    });
}

bool MapWrapEditor::removeColumnFromSelected() {
    return impl_->commitSelectedGeometryEdit(tr("command.remove_column"), [](Surface& surface) {
        if (surface.kind() == SurfaceKind::Grid) {
            auto& g = static_cast<SurfaceGrid&>(surface);
            int old = g.cols();
            g.removeColumn();
            return g.cols() != old;
        }
        if (surface.kind() == SurfaceKind::Bezier) {
            auto& b = static_cast<SurfaceBezier&>(surface);
            if (b.controlCols() <= 2) return false;
            int old = b.controlCols();
            b.setControlDimensions(old - 1, b.controlRows());
            return b.controlCols() != old;
        }
        return false;
    });
}

bool MapWrapEditor::addRowToSelected() {
    return impl_->commitSelectedGeometryEdit(tr("command.add_row"), [](Surface& surface) {
        if (surface.kind() == SurfaceKind::Grid) {
            auto& g = static_cast<SurfaceGrid&>(surface);
            if (g.rows() >= 20) return false;
            int old = g.rows();
            g.addRow();
            return g.rows() != old;
        }
        if (surface.kind() == SurfaceKind::Bezier) {
            auto& b = static_cast<SurfaceBezier&>(surface);
            if (b.controlRows() >= 12) return false;
            int old = b.controlRows();
            b.setControlDimensions(b.controlCols(), old + 1);
            return b.controlRows() != old;
        }
        return false;
    });
}

bool MapWrapEditor::removeRowFromSelected() {
    return impl_->commitSelectedGeometryEdit(tr("command.remove_row"), [](Surface& surface) {
        if (surface.kind() == SurfaceKind::Grid) {
            auto& g = static_cast<SurfaceGrid&>(surface);
            int old = g.rows();
            g.removeRow();
            return g.rows() != old;
        }
        if (surface.kind() == SurfaceKind::Bezier) {
            auto& b = static_cast<SurfaceBezier&>(surface);
            if (b.controlRows() <= 2) return false;
            int old = b.controlRows();
            b.setControlDimensions(b.controlCols(), old - 1);
            return b.controlRows() != old;
        }
        return false;
    });
}

bool MapWrapEditor::adjustSelectedMeshResolution(int delta) {
    return impl_->commitSelectedGeometryEdit(tr("command.adjust_mesh_resolution"), [delta](Surface& surface) {
        if (surface.kind() == SurfaceKind::Quad) {
            auto& q = static_cast<SurfaceQuad&>(surface);
            int old = q.meshResolution();
            q.setMeshResolution(old + delta);
            return q.meshResolution() != old;
        }
        if (surface.kind() == SurfaceKind::Grid) {
            auto& g = static_cast<SurfaceGrid&>(surface);
            int old = g.meshResolution();
            g.setMeshResolution(old + delta);
            return g.meshResolution() != old;
        }
        if (surface.kind() == SurfaceKind::Bezier) {
            auto& b = static_cast<SurfaceBezier&>(surface);
            int old = b.meshResolution();
            b.setMeshResolution(old + delta);
            return b.meshResolution() != old;
        }
        return false;
    });
}

// ===========================================================================
// Section 11: Surface creation
// ===========================================================================

void MapWrapEditor::addQuad() {
    if (!impl_->document) return;
    auto surface = impl_->document->createQuadSurface();
    if (impl_->undoStack) {
        int idx = (int)impl_->document->surfaces().size();
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->addSurface(surface);
    }
    impl_->selectedSurface = surface->id();
    impl_->selectDefaultHandle(*surface);
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::addGrid(int c, int r) {
    if (!impl_->document) return;
    auto surface = impl_->document->createGridSurface(c, r);
    if (impl_->undoStack) {
        int idx = (int)impl_->document->surfaces().size();
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->addSurface(surface);
    }
    impl_->selectedSurface = surface->id();
    impl_->selectDefaultHandle(*surface);
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::addBezier(int controlCols, int controlRows) {
    if (!impl_->document) return;
    auto surface = impl_->document->createBezierSurface(controlCols, controlRows);
    if (impl_->undoStack) {
        int idx = (int)impl_->document->surfaces().size();
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->addSurface(surface);
    }
    impl_->selectedSurface = surface->id();
    impl_->selectDefaultHandle(*surface);
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::addTriangle() {
    if (!impl_->document) return;
    auto surface = impl_->document->createTriangleSurface();
    if (impl_->undoStack) {
        int idx = (int)impl_->document->surfaces().size();
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->addSurface(surface);
    }
    impl_->selectedSurface = surface->id();
    impl_->selectDefaultHandle(*surface);
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::addCircle() {
    if (!impl_->document) return;
    auto surface = impl_->document->createCircleSurface();
    if (impl_->undoStack) {
        int idx = (int)impl_->document->surfaces().size();
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->addSurface(surface);
    }
    impl_->selectedSurface = surface->id();
    impl_->selectDefaultHandle(*surface);
    impl_->hitTestIndex.markDirty();
}

void MapWrapEditor::addPolygon(const std::vector<Vec2>& points) {
    if (!impl_->document) return;

    std::vector<Vec2> pts = points;
    if (pts.empty()) {
        // Default: regular pentagon centered at (0.5, 0.5)
        for (int i = 0; i < 5; ++i) {
            float angle = 2.0f * kPi * float(i) / 5.0f - kPi / 2.0f;
            pts.push_back(Vec2(0.5f + 0.3f * std::cos(angle),
                               0.5f + 0.3f * std::sin(angle)));
        }
    }

    auto surface = impl_->document->createPolygonSurface(pts);
    if (impl_->undoStack) {
        int idx = (int)impl_->document->surfaces().size();
        impl_->undoStack->push(std::make_unique<CreateSurfaceCommand>(
            impl_->document, surface, idx));
    } else {
        impl_->document->addSurface(surface);
    }
    impl_->selectedSurface = surface->id();
    impl_->selectDefaultHandle(*surface);
    impl_->hitTestIndex.markDirty();
}

// ===========================================================================
// Section 12: Overlay
// ===========================================================================

void MapWrapEditor::drawOverlay(const OverlayOptions& options) {
    // Store overlay options for use by the rendering system.
    // The MapWrapRenderer reads the editor state (selection, handles) and
    // performs the actual drawing using the TrussC graphics API.
    impl_->overlayOptions = options;
}

// ===========================================================================
// Section 13: Viewport
// ===========================================================================

EditorViewport& MapWrapEditor::viewport() { return impl_->viewport; }
const EditorViewport& MapWrapEditor::viewport() const { return impl_->viewport; }

// ===========================================================================
// Section 14: Nudge / Align / Fit
// ===========================================================================

void MapWrapEditor::nudgeSelected(Vec2 deltaPixels) {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Vec2 canvasScale(impl_->viewport.canvasSizePixels.x * impl_->viewport.zoom,
                     impl_->viewport.canvasSizePixels.y * impl_->viewport.zoom);
    if (canvasScale.x < 1e-6f || canvasScale.y < 1e-6f) return;
    Vec2 deltaNorm(deltaPixels.x / canvasScale.x, deltaPixels.y / canvasScale.y);

    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, deltaNorm);

    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        if (!snapshotEqualsApprox(before, after)) {
            impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
                impl_->document, impl_->selectedSurface,
                std::move(before), std::move(after),
                tr("command.nudge")));
        }
    }
}

void MapWrapEditor::nudgeSelectedNormalized(Vec2 deltaNorm) {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, deltaNorm);

    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        if (!snapshotEqualsApprox(before, after)) {
            impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
                impl_->document, impl_->selectedSurface,
                std::move(before), std::move(after),
                tr("command.nudge")));
        }
    }
}

void MapWrapEditor::nudgeSelectedHandle(Vec2 deltaPixels) {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    if (!impl_->normalizeSelectedHandle()) return;

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Vec2 canvasScale(impl_->viewport.canvasSizePixels.x * impl_->viewport.zoom,
                     impl_->viewport.canvasSizePixels.y * impl_->viewport.zoom);
    if (canvasScale.x < 1e-6f || canvasScale.y < 1e-6f) return;
    Vec2 deltaNorm(deltaPixels.x / canvasScale.x, deltaPixels.y / canvasScale.y);

    SurfaceSnapshot before = takeSnapshot(*surface);

    switch (impl_->selectedHandleKind) {
        case HandleKind::Vertex:
            moveHandleVertex(*surface, impl_->selectedHandleIndex, deltaNorm, false);
            break;
        case HandleKind::TextureVertex:
            moveHandleVertex(*surface, impl_->selectedHandleIndex, deltaNorm, true);
            break;
        case HandleKind::GridPoint:
            moveGridPointHandle(*surface, impl_->selectedHandleIndex, deltaNorm);
            break;
        default:
            break;
    }

    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        if (!snapshotEqualsApprox(before, after)) {
            impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
                impl_->document, impl_->selectedSurface,
                std::move(before), std::move(after),
                tr("command.nudge_handle")));
        }
    }
}

void MapWrapEditor::setSelectedHandlePosition(Vec2 canvasNorm) {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    if (!impl_->normalizeSelectedHandle()) return;

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    SurfaceSnapshot before = takeSnapshot(*surface);

    // Compute the current handle position and the delta to the target
    Vec2 current;
    bool valid = false;
    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            auto& dp = static_cast<SurfaceQuad&>(*surface).destinationPoints();
            if (impl_->selectedHandleIndex >= 0 && impl_->selectedHandleIndex < 4) {
                current = dp[impl_->selectedHandleIndex]; valid = true;
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& dp = static_cast<SurfaceTriangle&>(*surface).destinationPoints();
            if (impl_->selectedHandleIndex >= 0 && impl_->selectedHandleIndex < 3) {
                current = dp[impl_->selectedHandleIndex]; valid = true;
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(*surface);
            if (impl_->selectedHandleIndex == 0) {
                current = c.center(); valid = true;
            }
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<SurfaceGrid&>(*surface);
            int stride = g.cols() + 1;
            int row = impl_->selectedHandleIndex / stride;
            int col = impl_->selectedHandleIndex % stride;
            if (impl_->selectedHandleIndex >= 0 &&
                row >= 0 && row <= g.rows() &&
                col >= 0 && col <= g.cols()) {
                current = g.gridPoint(col, row);
                valid = true;
            }
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<SurfaceBezier&>(*surface);
            int stride = b.controlCols();
            int row = impl_->selectedHandleIndex / stride;
            int col = impl_->selectedHandleIndex % stride;
            if (impl_->selectedHandleIndex >= 0 &&
                row >= 0 && row < b.controlRows() &&
                col >= 0 && col < b.controlCols()) {
                current = b.controlPoint(col, row);
                valid = true;
            }
            break;
        }
        case SurfaceKind::Polygon: {
            auto& dp = static_cast<SurfacePolygon&>(*surface).destinationPoints();
            if (impl_->selectedHandleIndex >= 0 &&
                impl_->selectedHandleIndex < (int)dp.size()) {
                current = dp[impl_->selectedHandleIndex]; valid = true;
            }
            break;
        }
        default:
            break;
    }

    if (valid) {
        Vec2 delta(canvasNorm.x - current.x, canvasNorm.y - current.y);
        if (impl_->selectedHandleKind == HandleKind::GridPoint) {
            moveGridPointHandle(*surface, impl_->selectedHandleIndex, delta);
        } else {
            moveHandleVertex(*surface, impl_->selectedHandleIndex, delta,
                             impl_->selectedHandleKind == HandleKind::TextureVertex);
        }

        if (impl_->undoStack) {
            SurfaceSnapshot after = takeSnapshot(*surface);
            impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
                impl_->document, impl_->selectedSurface,
                std::move(before), std::move(after),
                tr("command.set_handle_position")));
        }
    }
}

void MapWrapEditor::fitSelectedToCanvas() {
    fitSelectedToRect(Rect(0, 0, 1, 1));
}

void MapWrapEditor::fitSelectedToRect(Rect canvasNormRect) {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    SurfaceSnapshot before = takeSnapshot(*surface);

    float x = canvasNormRect.x, y = canvasNormRect.y;
    float w = canvasNormRect.w, h = canvasNormRect.h;

    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            auto& q = static_cast<SurfaceQuad&>(*surface);
            auto& dp = q.destinationPoints();
            dp[0] = Vec2(x, y);
            dp[1] = Vec2(x + w, y);
            dp[2] = Vec2(x + w, y + h);
            dp[3] = Vec2(x, y + h);
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<SurfaceGrid&>(*surface);
            for (int r = 0; r <= g.rows(); ++r)
                for (int c = 0; c <= g.cols(); ++c) {
                    float px = x + w * float(c) / float(g.cols());
                    float py = y + h * float(r) / float(g.rows());
                    g.setGridPoint(c, r, Vec2(px, py));
            }
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<SurfaceBezier&>(*surface);
            Rect bounds = surfaceBounds(*surface);
            if (bounds.w > 1e-8f && bounds.h > 1e-8f) {
                float scaleX = w / bounds.w;
                float scaleY = h / bounds.h;
                for (int r = 0; r < b.controlRows(); ++r) {
                    for (int c = 0; c < b.controlCols(); ++c) {
                        Vec2 p = b.controlPoint(c, r);
                        b.setControlPoint(c, r, Vec2(x + (p.x - bounds.x) * scaleX,
                                                     y + (p.y - bounds.y) * scaleY));
                    }
                }
            } else {
                for (int r = 0; r < b.controlRows(); ++r) {
                    for (int c = 0; c < b.controlCols(); ++c) {
                        float u = float(c) / float(std::max(1, b.controlCols() - 1));
                        float v = float(r) / float(std::max(1, b.controlRows() - 1));
                        b.setControlPoint(c, r, Vec2(x + w * u, y + h * v));
                    }
                }
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& t = static_cast<SurfaceTriangle&>(*surface);
            auto& dp = t.destinationPoints();
            dp[0] = Vec2(x + w * 0.5f, y);
            dp[1] = Vec2(x + w, y + h);
            dp[2] = Vec2(x, y + h);
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(*surface);
            c.setCenter(Vec2(x + w * 0.5f, y + h * 0.5f));
            c.setRadiusX(w * 0.5f);
            c.setRadiusY(h * 0.5f);
            c.setRotation(0);
            break;
        }
        case SurfaceKind::Polygon: {
            // Scale polygon bounding box to fit the rect
            Rect bounds = surfaceBounds(*surface);
            if (bounds.w > 1e-8f && bounds.h > 1e-8f) {
                float scaleX = w / bounds.w;
                float scaleY = h / bounds.h;
                auto& dp = static_cast<SurfacePolygon&>(*surface).destinationPoints();
                for (auto& p : dp) {
                    p = Vec2(x + (p.x - bounds.x) * scaleX,
                             y + (p.y - bounds.y) * scaleY);
                }
            }
            break;
        }
    }
    surface->markDirty();

    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.fit_to_canvas")));
    }
}

void MapWrapEditor::alignSelectedLeft() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Rect bounds = surfaceBounds(*surface);
    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, Vec2(-bounds.x, 0));
    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.align_left")));
    }
}

void MapWrapEditor::alignSelectedRight() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Rect bounds = surfaceBounds(*surface);
    float rightEdge = bounds.x + bounds.w;
    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, Vec2(1.0f - rightEdge, 0));
    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.align_right")));
    }
}

void MapWrapEditor::alignSelectedTop() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Rect bounds = surfaceBounds(*surface);
    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, Vec2(0, -bounds.y));
    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.align_top")));
    }
}

void MapWrapEditor::alignSelectedBottom() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Rect bounds = surfaceBounds(*surface);
    float bottomEdge = bounds.y + bounds.h;
    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, Vec2(0, 1.0f - bottomEdge));
    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.align_bottom")));
    }
}

void MapWrapEditor::alignSelectedCenterX() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Rect bounds = surfaceBounds(*surface);
    float center = bounds.x + bounds.w * 0.5f;
    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, Vec2(0.5f - center, 0));
    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.align_center_x")));
    }
}

void MapWrapEditor::alignSelectedCenterY() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    Rect bounds = surfaceBounds(*surface);
    float center = bounds.y + bounds.h * 0.5f;
    SurfaceSnapshot before = takeSnapshot(*surface);
    moveSurface(*surface, Vec2(0, 0.5f - center));
    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.align_center_y")));
    }
}

void MapWrapEditor::distributeSelectedHorizontally() {
    // Requires multi-selection support — single surface is a no-op
}

void MapWrapEditor::distributeSelectedVertically() {
    // Requires multi-selection support — single surface is a no-op
}

// ===========================================================================
// Section 15: Clipboard
// ===========================================================================

void MapWrapEditor::copySelectedGeometry() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return;

    impl_->clipboardKind = surface->kind();
    impl_->clipboardGeometry.clear();
    impl_->clipboardUV.clear();

    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            auto& dp = static_cast<SurfaceQuad&>(*surface).destinationPoints();
            auto& uv = static_cast<SurfaceQuad&>(*surface).uvPoints();
            for (int i = 0; i < 4; ++i) {
                impl_->clipboardGeometry.push_back(dp[i]);
                impl_->clipboardUV.push_back(uv[i]);
            }
            break;
        }
        case SurfaceKind::Triangle: {
            auto& dp = static_cast<SurfaceTriangle&>(*surface).destinationPoints();
            auto& uv = static_cast<SurfaceTriangle&>(*surface).uvPoints();
            for (int i = 0; i < 3; ++i) {
                impl_->clipboardGeometry.push_back(dp[i]);
                impl_->clipboardUV.push_back(uv[i]);
            }
            break;
        }
        case SurfaceKind::Circle: {
            auto& c = static_cast<SurfaceCircle&>(*surface);
            impl_->clipboardGeometry.push_back(c.center());
            impl_->clipboardGeometry.push_back(Vec2(c.radiusX(), c.radiusY()));
            impl_->clipboardGeometry.push_back(Vec2(c.rotation(), 0));
            break;
        }
        case SurfaceKind::Grid: {
            auto& g = static_cast<SurfaceGrid&>(*surface);
            impl_->clipboardGeometry.push_back(Vec2(float(g.cols()), float(g.rows())));
            for (int r = 0; r <= g.rows(); ++r)
                for (int c = 0; c <= g.cols(); ++c)
                    impl_->clipboardGeometry.push_back(g.gridPoint(c, r));
            break;
        }
        case SurfaceKind::Bezier: {
            auto& b = static_cast<SurfaceBezier&>(*surface);
            impl_->clipboardGeometry.push_back(Vec2(float(b.controlCols()), float(b.controlRows())));
            impl_->clipboardGeometry.push_back(Vec2(float(b.meshResolution()), 0.0f));
            for (int r = 0; r < b.controlRows(); ++r)
                for (int c = 0; c < b.controlCols(); ++c)
                    impl_->clipboardGeometry.push_back(b.controlPoint(c, r));
            break;
        }
        case SurfaceKind::Polygon: {
            auto& dp = static_cast<SurfacePolygon&>(*surface).destinationPoints();
            auto& uv = static_cast<SurfacePolygon&>(*surface).uvPoints();
            impl_->clipboardGeometry = dp;
            impl_->clipboardUV = uv;
            break;
        }
    }
}

void MapWrapEditor::pasteGeometryToSelected() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    if (impl_->clipboardGeometry.empty()) return;

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    // Only paste if kinds match (or flexible for polygon↔quad conversions)
    SurfaceSnapshot before = takeSnapshot(*surface);

    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            if (impl_->clipboardKind == SurfaceKind::Quad &&
                impl_->clipboardGeometry.size() >= 4) {
                auto& dp = static_cast<SurfaceQuad&>(*surface).destinationPoints();
                for (int i = 0; i < 4; ++i) dp[i] = impl_->clipboardGeometry[i];
            }
            break;
        }
        case SurfaceKind::Triangle: {
            if (impl_->clipboardKind == SurfaceKind::Triangle &&
                impl_->clipboardGeometry.size() >= 3) {
                auto& dp = static_cast<SurfaceTriangle&>(*surface).destinationPoints();
                for (int i = 0; i < 3; ++i) dp[i] = impl_->clipboardGeometry[i];
            }
            break;
        }
        case SurfaceKind::Circle: {
            if (impl_->clipboardKind == SurfaceKind::Circle &&
                impl_->clipboardGeometry.size() >= 3) {
                auto& c = static_cast<SurfaceCircle&>(*surface);
                c.setCenter(impl_->clipboardGeometry[0]);
                c.setRadiusX(impl_->clipboardGeometry[1].x);
                c.setRadiusY(impl_->clipboardGeometry[1].y);
                c.setRotation(impl_->clipboardGeometry[2].x);
            }
            break;
        }
        case SurfaceKind::Grid: {
            if (impl_->clipboardKind == SurfaceKind::Grid &&
                impl_->clipboardGeometry.size() >= 2) {
                auto& g = static_cast<SurfaceGrid&>(*surface);
                int srcCols = (int)impl_->clipboardGeometry[0].x;
                int srcRows = (int)impl_->clipboardGeometry[0].y;
                if (srcCols == g.cols() && srcRows == g.rows()) {
                    int idx = 1;
                    for (int r = 0; r <= srcRows; ++r)
                        for (int c = 0; c <= srcCols; ++c, ++idx)
                            if (idx < (int)impl_->clipboardGeometry.size())
                                g.setGridPoint(c, r, impl_->clipboardGeometry[idx]);
                }
            }
            break;
        }
        case SurfaceKind::Bezier: {
            if (impl_->clipboardKind == SurfaceKind::Bezier &&
                impl_->clipboardGeometry.size() >= 2) {
                auto& b = static_cast<SurfaceBezier&>(*surface);
                int srcCols = (int)impl_->clipboardGeometry[0].x;
                int srcRows = (int)impl_->clipboardGeometry[0].y;
                int srcResolution = (int)impl_->clipboardGeometry[1].x;
                if (srcCols >= 2 && srcRows >= 2) {
                    b.setControlDimensions(srcCols, srcRows);
                    b.setMeshResolution(srcResolution);
                    int idx = 2;
                    for (int r = 0; r < srcRows; ++r)
                        for (int c = 0; c < srcCols; ++c, ++idx)
                            if (idx < (int)impl_->clipboardGeometry.size())
                                b.setControlPoint(c, r, impl_->clipboardGeometry[idx]);
                }
            }
            break;
        }
        case SurfaceKind::Polygon: {
            if (impl_->clipboardKind == SurfaceKind::Polygon) {
                auto& p = static_cast<SurfacePolygon&>(*surface);
                p.destinationPoints() = impl_->clipboardGeometry;
                if (!impl_->clipboardUV.empty())
                    p.uvPoints() = impl_->clipboardUV;
            }
            break;
        }
    }
    surface->markDirty();

    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.paste_geometry")));
    }
}

void MapWrapEditor::copySelectedUV() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return;

    impl_->clipboardUV.clear();

    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            auto& uv = static_cast<SurfaceQuad&>(*surface).uvPoints();
            for (int i = 0; i < 4; ++i) impl_->clipboardUV.push_back(uv[i]);
            break;
        }
        case SurfaceKind::Triangle: {
            auto& uv = static_cast<SurfaceTriangle&>(*surface).uvPoints();
            for (int i = 0; i < 3; ++i) impl_->clipboardUV.push_back(uv[i]);
            break;
        }
        case SurfaceKind::Polygon: {
            impl_->clipboardUV = static_cast<SurfacePolygon&>(*surface).uvPoints();
            break;
        }
        default:
            break;
    }
}

void MapWrapEditor::pasteUVToSelected() {
    if (!impl_->document || impl_->selectedSurface.empty()) return;
    if (impl_->clipboardUV.empty()) return;

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface || isEffectivelyLocked(*impl_->document, *surface)) return;

    SurfaceSnapshot before = takeSnapshot(*surface);

    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            if (impl_->clipboardUV.size() >= 4) {
                auto& uv = static_cast<SurfaceQuad&>(*surface).uvPoints();
                for (int i = 0; i < 4; ++i) uv[i] = impl_->clipboardUV[i];
            }
            break;
        }
        case SurfaceKind::Triangle: {
            if (impl_->clipboardUV.size() >= 3) {
                auto& uv = static_cast<SurfaceTriangle&>(*surface).uvPoints();
                for (int i = 0; i < 3; ++i) uv[i] = impl_->clipboardUV[i];
            }
            break;
        }
        case SurfaceKind::Polygon: {
            auto& uv = static_cast<SurfacePolygon&>(*surface).uvPoints();
            if (impl_->clipboardUV.size() == uv.size())
                uv = impl_->clipboardUV;
            break;
        }
        default:
            break;
    }
    surface->markDirty();

    if (impl_->undoStack) {
        SurfaceSnapshot after = takeSnapshot(*surface);
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<SurfaceEditCommand>(
            impl_->document, impl_->selectedSurface,
            std::move(before), std::move(after),
            tr("command.paste_uv")));
    }
}

void MapWrapEditor::lockSelectedAspectRatio(bool enabled) {
    impl_->aspectRatioLocked = enabled;
}

// ===========================================================================
// Section 16: Properties
// ===========================================================================

std::vector<EditableProperty> MapWrapEditor::selectedProperties() const {
    if (!impl_->document || impl_->selectedSurface.empty()) return {};

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return {};

    std::vector<EditableProperty> props;

    // --- Common properties ---
    props.push_back(EditableProperty::fromI18n(
        "surfaceKind", "property.surface_kind", PropertyKind::Enum,
        surfaceKindToPropertyString(surface->kind())));
    props.push_back(EditableProperty::fromI18n(
        "name", "property.name", PropertyKind::String, surface->name()));
    props.push_back(EditableProperty::fromI18n(
        "visible", "property.visible", PropertyKind::Bool,
        formatBool(surface->isVisible())));
    props.push_back(EditableProperty::fromI18n(
        "locked", "property.locked", PropertyKind::Bool,
        formatBool(surface->isLocked())));
    props.push_back(EditableProperty::fromI18n(
        "opacity", "property.opacity", PropertyKind::Float,
        formatFloat(surface->opacity()), "0", "1"));

    // Blend
    props.push_back(EditableProperty::fromI18n(
        "blend.enabled", "property.blend_enabled", PropertyKind::Bool,
        formatBool(surface->blend().enabled)));
    props.push_back(EditableProperty::fromI18n(
        "blend.opacity", "property.blend_opacity", PropertyKind::Float,
        formatFloat(surface->blend().opacity), "0", "1"));
    props.push_back(EditableProperty::fromI18n(
        "blend.brightness", "property.blend_brightness", PropertyKind::Float,
        formatFloat(surface->blend().brightness), "0", "2"));

    // Color correction
    props.push_back(EditableProperty::fromI18n(
        "colorCorrection.enabled", "property.cc_enabled", PropertyKind::Bool,
        formatBool(surface->colorCorrection().enabled)));
    props.push_back(EditableProperty::fromI18n(
        "colorCorrection.brightness", "property.cc_brightness", PropertyKind::Float,
        formatFloat(surface->colorCorrection().brightness), "0", "2"));
    props.push_back(EditableProperty::fromI18n(
        "colorCorrection.contrast", "property.cc_contrast", PropertyKind::Float,
        formatFloat(surface->colorCorrection().contrast), "0", "2"));
    props.push_back(EditableProperty::fromI18n(
        "colorCorrection.saturation", "property.cc_saturation", PropertyKind::Float,
        formatFloat(surface->colorCorrection().saturation), "0", "2"));

    // --- Type-specific properties ---
    switch (surface->kind()) {
        case SurfaceKind::Quad: {
            const auto& q = static_cast<const SurfaceQuad&>(*surface);
            props.push_back(EditableProperty::fromI18n(
                "perspectiveCorrection", "property.perspective", PropertyKind::Bool,
                formatBool(q.perspectiveCorrection())));
            props.push_back(EditableProperty::fromI18n(
                "meshResolution", "property.mesh_resolution", PropertyKind::Int,
                formatInt(q.meshResolution()), "1", "96"));
            const auto& dp = q.destinationPoints();
            for (int i = 0; i < 4; ++i) {
                props.push_back(EditableProperty::fromI18n(
                    "dest." + std::to_string(i) + ".x",
                    "property.dest_x_" + std::to_string(i),
                    PropertyKind::Float, formatFloat(dp[i].x)));
                props.push_back(EditableProperty::fromI18n(
                    "dest." + std::to_string(i) + ".y",
                    "property.dest_y_" + std::to_string(i),
                    PropertyKind::Float, formatFloat(dp[i].y)));
            }
            break;
        }
        case SurfaceKind::Grid: {
            const auto& g = static_cast<const SurfaceGrid&>(*surface);
            props.push_back(EditableProperty::fromI18n(
                "cols", "property.cols", PropertyKind::Int,
                formatInt(g.cols()), "1", "20"));
            props.push_back(EditableProperty::fromI18n(
                "rows", "property.rows", PropertyKind::Int,
                formatInt(g.rows()), "1", "20"));
            props.push_back(EditableProperty::fromI18n(
                "curvedInterpolation", "property.curved", PropertyKind::Bool,
                formatBool(g.curvedInterpolation())));
            props.push_back(EditableProperty::fromI18n(
                "meshResolution", "property.mesh_resolution", PropertyKind::Int,
                formatInt(g.meshResolution()), "1", "64"));
            break;
        }
        case SurfaceKind::Bezier: {
            const auto& b = static_cast<const SurfaceBezier&>(*surface);
            props.push_back(EditableProperty::fromI18n(
                "controlCols", "property.control_cols", PropertyKind::Int,
                formatInt(b.controlCols()), "2", "12"));
            props.push_back(EditableProperty::fromI18n(
                "controlRows", "property.control_rows", PropertyKind::Int,
                formatInt(b.controlRows()), "2", "12"));
            props.push_back(EditableProperty::fromI18n(
                "meshResolution", "property.mesh_resolution", PropertyKind::Int,
                formatInt(b.meshResolution()), "2", "96"));
            break;
        }
        case SurfaceKind::Triangle: {
            const auto& t = static_cast<const SurfaceTriangle&>(*surface);
            const auto& dp = t.destinationPoints();
            for (int i = 0; i < 3; ++i) {
                props.push_back(EditableProperty::fromI18n(
                    "dest." + std::to_string(i) + ".x",
                    "property.dest_x_" + std::to_string(i),
                    PropertyKind::Float, formatFloat(dp[i].x)));
                props.push_back(EditableProperty::fromI18n(
                    "dest." + std::to_string(i) + ".y",
                    "property.dest_y_" + std::to_string(i),
                    PropertyKind::Float, formatFloat(dp[i].y)));
            }
            break;
        }
        case SurfaceKind::Circle: {
            const auto& c = static_cast<const SurfaceCircle&>(*surface);
            props.push_back(EditableProperty::fromI18n(
                "center", "property.center", PropertyKind::Vec2,
                formatVec2(c.center())));
            props.push_back(EditableProperty::fromI18n(
                "radiusX", "property.radius_x", PropertyKind::Float,
                formatFloat(c.radiusX()), "0.001", "2"));
            props.push_back(EditableProperty::fromI18n(
                "radiusY", "property.radius_y", PropertyKind::Float,
                formatFloat(c.radiusY()), "0.001", "2"));
            props.push_back(EditableProperty::fromI18n(
                "rotation", "property.rotation", PropertyKind::Float,
                formatFloat(c.rotation()), "0", "360"));
            props.push_back(EditableProperty::fromI18n(
                "segments", "property.segments", PropertyKind::Int,
                formatInt(c.segments()), "3", "256"));
            break;
        }
        case SurfaceKind::Polygon: {
            const auto& p = static_cast<const SurfacePolygon&>(*surface);
            props.push_back(EditableProperty::fromI18n(
                "closed", "property.closed", PropertyKind::Bool,
                formatBool(p.closed())));
            break;
        }
    }

    return props;
}

Result MapWrapEditor::setSelectedProperty(const std::string& path,
                                           const std::string& jsonValue) {
    if (!impl_->document || impl_->selectedSurface.empty())
        return Result::error("No surface selected");

    auto surface = impl_->document->getSurface(impl_->selectedSurface);
    if (!surface) return Result::error("Surface not found");
    if (path != "locked" && isEffectivelyLocked(*impl_->document, *surface)) {
        return Result::error("Selected surface is locked");
    }

    if (path == "surfaceKind" || path == "kind") {
        SurfaceKind target = surface->kind();
        if (!surfaceKindFromPropertyString(jsonValue, target))
            return Result::error("Unknown surface kind: " + jsonValue);
        return convertSelectedTo(target);
    }

    // Get the old value for undo
    std::string oldValue;

    // Read old value by temporarily applying and reading
    // (Simpler: just read the current property value)
    if (path == "name") oldValue = surface->name();
    else if (path == "visible") oldValue = formatBool(surface->isVisible());
    else if (path == "locked") oldValue = formatBool(surface->isLocked());
    else if (path == "opacity") oldValue = formatFloat(surface->opacity());
    else if (path == "blend.enabled") oldValue = formatBool(surface->blend().enabled);
    else if (path == "blend.opacity") oldValue = formatFloat(surface->blend().opacity);
    else if (path == "blend.brightness") oldValue = formatFloat(surface->blend().brightness);
    else if (path == "colorCorrection.enabled") oldValue = formatBool(surface->colorCorrection().enabled);
    else if (path == "colorCorrection.brightness") oldValue = formatFloat(surface->colorCorrection().brightness);
    else if (path == "colorCorrection.contrast") oldValue = formatFloat(surface->colorCorrection().contrast);
    else if (path == "colorCorrection.saturation") oldValue = formatFloat(surface->colorCorrection().saturation);
    else {
        // Type-specific — use applyProperty to both read old value and apply
        oldValue = applyProperty(*surface, path, jsonValue);
        impl_->document->markDirty();
        if (impl_->undoStack) {
            impl_->undoStack->pushAlreadyExecuted(std::make_unique<PropertyEditCommand>(
                impl_->document, impl_->selectedSurface,
                path, oldValue, jsonValue,
                tr("command.edit_property")));
        }
        return Result::success();
    }

    // Apply the change
    applyProperty(*surface, path, jsonValue);
    impl_->document->markDirty();

    if (impl_->undoStack) {
        impl_->undoStack->pushAlreadyExecuted(std::make_unique<PropertyEditCommand>(
            impl_->document, impl_->selectedSurface,
            path, oldValue, jsonValue,
            tr("command.edit_property")));
    }

    return Result::success();
}

// ===========================================================================
// Section 17: Snap settings
// ===========================================================================

const SnapSettings& MapWrapEditor::snapSettings() const {
    return impl_->snapSettings;
}

void MapWrapEditor::setSnapSettings(const SnapSettings& settings) {
    impl_->snapSettings = settings;
}

} // namespace mapwrap
} // namespace tcx
