#pragma once
// =============================================================================
// tcxMapWrap — MapWrapEngine
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapRenderer.h"
#include "tcxMapWrap/MapWrapEditor.h"
#include "tcxMapWrap/SourceRegistry.h"
#include "tcxMapWrap/UndoStack.h"

namespace tcx {
namespace mapwrap {

class MapWrapEngine {
public:
    MapWrapEngine();
    ~MapWrapEngine();

    MapWrapDocument& document();
    const MapWrapDocument& document() const;

    MapWrapRenderer& renderer();
    const MapWrapRenderer& renderer() const;

    MapWrapEditor& editor();
    const MapWrapEditor& editor() const;

    SourceRegistry& sources();
    const SourceRegistry& sources() const;

    UndoStack& undoStack();
    const UndoStack& undoStack() const;

    void update(float dt);
    void draw();

    void setCanvasSize(Vec2 pixels);
    Vec2 canvasSize() const;

    const PerformanceSettings& performanceSettings() const;
    void setPerformanceSettings(const PerformanceSettings& settings);

    const RenderStats& stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
