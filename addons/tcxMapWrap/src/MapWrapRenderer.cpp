// =============================================================================
// tcxMapWrap — MapWrapRenderer.cpp Implementation
// =============================================================================
// The renderer always prepares per-surface mesh data. When compiled inside a
// TrussC application it also resolves Texture/Image/Fbo/Video/Pattern sources
// and submits textured meshes through TrussC's Mesh::draw(Texture&).
// =============================================================================

#include "tcxMapWrap/MapWrapRenderer.h"
#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/SourceTexture.h"
#include "tcxMapWrap/SourceFbo.h"
#include "tcxMapWrap/SourceVideo.h"
#include "tcxMapWrap/SourceImage.h"
#include "tcxMapWrap/SourceGenerated.h"
#include "tcxMapWrap/CalibrationPatterns.h"
#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapMask.h"

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
#include <TrussC.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <unordered_set>

namespace tcx {
namespace mapwrap {

namespace {

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

static float smoothCoverage(float signedDistance, float feather) {
    if (feather <= 1e-6f) return signedDistance >= 0.0f ? 1.0f : 0.0f;
    float t = clamp01((signedDistance + feather) / (2.0f * feather));
    return t * t * (3.0f - 2.0f * t);
}

static float distance2d(Vec2 a, Vec2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

static float distToSegment(Vec2 p, Vec2 a, Vec2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-12f) return distance2d(p, a);
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
    t = clamp01(t);
    return distance2d(p, Vec2(a.x + t * dx, a.y + t * dy));
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

static float polygonSignedDistance(const std::vector<Vec2>& poly, Vec2 p) {
    if (poly.size() < 2) return -1.0f;
    float nearest = 1e9f;
    for (size_t i = 0; i < poly.size(); ++i) {
        Vec2 a = poly[i];
        Vec2 b = poly[(i + 1) % poly.size()];
        nearest = std::min(nearest, distToSegment(p, a, b));
    }
    return pointInPolygon(poly, p) ? nearest : -nearest;
}

static float rectSignedDistance(Rect r, Vec2 p) {
    if (r.w <= 0.0f || r.h <= 0.0f) return -1.0f;
    float x2 = r.x + r.w;
    float y2 = r.y + r.h;
    bool inside = p.x >= r.x && p.x <= x2 && p.y >= r.y && p.y <= y2;
    if (inside) {
        float d = std::min(std::min(p.x - r.x, x2 - p.x),
                           std::min(p.y - r.y, y2 - p.y));
        return d;
    }
    float dx = std::max(std::max(r.x - p.x, 0.0f), p.x - x2);
    float dy = std::max(std::max(r.y - p.y, 0.0f), p.y - y2);
    return -std::sqrt(dx * dx + dy * dy);
}

static float ellipseSignedDistance(Rect r, Vec2 p) {
    if (r.w <= 0.0f || r.h <= 0.0f) return -1.0f;
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float rx = r.w * 0.5f;
    float ry = r.h * 0.5f;
    if (rx <= 1e-6f || ry <= 1e-6f) return -1.0f;
    float nx = (p.x - cx) / rx;
    float ny = (p.y - cy) / ry;
    float radial = std::sqrt(nx * nx + ny * ny);
    return (1.0f - radial) * std::min(rx, ry);
}

static float featherAmount(const MapWrapMask& mask, Vec2 canvasSizePixels) {
    if (mask.featherNorm > 0.0f) return mask.featherNorm;
    if (mask.featherPixels <= 0.0f) return 0.0f;
    float denom = std::max(1.0f, std::max(canvasSizePixels.x, canvasSizePixels.y));
    return mask.featherPixels / denom;
}

static Rect fullRectIfEmpty(Rect r) {
    if (r.w <= 0.0f || r.h <= 0.0f) return Rect(0, 0, 1, 1);
    return r;
}

static Vec2 maskPointForSpace(const MapWrapMask& mask, Vec2 surfaceLocal, Vec2 sourceUv, Vec2 canvasNorm) {
    switch (mask.space) {
        case MaskSpace::SurfaceLocal: return surfaceLocal;
        case MaskSpace::SourceUV:     return sourceUv;
        case MaskSpace::Canvas:       return canvasNorm;
        case MaskSpace::Output:       return canvasNorm;
    }
    return surfaceLocal;
}

static bool hasEnabledMask(const Surface& surface) {
    for (const auto& mask : surface.masks()) {
        if (mask.enabled) return true;
    }
    return false;
}

static bool hasPositiveMask(const Surface& surface) {
    for (const auto& mask : surface.masks()) {
        if (mask.enabled && mask.operation != MaskOperation::Subtract) return true;
    }
    return false;
}

static int maskedSubdivisionFor(const Surface& surface, const PerformanceSettings& perf) {
    if (!hasEnabledMask(surface)) return 1;
    int target = 32;
    if (perf.mode == PerformanceMode::Quality) target = 48;
    if (perf.mode == PerformanceMode::LowPower) target = 16;
    return std::max(1, std::min(std::max(1, perf.maxGridSubdivision), target));
}

static std::string sourceIdOf(const std::shared_ptr<Source>& source) {
    return source ? source->id() : std::string();
}

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
struct CpuAlphaBuffer {
    const unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};

static float sampleAlpha(const CpuAlphaBuffer& buffer, float u, float v) {
    if (!buffer.data || buffer.width <= 0 || buffer.height <= 0 || buffer.channels <= 0) {
        return 1.0f;
    }
    u = clamp01(u);
    v = clamp01(v);
    int x = std::max(0, std::min(buffer.width - 1, static_cast<int>(std::round(u * float(buffer.width - 1)))));
    int y = std::max(0, std::min(buffer.height - 1, static_cast<int>(std::round(v * float(buffer.height - 1)))));
    int idx = (y * buffer.width + x) * buffer.channels;
    if (buffer.channels >= 4) return buffer.data[idx + 3] / 255.0f;
    if (buffer.channels == 1) return buffer.data[idx] / 255.0f;
    return 1.0f;
}

struct TextureResolve {
    trussc::Texture* texture = nullptr;
    bool placeholder = false;
};
#endif

} // namespace

// ===========================================================================
// Impl
// ===========================================================================

struct MapWrapRenderer::Impl {
    MapWrapDocument* document = nullptr;
    SourceRegistry* sources = nullptr;

