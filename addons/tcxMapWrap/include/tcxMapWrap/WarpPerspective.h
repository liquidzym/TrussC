#pragma once
// =============================================================================
// tcxMapWrap — WarpPerspective
// =============================================================================

#include "tcxMapWrap/Warp.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

ResultT<Mat3> computeHomography(
    const std::array<Vec2, 4>& src,
    const std::array<Vec2, 4>& dst
);

class WarpPerspective : public Warp {
public:
    WarpKind kind() const override { return WarpKind::Perspective; }
    void reset() override;

    std::array<Vec2, 4>& srcPoints();
    const std::array<Vec2, 4>& srcPoints() const;
    std::array<Vec2, 4>& dstPoints();
    const std::array<Vec2, 4>& dstPoints() const;

    ResultT<Mat3> computeMatrix() const;

private:
    std::array<Vec2, 4> src_ = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    std::array<Vec2, 4> dst_ = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
};

} // namespace mapwrap
} // namespace tcx
