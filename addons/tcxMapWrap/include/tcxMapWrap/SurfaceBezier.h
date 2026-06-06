#pragma once
// =============================================================================
// tcxMapWrap — SurfaceBezier
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfaceBezier : public Surface {
public:
    SurfaceBezier(int controlCols = 4, int controlRows = 4);
    SurfaceKind kind() const override { return SurfaceKind::Bezier; }
    std::unique_ptr<Surface> clone() const override;

    int controlCols() const;
    int controlRows() const;
    void setControlDimensions(int cols, int rows);

    Vec2 controlPoint(int col, int row) const;
    void setControlPoint(int col, int row, Vec2 pos);

    std::vector<Vec2>& controlPoints();
    const std::vector<Vec2>& controlPoints() const;
    void setControlPoints(const std::vector<Vec2>& points);

    int meshResolution() const;
    void setMeshResolution(int resolution);

    Vec2 evaluate(float u, float v) const;

    MeshBuildResult buildMesh(const MeshBuildContext& ctx) override;
    HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const override;
    GeometryValidation validateGeometry() const override;

private:
    int controlCols_ = 4;
    int controlRows_ = 4;
    int meshResolution_ = 24;
    std::vector<Vec2> controlPoints_;

    void resetControlPoints();
};

} // namespace mapwrap
} // namespace tcx