    Vec2 canvasSize = Vec2(1920, 1080);
    PerformanceSettings perfSettings;
    RenderStats stats;
    bool lastSubmittedGpu = false;

    std::unordered_map<SurfaceId, SurfaceRenderData> surfaceData;
    std::vector<uint8_t> placeholderPixels;
    std::vector<SurfaceId> lastFrameSurfaceIds;

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    struct PatternRuntime {
        trussc::Texture texture;
        std::vector<uint8_t> pixels;
        BuiltinPatternKind kind = BuiltinPatternKind::Grid;
        int width = 0;
        int height = 0;
        uint64_t updatedFrame = 0;
        bool valid = false;
    };

    trussc::Texture placeholderTexture;
    bool placeholderTextureReady = false;
    std::unordered_map<SourceId, PatternRuntime> patternTextures;
    std::unordered_map<SourceId, PatternRuntime> generatedTextures;
    std::unordered_map<SurfaceId, trussc::Mesh> gpuMeshes;
#endif

    static constexpr int kPlaceholderSize = 8;

    void initPlaceholder() {
        placeholderPixels.resize(kPlaceholderSize * kPlaceholderSize * 4, 0);
        generateCheckerboard(placeholderPixels.data(), kPlaceholderSize, kPlaceholderSize, 2, 2);
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
        placeholderTextureReady = false;
#endif
    }

    std::shared_ptr<Source> resolveSource(const SourceId& sourceId) const {
        if (!sources || sourceId.empty()) return nullptr;
        return sources->get(sourceId);
    }

