#pragma once
// =============================================================================
// tcxMapWrap — WarpGrid
// =============================================================================

#include "tcxMapWrap/Warp.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

Vec2 bilinearInterpolate(Vec2 p00, Vec2 p10, Vec2 p01, Vec2 p11, float u, float v);
Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t);

class WarpGrid : public Warp {
public:
    WarpKind kind() const override { return WarpKind::Grid; }
    void reset() override;
};

} // namespace mapwrap
} // namespace tcx
