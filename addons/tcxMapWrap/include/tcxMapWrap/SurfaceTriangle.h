#pragma once
// =============================================================================
// tcxMapWrap — SurfaceTriangle
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfaceTriangle : public Surface {
public:
    SurfaceTriangle();
    SurfaceKind kind() const override { return SurfaceKind::Triangle; }

    std::array<Vec2, 3>& destinationPoints();
    const std::array<Vec2, 3>& destinationPoints() const;

    std::array<Vec2, 3>& uvPoints();
    const std::array<Vec2, 3>& uvPoints() const;

    MeshBuildResult buildMesh(const MeshBuildContext& ctx) override;
    HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const override;
    GeometryValidation validateGeometry() const override;

private:
    std::array<Vec2, 3> dest_ = {{ Vec2(0.5f,0.1f), Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};
    std::array<Vec2, 3> uv_ = {{ Vec2(0.5f,0), Vec2(1,1), Vec2(0,1) }};
};

} // namespace mapwrap
} // namespace tcx