    void syncVideoVisibility() {
        if (!sources || !document) return;

        std::unordered_set<SourceId> visibleSources;
        for (const auto& surface : document->surfaces()) {
            if (!surface || !surface->isVisible()) continue;
            if (!surface->source().empty()) visibleSources.insert(surface->source());
        }

        for (auto& source : sources->all()) {
            if (auto video = std::dynamic_pointer_cast<SourceVideo>(source)) {
                video->setRenderVisible(visibleSources.find(video->id()) != visibleSources.end(),
                                        perfSettings.pauseVideoWhenHidden);
            }
        }
    }

    void rebuildMesh(Surface& surface) {
        auto it = surfaceData.find(surface.id());
        if (it == surfaceData.end()) {
            SurfaceRenderData data;
            data.surfaceId = surface.id();
            data.dirty = false;
            surfaceData[surface.id()] = std::move(data);
            it = surfaceData.find(surface.id());
        }

        MeshBuildContext ctx;
        ctx.canvasSizePixels = canvasSize;
        ctx.meshSubdivision = maskedSubdivisionFor(surface, perfSettings);

        MeshBuildResult result = surface.buildMesh(ctx);
        SurfaceRenderData& rd = it->second;
        if (result.ok) {
            rd.vertices = std::move(result.mesh.vertices);
            rd.uvs = std::move(result.mesh.uvs);
            rd.indices = std::move(result.mesh.indices);
            rd.maskAlphas.assign(rd.vertices.size() / 2, 1.0f);
        } else {
            rd.vertices.clear();
            rd.uvs.clear();
            rd.indices.clear();
            rd.maskAlphas.clear();
        }
        rd.surfaceRevision = surface.revision();
        rd.dirty = false;
        rd.hasActiveMask = hasEnabledMask(surface);
        surface.clearDirty();
    }

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    trussc::Texture* ensurePlaceholderTexture() {
        if (placeholderTextureReady && placeholderTexture.isAllocated()) return &placeholderTexture;
        trussc::Pixels pixels;
        pixels.setFromPixels(placeholderPixels.data(), kPlaceholderSize, kPlaceholderSize, 4);
        placeholderTexture.allocate(pixels, trussc::TextureUsage::Immutable);
        placeholderTextureReady = placeholderTexture.isAllocated();
        return placeholderTextureReady ? &placeholderTexture : nullptr;
    }

    PatternRuntime* ensurePatternTexture(CalibrationPatternSource& pattern) {
        SourceId id = pattern.id();
        int w = std::max(1, static_cast<int>(std::round(pattern.size().x)));
        int h = std::max(1, static_cast<int>(std::round(pattern.size().y)));
        BuiltinPatternKind kind = pattern.pattern();
        auto& runtime = patternTextures[id];
        if (runtime.valid && runtime.width == w && runtime.height == h && runtime.kind == kind) {
            return &runtime;
        }

        runtime.pixels.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
        if (!pattern.generatePixels(runtime.pixels.data(), w, h)) {
            runtime.valid = false;
            return nullptr;
        }

        trussc::Pixels pixels;
        pixels.setFromPixels(runtime.pixels.data(), w, h, 4);
        runtime.texture.allocate(pixels, trussc::TextureUsage::Immutable, true);
        runtime.width = w;
        runtime.height = h;
        runtime.kind = kind;
        runtime.valid = runtime.texture.isAllocated();
        return runtime.valid ? &runtime : nullptr;
    }

