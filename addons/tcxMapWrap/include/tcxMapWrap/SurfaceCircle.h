#pragma once
// =============================================================================
// tcxMapWrap — SurfaceCircle
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfaceCircle : public Surface {
public:
    SurfaceCircle();
    SurfaceKind kind() const override { return SurfaceKind::Circle; }

    Vec2 center() const;
    void setCenter(Vec2 center);
    float radiusX() const;
    void setRadiusX(float rx);
    float radiusY() const;
    void setRadiusY(float ry);
    float rotation() const;
    void setRotation(float deg);
    int segments() const;
    void setSegments(int segments);

    MeshBuildResult buildMesh(const MeshBuildContext& ctx) override;
    HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const override;
    GeometryValidation validateGeometry() const override;

private:
    Vec2 center_ = Vec2(0.5f, 0.5f);
    float radiusX_ = 0.3f;
    float radiusY_ = 0.3f;
    float rotation_ = 0.0f;
    int segments_ = 64;
};

} // namespace mapwrap
} // namespace tcx
