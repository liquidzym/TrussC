#pragma once
// =============================================================================
// tcxMapWrap — MapWrapOutput
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapMask.h"

namespace tcx {
namespace mapwrap {

struct MapWrapOutput {
    OutputId id;
    std::string name = "Main Output";

    Rect canvasRegionNorm = Rect(0, 0, 1, 1);
    Rect displayRegionPixels = Rect(0, 0, 1920, 1080);

    Vec2 pixelSize = Vec2(1920, 1080);
    float contentScale = 1.0f;
    float rotationDegrees = 0.0f;

    bool enabled = true;
    bool showTestPattern = false;

    BlendSettings blend;
    ColorCorrection colorCorrection;
    std::vector<MapWrapMask> masks;
};

} // namespace mapwrap
} // namespace tcx
