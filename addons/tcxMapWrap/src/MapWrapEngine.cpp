// =============================================================================
// tcxMapWrap — MapWrapEngine.cpp Implementation
// =============================================================================

#include "tcxMapWrap/MapWrapEngine.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/MapWrapRenderer.h"
#include "tcxMapWrap/MapWrapEditor.h"
#include "tcxMapWrap/SourceRegistry.h"
#include "tcxMapWrap/UndoStack.h"

namespace tcx {
namespace mapwrap {

// ===========================================================================
// Impl
// ===========================================================================

struct MapWrapEngine::Impl {
    std::unique_ptr<MapWrapDocument> document;
    std::unique_ptr<MapWrapRenderer> renderer;
    std::unique_ptr<MapWrapEditor> editor;
    std::unique_ptr<SourceRegistry> sources;
    std::unique_ptr<UndoStack> undoStack;
    PerformanceSettings perfSettings;
    RenderStats stats;
    Vec2 canvasSize = Vec2(1920, 1080);
};

// ===========================================================================
// Construction
// ===========================================================================

MapWrapEngine::MapWrapEngine()
    : impl_(std::make_unique<Impl>())
{
    // Create all sub-systems
    impl_->document = std::make_unique<MapWrapDocument>();
    impl_->sources = std::make_unique<SourceRegistry>();
    impl_->renderer = std::make_unique<MapWrapRenderer>();
    impl_->editor = std::make_unique<MapWrapEditor>();
    impl_->undoStack = std::make_unique<UndoStack>();

    // Wire the renderer to the document + source registry
    impl_->renderer->setup(impl_->document.get(), impl_->sources.get());

    // Wire the editor to the document + undo stack
    impl_->editor->setDocument(impl_->document.get());
    impl_->editor->setUndoStack(impl_->undoStack.get());

    // Synchronize canvas size
    impl_->canvasSize = impl_->document->designCanvasSize();
}

MapWrapEngine::~MapWrapEngine() = default;

// ===========================================================================
// Sub-system access
// ===========================================================================

MapWrapDocument& MapWrapEngine::document() { return *impl_->document; }
const MapWrapDocument& MapWrapEngine::document() const { return *impl_->document; }

MapWrapRenderer& MapWrapEngine::renderer() { return *impl_->renderer; }
const MapWrapRenderer& MapWrapEngine::renderer() const { return *impl_->renderer; }

MapWrapEditor& MapWrapEngine::editor() { return *impl_->editor; }
const MapWrapEditor& MapWrapEngine::editor() const { return *impl_->editor; }

SourceRegistry& MapWrapEngine::sources() { return *impl_->sources; }
const SourceRegistry& MapWrapEngine::sources() const { return *impl_->sources; }

UndoStack& MapWrapEngine::undoStack() { return *impl_->undoStack; }
const UndoStack& MapWrapEngine::undoStack() const { return *impl_->undoStack; }

// ===========================================================================
// Main loop
// ===========================================================================

void MapWrapEngine::update(float dt) {
    // 1. Advance source clocks and update all sources (video playback, etc.)
    impl_->sources->update(dt);

    // 2. Update the renderer (mesh rebuilds, async operations, etc.)
    impl_->renderer->update(dt);

    // 3. Cache render stats
    impl_->stats = impl_->renderer->stats();
}

void MapWrapEngine::draw() {
    impl_->renderer->draw();
}

// ===========================================================================
// Canvas
// ===========================================================================

void MapWrapEngine::setCanvasSize(Vec2 pixels) {
    impl_->canvasSize = pixels;
    impl_->renderer->setCanvasSize(pixels);
    impl_->document->setDesignCanvasSize(pixels);
    auto& viewport = impl_->editor->viewport();
    viewport.viewSizePixels = pixels;
    viewport.canvasSizePixels = pixels;
}

Vec2 MapWrapEngine::canvasSize() const {
    return impl_->canvasSize;
}

// ===========================================================================
// Settings / Stats
// ===========================================================================

const PerformanceSettings& MapWrapEngine::performanceSettings() const {
    return impl_->perfSettings;
}

void MapWrapEngine::setPerformanceSettings(const PerformanceSettings& settings) {
    impl_->perfSettings = settings;
    impl_->renderer->setPerformanceSettings(settings);
}

const RenderStats& MapWrapEngine::stats() const {
    return impl_->stats;
}

} // namespace mapwrap
} // namespace tcx