    PatternRuntime* ensureGeneratedTexture(SourceGenerated& generated) {
        SourceId id = generated.id();
        int w = std::max(1, static_cast<int>(std::round(generated.size().x)));
        int h = std::max(1, static_cast<int>(std::round(generated.size().y)));
        auto& runtime = generatedTextures[id];
        uint64_t frame = trussc::getFrameCount();
        if (runtime.valid && runtime.width == w && runtime.height == h && runtime.updatedFrame == frame) {
            return &runtime;
        }

        runtime.pixels.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
        if (!generated.generatePixels(runtime.pixels.data(), w, h)) {
            double time = generated.elapsedSeconds();
            for (int y = 0; y < h; ++y) {
                float v = h > 1 ? static_cast<float>(y) / static_cast<float>(h - 1) : 0.0f;
                for (int x = 0; x < w; ++x) {
                    float u = w > 1 ? static_cast<float>(x) / static_cast<float>(w - 1) : 0.0f;
                    float wave = 0.5f + 0.5f * std::sin(static_cast<float>(time) * 2.2f + u * 9.0f + v * 6.0f);
                    float ringDx = u - 0.5f;
                    float ringDy = v - 0.5f;
                    float ring = 0.5f + 0.5f * std::cos(std::sqrt(ringDx * ringDx + ringDy * ringDy) * 36.0f -
                                                         static_cast<float>(time) * 5.0f);
                    int idx = (y * w + x) * 4;
                    runtime.pixels[idx + 0] = static_cast<uint8_t>(std::min(255.0f, (0.18f + u * 0.55f + wave * 0.25f) * 255.0f));
                    runtime.pixels[idx + 1] = static_cast<uint8_t>(std::min(255.0f, (0.20f + v * 0.50f + ring * 0.28f) * 255.0f));
                    runtime.pixels[idx + 2] = static_cast<uint8_t>(std::min(255.0f, (0.35f + wave * 0.45f) * 255.0f));
                    runtime.pixels[idx + 3] = 255;
                }
            }
        }

        trussc::Pixels pixels;
        pixels.setFromPixels(runtime.pixels.data(), w, h, 4);
        bool needsAllocate = !runtime.valid || runtime.width != w || runtime.height != h;
        if (needsAllocate) {
            runtime.texture.allocate(pixels, trussc::TextureUsage::Dynamic);
            runtime.width = w;
            runtime.height = h;
            runtime.kind = BuiltinPatternKind::SolidColor;
            runtime.valid = runtime.texture.isAllocated();
        }
        if (runtime.valid) {
            runtime.texture.loadData(pixels);
            runtime.updatedFrame = frame;
        }
        return runtime.valid ? &runtime : nullptr;
    }

    std::optional<CpuAlphaBuffer> alphaBufferFor(const SourceId& id) {
        auto source = resolveSource(id);
        if (!source) return std::nullopt;

        if (auto image = std::dynamic_pointer_cast<SourceImage>(source)) {
            if (!image->ensureLoaded()) return std::nullopt;
            CpuAlphaBuffer b{ image->pixelsData(), image->pixelsWidth(), image->pixelsHeight(), image->pixelsChannels() };
            return b.data ? std::optional<CpuAlphaBuffer>(b) : std::nullopt;
        }

        if (auto video = std::dynamic_pointer_cast<SourceVideo>(source)) {
            if (!video->ensureLoaded()) return std::nullopt;
            CpuAlphaBuffer b{ video->pixelsData(), video->pixelsWidth(), video->pixelsHeight(), video->pixelsChannels() };
            return b.data ? std::optional<CpuAlphaBuffer>(b) : std::nullopt;
        }

        if (auto pattern = std::dynamic_pointer_cast<CalibrationPatternSource>(source)) {
            PatternRuntime* rt = ensurePatternTexture(*pattern);
            if (!rt || !rt->valid) return std::nullopt;
            CpuAlphaBuffer b{ rt->pixels.data(), rt->width, rt->height, 4 };
            return b.data ? std::optional<CpuAlphaBuffer>(b) : std::nullopt;
        }

        return std::nullopt;
    }

