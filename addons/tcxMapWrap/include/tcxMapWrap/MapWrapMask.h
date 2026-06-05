#pragma once
// =============================================================================
// tcxMapWrap — MapWrapMask
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapInput.h"

namespace tcx {
namespace mapwrap {

class MapWrapMask {
public:
    MaskId id;
    std::string name;

    MaskKind kind = MaskKind::Polygon;
    MaskOperation operation = MaskOperation::Add;
    MaskSpace space = MaskSpace::SurfaceLocal;

    bool enabled = true;
    bool inverted = false;

    float opacity = 1.0f;
    float featherPixels = 0.0f;
    float featherNorm = 0.0f;

    std::vector<Vec2> points;
    Rect rect;
    SourceId alphaTextureSource;

    GeometryValidation validateGeometry() const;
    HitResult hitTest(const Vec2& pos, const HitTestOptions& options) const;

    // Localized name helpers
    std::string kindName() const;
    std::string operationName() const;
    std::string spaceName() const;
};

} // namespace mapwrap
} // namespace tcx
