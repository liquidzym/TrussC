#pragma once
// =============================================================================
// tcxMapWrap — SurfaceGroup
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfaceGroup {
public:
    GroupId id;
    std::string name;

    bool visible = true;
    bool locked = false;
    float opacity = 1.0f;

    Vec2 translate = Vec2(0, 0);
    float rotation = 0.0f;
    Vec2 scale = Vec2(1, 1);

    std::vector<SurfaceId> surfaceIds;
};

} // namespace mapwrap
} // namespace tcx