    TextureResolve textureForSource(const std::shared_ptr<Source>& source) {
        auto placeholder = [this]() -> TextureResolve {
            return TextureResolve{ ensurePlaceholderTexture(), true };
        };

        if (!source) return placeholder();

        if (auto textureSource = std::dynamic_pointer_cast<SourceTexture>(source)) {
            if (textureSource->hasTexture()) {
                return TextureResolve{ static_cast<trussc::Texture*>(textureSource->texture()), false };
            }
            return placeholder();
        }

        if (auto fboSource = std::dynamic_pointer_cast<SourceFbo>(source)) {
            if (fboSource->hasFbo()) {
                auto* fbo = static_cast<trussc::Fbo*>(fboSource->fbo());
                return fbo ? TextureResolve{ &fbo->getTexture(), false } : placeholder();
            }
            return placeholder();
        }

        if (auto image = std::dynamic_pointer_cast<SourceImage>(source)) {
            if (image->ensureLoaded()) {
                return TextureResolve{ static_cast<trussc::Texture*>(image->textureHandle()), false };
            }
            return placeholder();
        }

        if (auto video = std::dynamic_pointer_cast<SourceVideo>(source)) {
            if (video->ensureLoaded()) {
                return TextureResolve{ static_cast<trussc::Texture*>(video->textureHandle()), false };
            }
            return placeholder();
        }

        if (auto pattern = std::dynamic_pointer_cast<CalibrationPatternSource>(source)) {
            PatternRuntime* rt = ensurePatternTexture(*pattern);
            return (rt && rt->valid) ? TextureResolve{ &rt->texture, false } : placeholder();
        }

        if (auto generated = std::dynamic_pointer_cast<SourceGenerated>(source)) {
            PatternRuntime* rt = ensureGeneratedTexture(*generated);
            return (rt && rt->valid) ? TextureResolve{ &rt->texture, false } : placeholder();
        }

        return placeholder();
    }

    void buildGpuMesh(const Surface& surface, SurfaceRenderData& rd, float width, float height) {
        auto& mesh = gpuMeshes[surface.id()];
        mesh.clear();
        mesh.setMode(trussc::PrimitiveMode::Triangles);

        Rect src = surface.sourceRect();
        float baseOpacity = clamp01(surface.opacity() * surface.blend().opacity);
        float brightness = std::max(0.0f, surface.blend().brightness);
        ColorCorrection cc = surface.colorCorrection();
        if (cc.enabled) {
            baseOpacity *= clamp01(cc.opacity);
            brightness *= std::max(0.0f, cc.brightness);
        }

        size_t vertexCount = rd.vertices.size() / 2;
        for (size_t vi = 0; vi < vertexCount; ++vi) {
            float x = rd.vertices[vi * 2] * width;
            float y = rd.vertices[vi * 2 + 1] * height;
            float u = (vi * 2 + 1 < rd.uvs.size()) ? rd.uvs[vi * 2] : 0.0f;
            float v = (vi * 2 + 1 < rd.uvs.size()) ? rd.uvs[vi * 2 + 1] : 0.0f;
            float maskAlpha = (vi < rd.maskAlphas.size()) ? rd.maskAlphas[vi] : 1.0f;
            float alpha = clamp01(baseOpacity * maskAlpha);

            mesh.addVertex(x, y, 0.0f);
            mesh.addTexCoord(src.x + u * src.w, src.y + v * src.h);
            mesh.addColor(clamp01(brightness), clamp01(brightness), clamp01(brightness), alpha);
        }
        for (auto index : rd.indices) {
            mesh.addIndex(index);
        }
    }

    void submitGpuSurface(const Surface& surface, SurfaceRenderData& rd, const std::shared_ptr<Source>& source) {
        if (!sg_isvalid() || trussc::headless::isActive()) return;

        TextureResolve resolved = textureForSource(source);
        trussc::Texture* texture = resolved.texture;
        if (!texture || !texture->isAllocated()) return;

        buildGpuMesh(surface, rd, canvasSize.x, canvasSize.y);
        trussc::Mesh& mesh = gpuMeshes[surface.id()];
        if (mesh.getNumVertices() == 0 || mesh.getNumIndices() == 0) return;

        // Map tcxMapWrap blend modes to TrussC's currently available pipelines.
        // Normal alpha blending is the default; unsupported modes intentionally
        // degrade to normal rather than breaking platform portability.
        trussc::BlendMode prevBlend = trussc::getBlendMode();
        trussc::setBlendMode(trussc::BlendMode::Alpha);
        mesh.draw(*texture);
        trussc::setBlendMode(prevBlend);

        lastSubmittedGpu = true;
        if (resolved.placeholder) stats.placeholderDrawCount++;
        else stats.texturedDrawCount++;
    }
#endif

