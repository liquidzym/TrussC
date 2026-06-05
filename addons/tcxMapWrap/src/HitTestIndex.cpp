// =============================================================================
// tcxMapWrap — HitTestIndex.cpp Full Implementation
// =============================================================================
// Simple linear hit test: iterate surfaces top-to-bottom (last in list = top),
// call each surface's hitTest(), return first hit. P0: no spatial index,
// mark dirty when document changes.

#include "tcxMapWrap/HitTestIndex.h"

namespace tcx {
namespace mapwrap {

void HitTestIndex::rebuild(const MapWrapDocument& document) {
    document_ = &document;
    dirty_ = false;
}

HitResult HitTestIndex::query(Vec2 canvasNorm, const HitTestOptions& options) const {
    HitResult result;
    result.canvasNormPos = canvasNorm;

    if (!document_) return result;

    const auto& surfaces = document_->surfaces();

    // Iterate top-to-bottom: last surface in the list is drawn on top,
    // so we check it first for hit priority.
    for (int i = (int)surfaces.size() - 1; i >= 0; --i) {
        const auto& surface = surfaces[i];
        if (!surface) continue;

        // Skip invisible surfaces unless explicitly requested
        if (!surface->isVisible() && !options.includeInvisible) continue;

        // Skip locked surfaces unless explicitly requested
        if (surface->isLocked() && !options.includeLocked) continue;

        HitResult hr = surface->hitTest(canvasNorm, options);
        if (hr.hit) return hr;
    }

    return result;
}

void HitTestIndex::markDirty() { dirty_ = true; }
bool HitTestIndex::isDirty() const { return dirty_; }

} // namespace mapwrap
} // namespace tcx
