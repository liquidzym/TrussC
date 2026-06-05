#pragma once

#include <TrussC.h>
#include <tcxMapWrap.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace mapwrap_demo {

struct DrawOptions {
    bool showSurfaceNames = true;
    bool showWireframe = true;
    bool showHandles = true;
    bool showMasks = true;
    bool showOutputBounds = false;
    float handleRadius = 5.0f;
};

inline bool isBitmapSafeText(const std::string& text) {
    for (unsigned char ch : text) {
        if (ch == '\n' || ch == '\t') continue;
        if (ch < 32 || ch >= 127) return false;
    }
    return true;
}

inline std::string bitmapText(const std::string& text, const std::string& fallback = "") {
    if (isBitmapSafeText(text)) return text;
    if (!fallback.empty()) return fallback;

    std::string out;
    out.reserve(text.size());
    bool lastWasReplacement = false;
    for (unsigned char ch : text) {
        if (ch == '\n' || ch == '\t' || (ch >= 32 && ch < 127)) {
            out.push_back(static_cast<char>(ch));
            lastWasReplacement = false;
        } else if (!lastWasReplacement) {
            out.push_back('?');
            lastWasReplacement = true;
        }
    }
    return out.empty() ? std::string("?") : out;
}

inline void drawBitmapText(const std::string& text, float x, float y, const std::string& fallback = "") {
    tc::drawBitmapString(bitmapText(text, fallback), x, y);
}

inline const char* modeName(tcx::mapwrap::EditMode mode) {
    using tcx::mapwrap::EditMode;
    switch (mode) {
        case EditMode::Presentation: return "Presentation";
        case EditMode::SurfaceEdit: return "Surface Edit";
        case EditMode::TextureEdit: return "Texture Edit";
        case EditMode::SourceAssign: return "Source Assign";
        case EditMode::MaskEdit: return "Mask Edit";
        case EditMode::OutputEdit: return "Output Edit";
        default: return "Unknown";
    }
}

inline const char* surfaceKindName(tcx::mapwrap::SurfaceKind kind) {
    using tcx::mapwrap::SurfaceKind;
    switch (kind) {
        case SurfaceKind::Quad: return "Quad";
        case SurfaceKind::Grid: return "Grid";
        case SurfaceKind::Bezier: return "Bezier";
        case SurfaceKind::Triangle: return "Triangle";
        case SurfaceKind::Circle: return "Circle";
        case SurfaceKind::Polygon: return "Polygon";
        default: return "Surface";
    }
}

inline const char* sourceKindName(tcx::mapwrap::SourceKind kind) {
    using tcx::mapwrap::SourceKind;
    switch (kind) {
        case SourceKind::Texture: return "Texture";
        case SourceKind::Fbo: return "FBO";
        case SourceKind::Video: return "Video";
        case SourceKind::Image: return "Image";
        case SourceKind::Generated: return "Generated";
        case SourceKind::BuiltinPattern: return "Pattern";
        default: return "Source";
    }
}

inline std::string surfaceLabel(const tcx::mapwrap::Surface& surface) {
    return bitmapText(surface.name(), std::string(surfaceKindName(surface.kind())) + " " + surface.id());
}

inline std::string sourceLabel(const tcx::mapwrap::Source& source) {
    return bitmapText(source.name(), std::string(sourceKindName(source.kind())) + " " + source.id());
}

inline tcx::mapwrap::Vec2 screenPoint(tcx::mapwrap::Vec2 p, float width, float height) {
    return tcx::mapwrap::Vec2(p.x * width, p.y * height);
}

inline tc::Color mixColor(tc::Color a, tc::Color b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return tc::Color(a.r + (b.r - a.r) * t,
                     a.g + (b.g - a.g) * t,
                     a.b + (b.b - a.b) * t,
                     a.a + (b.a - a.a) * t);
}

inline tc::Color baseColorForSourceKind(tcx::mapwrap::SourceKind kind, int sourceIndex) {
    static const tc::Color palette[] = {
        tc::Color(0.10f, 0.72f, 0.95f, 0.92f),
        tc::Color(0.95f, 0.32f, 0.58f, 0.92f),
        tc::Color(0.96f, 0.72f, 0.18f, 0.92f),
        tc::Color(0.36f, 0.86f, 0.43f, 0.92f),
        tc::Color(0.58f, 0.46f, 0.96f, 0.92f),
        tc::Color(0.95f, 0.48f, 0.18f, 0.92f)
    };

    switch (kind) {
        case tcx::mapwrap::SourceKind::Image: return tc::Color(0.22f, 0.60f, 0.96f, 0.92f);
        case tcx::mapwrap::SourceKind::Video: return tc::Color(0.96f, 0.30f, 0.42f, 0.92f);
        case tcx::mapwrap::SourceKind::Fbo: return tc::Color(0.28f, 0.88f, 0.62f, 0.92f);
        case tcx::mapwrap::SourceKind::Generated: return tc::Color(0.92f, 0.48f, 0.96f, 0.92f);
        case tcx::mapwrap::SourceKind::BuiltinPattern: return tc::Color(0.96f, 0.80f, 0.20f, 0.92f);
        default: return palette[sourceIndex % 6];
    }
}

