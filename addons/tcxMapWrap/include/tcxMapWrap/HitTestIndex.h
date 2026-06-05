#pragma once
// =============================================================================
// tcxMapWrap — HitTestIndex
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapDocument.h"

namespace tcx {
namespace mapwrap {

class HitTestIndex {
public:
    void rebuild(const MapWrapDocument& document);
    HitResult query(Vec2 canvasNorm, const HitTestOptions& options) const;

    void markDirty();
    bool isDirty() const;

private:
    bool dirty_ = true;
    const MapWrapDocument* document_ = nullptr;
};

} // namespace mapwrap
} // namespace tcx
