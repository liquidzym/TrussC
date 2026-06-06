#pragma once
// =============================================================================
// tcxMapWrap — MapWrapRenderer
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapInput.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/SourceRegistry.h"

#include <cstdint>
#include <unordered_map>

namespace tcx {
namespace mapwrap {

struct RenderOptions {
    bool showOverlay = false;
    bool showDebugStats = false;
    bool submitTexturedMeshes = true;
    OverlayOptions overlayOptions;
};

// Per-surface mesh data prepared for GPU upload
struct SurfaceRenderData {
    SurfaceId surfaceId;
    std::vector<float> vertices;       // x,y pairs (2 floats per vertex)
    std::vector<float> uvs;            // u,v pairs (2 floats per vertex)
    std::vector<uint32_t> baseIndices; // unmasked triangle indices
    std::vector<uint32_t> indices;     // triangle indices
    std::vector<float> maskAlphas;     // one alpha coverage value per vertex
    uint64_t surfaceRevision = 0;
    bool dirty = true;
    bool hasActiveMask = false;
};

class MapWrapRenderer {
public:
    MapWrapRenderer();
    ~MapWrapRenderer();

    Result setup(MapWrapDocument* document, SourceRegistry* sources);
    void update(float dt);
    void draw(const RenderOptions& options = {});
    void setCanvasSize(Vec2 pixels);
    Vec2 canvasSize() const;

    RenderStats stats() const;
    void setPerformanceSettings(const PerformanceSettings& settings);
    PerformanceSettings performanceSettings() const;
    bool supportsGpuDrawing() const;
    bool lastDrawSubmittedGpu() const;

    // Access per-surface render data for external GPU submission
    const SurfaceRenderData* surfaceRenderData(const SurfaceId& id) const;
    SurfaceRenderData* surfaceRenderData(const SurfaceId& id);

    // Mark a surface's mesh as needing rebuild
    void markDirty(const SurfaceId& id);

    // Access the prepared placeholder texture data (RGBA8 checkerboard)
    // Returns nullptr if not yet generated; call generatePlaceholder() first.
    const uint8_t* placeholderTextureData() const;
    Vec2 placeholderTextureSize() const;

    // Generate the default placeholder texture (8×8 checkerboard)
    void generatePlaceholder();

private:
    void rebuildMesh(Surface& surface);
    void generateCheckerboardPlaceholder();
    std::shared_ptr<Source> resolveSource(const SourceId& sourceId) const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