    float alphaTextureCoverage(const MapWrapMask& mask, Vec2 p, float shapeCoverage) {
        if (shapeCoverage <= 0.0f) return 0.0f;
        Rect r = fullRectIfEmpty(mask.rect);
        if (r.w <= 0.0f || r.h <= 0.0f) return 0.0f;
        float u = (p.x - r.x) / r.w;
        float v = (p.y - r.y) / r.h;
        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return 0.0f;

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
        if (!mask.alphaTextureSource.empty()) {
            auto alpha = alphaBufferFor(mask.alphaTextureSource);
            if (alpha) return shapeCoverage * sampleAlpha(*alpha, u, v);
        }
#else
        (void)u;
        (void)v;
#endif
        return shapeCoverage;
    }

    float singleMaskCoverage(const MapWrapMask& mask, Vec2 surfaceLocal, Vec2 sourceUv, Vec2 canvasNorm) {
        Vec2 p = maskPointForSpace(mask, surfaceLocal, sourceUv, canvasNorm);
        float feather = featherAmount(mask, canvasSize);
        float coverage = 0.0f;

        switch (mask.kind) {
            case MaskKind::Rectangle:
                coverage = smoothCoverage(rectSignedDistance(fullRectIfEmpty(mask.rect), p), feather);
                break;
            case MaskKind::Ellipse:
                coverage = smoothCoverage(ellipseSignedDistance(fullRectIfEmpty(mask.rect), p), feather);
                break;
            case MaskKind::Polygon:
            case MaskKind::Bezier:
            case MaskKind::Freehand:
                coverage = smoothCoverage(polygonSignedDistance(mask.points, p), feather);
                break;
            case MaskKind::AlphaTexture: {
                Rect r = fullRectIfEmpty(mask.rect);
                float shape = smoothCoverage(rectSignedDistance(r, p), feather);
                coverage = alphaTextureCoverage(mask, p, shape);
                break;
            }
        }

        coverage = clamp01(coverage) * clamp01(mask.opacity);
        if (mask.inverted) coverage = 1.0f - coverage;
        return clamp01(coverage);
    }

    void updateMaskAlphas(Surface& surface, SurfaceRenderData& rd) {
        size_t vertexCount = rd.vertices.size() / 2;
        rd.maskAlphas.assign(vertexCount, 1.0f);
        rd.hasActiveMask = hasEnabledMask(surface);
        if (!rd.hasActiveMask) return;

        bool positive = hasPositiveMask(surface);
        std::fill(rd.maskAlphas.begin(), rd.maskAlphas.end(), positive ? 0.0f : 1.0f);

        for (const auto& mask : surface.masks()) {
            if (!mask.enabled) continue;

            stats.maskCount++;
            if (mask.kind == MaskKind::AlphaTexture) stats.alphaMaskCount++;
            if (mask.featherPixels > 0.0f || mask.featherNorm > 0.0f) stats.featheredMaskCount++;

            for (size_t vi = 0; vi < vertexCount; ++vi) {
                Vec2 canvasNorm(rd.vertices[vi * 2], rd.vertices[vi * 2 + 1]);
                Vec2 surfaceLocal(0.0f, 0.0f);
                if (vi * 2 + 1 < rd.uvs.size()) {
                    surfaceLocal = Vec2(rd.uvs[vi * 2], rd.uvs[vi * 2 + 1]);
                }
                Rect src = surface.sourceRect();
                Vec2 sourceUv(src.x + surfaceLocal.x * src.w,
                              src.y + surfaceLocal.y * src.h);
                float coverage = singleMaskCoverage(mask, surfaceLocal, sourceUv, canvasNorm);
                switch (mask.operation) {
                    case MaskOperation::Add:
                        rd.maskAlphas[vi] = std::max(rd.maskAlphas[vi], coverage);
                        break;
                    case MaskOperation::Subtract:
                        rd.maskAlphas[vi] *= (1.0f - coverage);
                        break;
                    case MaskOperation::Intersect:
                        rd.maskAlphas[vi] *= coverage;
                        break;
                }
                rd.maskAlphas[vi] = clamp01(rd.maskAlphas[vi]);
            }
        }
    }

