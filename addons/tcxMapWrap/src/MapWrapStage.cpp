// =============================================================================
// tcxMapWrap — MapWrapStage.cpp Implementation
// =============================================================================

#include "tcxMapWrap/MapWrapStage.h"
#include "tcxMapWrap/MapWrapOutput.h"
#include "tcxMapWrap/MapWrapI18n.h"

#include <algorithm>

namespace tcx {
namespace mapwrap {

// ===========================================================================
// Impl
// ===========================================================================

struct MapWrapStage::Impl {
    Vec2 designCanvasSize = Vec2(1920, 1080);
    std::vector<MapWrapOutput> outputs;
    std::vector<MapWrapMask> globalMasks;
};

// ===========================================================================
// Construction
// ===========================================================================

MapWrapStage::MapWrapStage()
    : impl_(std::make_unique<Impl>())
{
    // Auto-create the default output
    ensureDefaultOutput();
}

MapWrapStage::~MapWrapStage() = default;

// ===========================================================================
// Canvas
// ===========================================================================

Vec2 MapWrapStage::designCanvasSize() const {
    return impl_->designCanvasSize;
}

void MapWrapStage::setDesignCanvasSize(Vec2 size) {
    impl_->designCanvasSize = size;
}

// ===========================================================================
// Outputs
// ===========================================================================

std::vector<MapWrapOutput>& MapWrapStage::outputs() {
    return impl_->outputs;
}

const std::vector<MapWrapOutput>& MapWrapStage::outputs() const {
    return impl_->outputs;
}

OutputId MapWrapStage::defaultOutputId() const {
    return "output_main";
}

MapWrapOutput* MapWrapStage::getOutput(const OutputId& id) {
    for (auto& output : impl_->outputs) {
        if (output.id == id) {
            return &output;
        }
    }
    return nullptr;
}

const MapWrapOutput* MapWrapStage::getOutput(const OutputId& id) const {
    for (const auto& output : impl_->outputs) {
        if (output.id == id) {
            return &output;
        }
    }
    return nullptr;
}

MapWrapOutput& MapWrapStage::ensureDefaultOutput() {
    // Check if "output_main" already exists
    for (auto& output : impl_->outputs) {
        if (output.id == "output_main") {
            return output;
        }
    }

    // Create the default output with localized name
    MapWrapOutput mainOutput;
    mainOutput.id = "output_main";
    mainOutput.name = tr("output.main");
    mainOutput.canvasRegionNorm = Rect(0, 0, 1, 1);
    mainOutput.displayRegionPixels = Rect(0, 0, 1920, 1080);
    mainOutput.pixelSize = Vec2(1920, 1080);
    mainOutput.contentScale = 1.0f;
    mainOutput.rotationDegrees = 0.0f;
    mainOutput.enabled = true;
    mainOutput.showTestPattern = false;

    impl_->outputs.push_back(std::move(mainOutput));
    return impl_->outputs.back();
}

// ===========================================================================
// Global masks
// ===========================================================================

std::vector<MapWrapMask>& MapWrapStage::globalMasks() {
    return impl_->globalMasks;
}

const std::vector<MapWrapMask>& MapWrapStage::globalMasks() const {
    return impl_->globalMasks;
}

} // namespace mapwrap
} // namespace tcx
