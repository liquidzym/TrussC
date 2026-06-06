#pragma once
// =============================================================================
// tcxMapWrap — SurfaceGrid
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfaceGrid : public Surface {
public:
    SurfaceGrid(int cols = 3, int rows = 3);
    SurfaceKind kind() const override { return SurfaceKind::Grid; }
    std::unique_ptr<Surface> clone() const override;

    int cols() const;
    int rows() const;
    void setCols(int cols);
    void setRows(int rows);

    Vec2 gridPoint(int col, int row) const;
    void setGridPoint(int col, int row, Vec2 pos);

    void addColumn();
    void removeColumn();
    void addRow();
    void removeRow();

    bool curvedInterpolation() const;
    void setCurvedInterpolation(bool curved);

    int meshResolution() const;
    void setMeshResolution(int resolution);

    MeshBuildResult buildMesh(const MeshBuildContext& ctx) override;
    HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const override;
    GeometryValidation validateGeometry() const override;

private:
    int cols_ = 3;
    int rows_ = 3;
    std::vector<Vec2> points_;
    bool curved_ = false;
    int meshResolution_ = 1;
};

} // namespace mapwrap
} // namespace tcx