    void drawSurface(Surface& surface, const RenderOptions& options) {
        if (!surface.isVisible()) {
            stats.skippedSurfaceCount++;
            return;
        }

        std::shared_ptr<Source> source = resolveSource(surface.source());
        if (!source && !surface.source().empty()) {
            stats.missingSourceCount++;
        }

        auto it = surfaceData.find(surface.id());
        if (it == surfaceData.end()) {
            SurfaceRenderData data;
            data.surfaceId = surface.id();
            data.dirty = true;
            surfaceData[surface.id()] = std::move(data);
            it = surfaceData.find(surface.id());
        }

        SurfaceRenderData& rd = it->second;
        if (rd.dirty || rd.surfaceRevision != surface.revision()) {
            rebuildMesh(surface);
            stats.rebuiltMeshCount++;
        }

        if (rd.indices.empty()) {
            stats.invalidSurfaceCount++;
            return;
        }

        updateMaskAlphas(surface, rd);
        stats.drawnSurfaceCount++;

#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
        if (options.submitTexturedMeshes) {
            submitGpuSurface(surface, rd, source);
        }
#else
        (void)options;
#endif
    }

    void drawOverlay(const RenderOptions& options) {
        if (!options.showOverlay || !document) return;

        for (const auto& surface : document->surfaces()) {
            if (!surface) continue;
            if (!surface->isVisible() && !options.overlayOptions.showInvisibleSurfaces) continue;
            if (surface->isLocked() && !options.overlayOptions.showLockedSurfaces) continue;

            auto it = surfaceData.find(surface->id());
            if (it == surfaceData.end() ||
                it->second.dirty ||
                it->second.surfaceRevision != surface->revision()) {
                rebuildMesh(*surface);
            }
        }
    }

