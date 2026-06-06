#pragma once
// =============================================================================
// tcxMapWrap — SurfaceQuad
// =============================================================================

#include "tcxMapWrap/Surface.h"
#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfaceQuad : public Surface {
public:
    SurfaceQuad();
    SurfaceKind kind() const override { return SurfaceKind::Quad; }
    std::unique_ptr<Surface> clone() const override;

    std::array<Vec2, 4>& destinationPoints();
    const std::array<Vec2, 4>& destinationPoints() const;
    void setDestinationPoint(int index, Vec2 pos);
    void setDestinationPoints(const std::array<Vec2, 4>& points);

    std::array<Vec2, 4>& uvPoints();
    const std::array<Vec2, 4>& uvPoints() const;
    void setUvPoint(int index, Vec2 uv);
    void setUvPoints(const std::array<Vec2, 4>& points);

    bool perspectiveCorrection() const;
    void setPerspectiveCorrection(bool enabled);

    int meshResolution() const;
    void setMeshResolution(int resolution);

    void resetToCanvas();
    void resetToRect(Rect normRect);
    void rotateCW();
    void rotateCCW();
    void flipHorizontal();
    void flipVertical();
    void expandToCanvas();

    MeshBuildResult buildMesh(const MeshBuildContext& ctx) override;
    HitResult hitTest(const Vec2& canvasNormPos, const HitTestOptions& options) const override;
    GeometryValidation validateGeometry() const override;

private:
    std::array<Vec2, 4> dest_ = {{ Vec2(0.1f,0.1f), Vec2(0.9f,0.1f), Vec2(0.9f,0.9f), Vec2(0.1f,0.9f) }};
    std::array<Vec2, 4> uv_ = {{ Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1) }};
    bool perspective_ = true;
    int meshResolution_ = 16;
};

} // namespace mapwrap
} // namespace tcx