inline int sourceIndex(const tcx::mapwrap::SourceRegistry& sources, const tcx::mapwrap::SourceId& id) {
    auto all = sources.all();
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i] && all[i]->id() == id) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

inline tc::Color vertexColor(float u, float v, tc::Color base, float alpha) {
    int cellX = static_cast<int>(std::floor(u * 10.0f));
    int cellY = static_cast<int>(std::floor(v * 6.0f));
    bool checker = ((cellX + cellY) & 1) != 0;
    tc::Color dark(base.r * 0.35f, base.g * 0.35f, base.b * 0.35f, alpha);
    tc::Color light(std::min(1.0f, base.r + 0.34f),
                    std::min(1.0f, base.g + 0.34f),
                    std::min(1.0f, base.b + 0.34f),
                    alpha);
    tc::Color c = checker ? light : dark;
    c = mixColor(c, tc::Color(u, v, 1.0f - u * 0.55f, alpha), 0.22f);
    return c;
}

inline bool renderDataBounds(const tcx::mapwrap::SurfaceRenderData& rd,
                             float width,
                             float height,
                             tcx::mapwrap::Rect& out) {
    if (rd.vertices.size() < 2) return false;

    float minX = rd.vertices[0] * width;
    float maxX = minX;
    float minY = rd.vertices[1] * height;
    float maxY = minY;

    for (size_t i = 0; i + 1 < rd.vertices.size(); i += 2) {
        float x = rd.vertices[i] * width;
        float y = rd.vertices[i + 1] * height;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    out = tcx::mapwrap::Rect(minX, minY, maxX - minX, maxY - minY);
    return true;
}

inline void drawRectOutline(const tcx::mapwrap::Rect& r) {
    tc::drawLine(r.x, r.y, r.x + r.w, r.y);
    tc::drawLine(r.x + r.w, r.y, r.x + r.w, r.y + r.h);
    tc::drawLine(r.x + r.w, r.y + r.h, r.x, r.y + r.h);
    tc::drawLine(r.x, r.y + r.h, r.x, r.y);
}

inline void drawHandle(tcx::mapwrap::Vec2 p,
                       float width,
                       float height,
                       float radius,
                       bool active = false) {
    auto s = screenPoint(p, width, height);
    if (active) {
        float pulse = 0.5f + 0.5f * std::sin(tc::getElapsedTimef() * 6.0f);
        tc::setColor(0.08f, 0.90f, 1.0f, 0.34f + 0.28f * pulse);
        tc::drawCircle(s.x, s.y, radius + 7.0f + pulse * 3.0f);
        tc::setColor(0.0f, 0.0f, 0.0f, 0.85f);
        tc::drawCircle(s.x, s.y, radius + 3.0f);
        tc::setColor(1.0f, 0.88f, 0.18f, 1.0f);
        tc::drawCircle(s.x, s.y, radius + 1.0f);
        return;
    }
    tc::setColor(0.02f, 0.03f, 0.04f, 0.9f);
    tc::drawCircle(s.x, s.y, radius + 2.0f);
    tc::setColor(1.0f, 1.0f, 1.0f, 0.95f);
    tc::drawCircle(s.x, s.y, radius);
}

inline void drawSurfaceHandles(const tcx::mapwrap::Surface& surface,
                               float width,
                               float height,
                               float radius,
                               tcx::mapwrap::HandleKind activeKind = tcx::mapwrap::HandleKind::None,
                               int activeIndex = -1) {
    using namespace tcx::mapwrap;

    if (auto q = dynamic_cast<const SurfaceQuad*>(&surface)) {
        for (int i = 0; i < 4; ++i) {
            bool active = (activeKind == HandleKind::Vertex ||
                           activeKind == HandleKind::TextureVertex) &&
                          activeIndex == i;
            drawHandle(q->destinationPoints()[i], width, height, radius, active);
        }
    } else if (auto t = dynamic_cast<const SurfaceTriangle*>(&surface)) {
        for (int i = 0; i < 3; ++i) {
            bool active = (activeKind == HandleKind::Vertex ||
                           activeKind == HandleKind::TextureVertex) &&
                          activeIndex == i;
            drawHandle(t->destinationPoints()[i], width, height, radius, active);
        }
    } else if (auto g = dynamic_cast<const SurfaceGrid*>(&surface)) {
        for (int row = 0; row <= g->rows(); ++row)
            for (int col = 0; col <= g->cols(); ++col) {
                int index = row * (g->cols() + 1) + col;
                drawHandle(g->gridPoint(col, row), width, height,
                           std::max(3.0f, radius - 1.0f),
                           activeKind == HandleKind::GridPoint && activeIndex == index);
            }
    } else if (auto b = dynamic_cast<const SurfaceBezier*>(&surface)) {
        tc::setColor(0.20f, 0.95f, 1.0f, 0.72f);
        for (int row = 0; row < b->controlRows(); ++row) {
            for (int col = 0; col + 1 < b->controlCols(); ++col) {
                auto a = screenPoint(b->controlPoint(col, row), width, height);
                auto c = screenPoint(b->controlPoint(col + 1, row), width, height);
                tc::drawLine(a.x, a.y, c.x, c.y);
            }
        }
        for (int col = 0; col < b->controlCols(); ++col) {
            for (int row = 0; row + 1 < b->controlRows(); ++row) {
                auto a = screenPoint(b->controlPoint(col, row), width, height);
                auto c = screenPoint(b->controlPoint(col, row + 1), width, height);
                tc::drawLine(a.x, a.y, c.x, c.y);
            }
        }
        for (int row = 0; row < b->controlRows(); ++row)
            for (int col = 0; col < b->controlCols(); ++col) {
                int index = row * b->controlCols() + col;
                drawHandle(b->controlPoint(col, row), width, height,
                           std::max(3.0f, radius - 1.0f),
                           activeKind == HandleKind::GridPoint && activeIndex == index);
            }
    } else if (auto c = dynamic_cast<const SurfaceCircle*>(&surface)) {
        drawHandle(c->center(), width, height, radius,
                   activeKind == HandleKind::Vertex && activeIndex == 0);
        drawHandle(Vec2(c->center().x + c->radiusX(), c->center().y), width, height, radius,
                   activeKind == HandleKind::Vertex && activeIndex == 1);
        drawHandle(Vec2(c->center().x, c->center().y + c->radiusY()), width, height, radius,
                   activeKind == HandleKind::Vertex && activeIndex == 2);
    } else if (auto p = dynamic_cast<const SurfacePolygon*>(&surface)) {
        const auto& points = p->destinationPoints();
        for (int i = 0; i < static_cast<int>(points.size()); ++i) {
            bool active = (activeKind == HandleKind::Vertex ||
                           activeKind == HandleKind::TextureVertex) &&
                          activeIndex == i;
            drawHandle(points[i], width, height, radius, active);
        }
    }
}

inline tcx::mapwrap::Vec2 maskPointToScreen(tcx::mapwrap::Vec2 p, const tcx::mapwrap::Rect& bounds) {
    return tcx::mapwrap::Vec2(bounds.x + p.x * bounds.w, bounds.y + p.y * bounds.h);
}

inline void drawMasks(const tcx::mapwrap::Surface& surface, const tcx::mapwrap::Rect& bounds) {
    using namespace tcx::mapwrap;

    for (const auto& mask : surface.masks()) {
        if (!mask.enabled) continue;

        if (mask.operation == MaskOperation::Subtract) {
            tc::setColor(1.0f, 0.22f, 0.18f, 0.35f);
        } else if (mask.operation == MaskOperation::Intersect) {
            tc::setColor(0.20f, 0.55f, 1.0f, 0.35f);
        } else {
            tc::setColor(1.0f, 0.92f, 0.18f, 0.35f);
        }

        if (mask.inverted) {
            tc::setColor(0.10f, 0.10f, 0.10f, 0.35f);
        }

        if (mask.kind == MaskKind::Rectangle) {
            Rect r(bounds.x + mask.rect.x * bounds.w,
                   bounds.y + mask.rect.y * bounds.h,
                   mask.rect.w * bounds.w,
                   mask.rect.h * bounds.h);
            tc::drawRect(r.x, r.y, r.w, r.h);
            tc::setColor(1.0f, 1.0f, 1.0f, 0.75f);
            drawRectOutline(r);
        } else if (mask.kind == MaskKind::AlphaTexture) {
            Rect r(bounds.x + mask.rect.x * bounds.w,
                   bounds.y + mask.rect.y * bounds.h,
                   mask.rect.w * bounds.w,
                   mask.rect.h * bounds.h);
            tc::setColor(0.10f, 0.85f, 1.0f, 0.78f);
            drawRectOutline(r);
            tc::drawLine(r.x, r.y, r.x + r.w, r.y + r.h);
            tc::drawLine(r.x + r.w, r.y, r.x, r.y + r.h);
        } else if (mask.kind == MaskKind::Ellipse) {
            float cx = bounds.x + (mask.rect.x + mask.rect.w * 0.5f) * bounds.w;
            float cy = bounds.y + (mask.rect.y + mask.rect.h * 0.5f) * bounds.h;
            tc::drawEllipse(cx, cy, mask.rect.w * bounds.w * 0.5f, mask.rect.h * bounds.h * 0.5f);
        } else if ((mask.kind == MaskKind::Polygon ||
                    mask.kind == MaskKind::Bezier ||
                    mask.kind == MaskKind::Freehand) &&
                   mask.points.size() >= 2) {
            for (size_t i = 0; i < mask.points.size(); ++i) {
                auto a = maskPointToScreen(mask.points[i], bounds);
                auto b = maskPointToScreen(mask.points[(i + 1) % mask.points.size()], bounds);
                tc::drawLine(a.x, a.y, b.x, b.y);
                tc::drawCircle(a.x, a.y, 4.0f);
            }
        }
    }
}

inline void drawOutputBounds(const tcx::mapwrap::MapWrapDocument& document,
                             float width,
                             float height) {
    const auto& outputs = document.stage().outputs();
    for (const auto& out : outputs) {
        const auto& r = out.canvasRegionNorm;
        tcx::mapwrap::Rect screenRect(r.x * width, r.y * height, r.w * width, r.h * height);
        tc::setColor(0.20f, 1.0f, 0.35f, 0.22f);
        tc::drawRect(screenRect.x, screenRect.y, screenRect.w, screenRect.h);
        tc::setColor(0.40f, 1.0f, 0.55f, 0.85f);
        drawRectOutline(screenRect);
        drawBitmapText(out.name.empty() ? std::string("Output") : out.name,
                       screenRect.x + 8.0f,
                       screenRect.y + 18.0f,
                       "Output");
    }
}

inline void drawDemo(tcx::mapwrap::MapWrapEngine& engine,
                     float width,
                     float height,
                     const DrawOptions& options = {}) {
    using namespace tcx::mapwrap;

    engine.draw();
    const bool gpuFillSubmitted = engine.renderer().lastDrawSubmittedGpu();

    if (options.showOutputBounds) {
        drawOutputBounds(engine.document(), width, height);
    }

    const auto& surfaces = engine.document().surfaces();
    for (const auto& surface : surfaces) {
        if (!surface || !surface->isVisible()) continue;

        const SurfaceRenderData* rd = engine.renderer().surfaceRenderData(surface->id());
        if (!rd || rd->vertices.empty() || rd->indices.empty()) continue;

        auto src = engine.sources().get(surface->source());
        SourceKind kind = src ? src->kind() : SourceKind::None;
        int idx = sourceIndex(engine.sources(), surface->source());
        tc::Color base = baseColorForSourceKind(kind, idx);
        float alpha = std::max(0.08f, std::min(1.0f, surface->opacity() * surface->blend().opacity));

        tc::Mesh mesh;
        mesh.setMode(tc::PrimitiveMode::Triangles);
        for (size_t i = 0; i + 1 < rd->vertices.size(); i += 2) {
            size_t vi = i / 2;
            float u = (vi * 2 + 1 < rd->uvs.size()) ? rd->uvs[vi * 2] : 0.0f;
            float v = (vi * 2 + 1 < rd->uvs.size()) ? rd->uvs[vi * 2 + 1] : 0.0f;
            mesh.addVertex(rd->vertices[i] * width, rd->vertices[i + 1] * height, 0.0f);
            mesh.addColor(vertexColor(u, v, base, alpha));
        }
        for (auto index : rd->indices) {
            mesh.addIndex(index);
        }

        if (!gpuFillSubmitted) {
            mesh.draw();
        }

        if (options.showWireframe) {
            tc::setColor(1.0f, 1.0f, 1.0f, 0.42f);
            mesh.drawWireframe();
        }

        tcx::mapwrap::Rect bounds;
        if (renderDataBounds(*rd, width, height, bounds)) {
            if (options.showMasks) {
                drawMasks(*surface, bounds);
            }

            if (options.showSurfaceNames) {
                tc::setColor(1.0f, 1.0f, 1.0f, 0.9f);
                drawBitmapText(surfaceLabel(*surface), bounds.x + 8.0f, bounds.y + 18.0f);
                if (src) {
                    tc::setColor(0.85f, 0.90f, 0.95f, 0.78f);
                    drawBitmapText(sourceLabel(*src), bounds.x + 8.0f, bounds.y + 34.0f);
                }
            }
        }

        if (options.showHandles) {
            bool selected = engine.editor().selectedSurface() == surface->id();
            drawSurfaceHandles(*surface, width, height, options.handleRadius,
                               selected ? engine.editor().selectedHandleKind() : HandleKind::None,
                               selected ? engine.editor().selectedHandleIndex() : -1);
        }
    }
}

} // namespace mapwrap_demo