    void countVideoStats() {
        if (!sources) return;
        for (auto& source : sources->all()) {
            auto video = std::dynamic_pointer_cast<SourceVideo>(source);
            if (!video) continue;
            if (video->renderVisible() && video->isPlaying()) {
                stats.activeVideoSourceCount++;
            } else if (!video->renderVisible() && video->isPlaying()) {
                stats.pausedVideoSourceCount++;
            }
        }
    }
};

// ===========================================================================
// Construction / Destruction
// ===========================================================================

MapWrapRenderer::MapWrapRenderer()
    : impl_(std::make_unique<Impl>())
{
    impl_->initPlaceholder();
}

MapWrapRenderer::~MapWrapRenderer() = default;

// ===========================================================================
// Setup
// ===========================================================================

Result MapWrapRenderer::setup(MapWrapDocument* document, SourceRegistry* sources) {
    if (!document) return Result::error("MapWrapRenderer::setup — document is null");
    if (!sources) return Result::error("MapWrapRenderer::setup — sources is null");

    impl_->document = document;
    impl_->sources = sources;

    for (const auto& surface : document->surfaces()) {
        if (surface) markDirty(surface->id());
    }

    return Result::success();
}

// ===========================================================================
// Update / Draw
// ===========================================================================

void MapWrapRenderer::update(float dt) {
    if (!impl_->document || !impl_->sources) return;
    (void)dt;

    impl_->syncVideoVisibility();

    const auto& surfaces = impl_->document->surfaces();
    for (const auto& surface : surfaces) {
        if (!surface) continue;
        if (impl_->surfaceData.find(surface->id()) == impl_->surfaceData.end()) {
            markDirty(surface->id());
        }
    }

    std::vector<SurfaceId> currentIds;
    currentIds.reserve(surfaces.size());
    std::unordered_set<SurfaceId> currentIdSet;
    currentIdSet.reserve(surfaces.size());
    for (const auto& surface : surfaces) {
        if (surface) {
            currentIds.push_back(surface->id());
            currentIdSet.insert(surface->id());
        }
    }

    for (auto it = impl_->surfaceData.begin(); it != impl_->surfaceData.end(); ) {
        if (currentIdSet.find(it->first) == currentIdSet.end()) {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
            impl_->gpuMeshes.erase(it->first);
#endif
            it = impl_->surfaceData.erase(it);
        } else {
            ++it;
        }
    }

    impl_->lastFrameSurfaceIds = std::move(currentIds);
}

void MapWrapRenderer::draw(const RenderOptions& options) {
    if (!impl_->document) return;

    impl_->stats = RenderStats{};
    impl_->lastSubmittedGpu = false;

    const auto& surfaces = impl_->document->surfaces();
    for (const auto& surface : surfaces) {
        if (surface) impl_->drawSurface(*surface, options);
    }

    impl_->drawOverlay(options);
    impl_->countVideoStats();
}

// ===========================================================================
// Canvas / Settings / Stats
// ===========================================================================

void MapWrapRenderer::setCanvasSize(Vec2 pixels) {
    if (impl_->canvasSize.x != pixels.x || impl_->canvasSize.y != pixels.y) {
        impl_->canvasSize = pixels;
        for (auto& [id, data] : impl_->surfaceData) {
            data.dirty = true;
        }
    }
}

Vec2 MapWrapRenderer::canvasSize() const {
    return impl_->canvasSize;
}

RenderStats MapWrapRenderer::stats() const {
    return impl_->stats;
}

void MapWrapRenderer::setPerformanceSettings(const PerformanceSettings& settings) {
    impl_->perfSettings = settings;
    for (auto& [id, data] : impl_->surfaceData) {
        data.dirty = true;
    }
}

PerformanceSettings MapWrapRenderer::performanceSettings() const {
    return impl_->perfSettings;
}

bool MapWrapRenderer::supportsGpuDrawing() const {
#if TCX_MAPWRAP_HAS_TRUSSC_RUNTIME
    return true;
#else
    return false;
#endif
}

bool MapWrapRenderer::lastDrawSubmittedGpu() const {
    return impl_->lastSubmittedGpu;
}

// ===========================================================================
// Per-surface render data access
// ===========================================================================

const SurfaceRenderData* MapWrapRenderer::surfaceRenderData(const SurfaceId& id) const {
    auto it = impl_->surfaceData.find(id);
    return (it != impl_->surfaceData.end()) ? &it->second : nullptr;
}

SurfaceRenderData* MapWrapRenderer::surfaceRenderData(const SurfaceId& id) {
    auto it = impl_->surfaceData.find(id);
    return (it != impl_->surfaceData.end()) ? &it->second : nullptr;
}

void MapWrapRenderer::markDirty(const SurfaceId& id) {
    auto it = impl_->surfaceData.find(id);
    if (it != impl_->surfaceData.end()) {
        it->second.dirty = true;
    } else {
        SurfaceRenderData data;
        data.surfaceId = id;
        data.dirty = true;
        impl_->surfaceData[id] = std::move(data);
    }
}

// ===========================================================================
// Placeholder texture
// ===========================================================================

const uint8_t* MapWrapRenderer::placeholderTextureData() const {
    return impl_->placeholderPixels.empty() ? nullptr : impl_->placeholderPixels.data();
}

Vec2 MapWrapRenderer::placeholderTextureSize() const {
    return Vec2(static_cast<float>(Impl::kPlaceholderSize),
                static_cast<float>(Impl::kPlaceholderSize));
}

void MapWrapRenderer::generatePlaceholder() {
    impl_->initPlaceholder();
}

// ===========================================================================
// Private helpers exposed for tests/internal callers
// ===========================================================================

std::shared_ptr<Source> MapWrapRenderer::resolveSource(const SourceId& sourceId) const {
    return impl_->resolveSource(sourceId);
}

void MapWrapRenderer::rebuildMesh(Surface& surface) {
    impl_->rebuildMesh(surface);
}

} // namespace mapwrap
} // namespace tcx
