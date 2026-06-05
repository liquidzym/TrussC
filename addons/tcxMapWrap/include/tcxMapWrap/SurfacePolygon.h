#pragma once
// =============================================================================
// tcxMapWrap — SurfacePolygon
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfacePolygon : public Surface {
public:
    SurfacePolygon();
    SurfaceKind kind() const override { return SurfaceKind::Polygon; }

    std::vector<Vec2>& destinationPoints();
    const std::vector<Vec2>& destinationPoints() const;

    std::vector<Vec2>& uvPoints();
    const std::vector<Vec2>& uvPoints() const;

    bool closed() const;
    void setClosed(bool closed);

    void addPoint(Vec2 p);
    void removePoint(size_t index);
    void movePoint(size_t index, Vec2 p);

    bool triangulate();

    MeshBuildResult buildMesh(const MeshBuildContext& ctx) override;
    HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const override;
    GeometryValidation validateGeometry() const override;

private:
    std::vector<Vec2> destPoints_;
    std::vector<Vec2> uvPoints_;
    bool closed_ = true;
};

} // namespace mapwrap
} // namespace tcx
