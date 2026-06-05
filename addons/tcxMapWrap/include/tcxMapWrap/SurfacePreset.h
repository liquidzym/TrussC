#pragma once
// =============================================================================
// tcxMapWrap — SurfacePreset
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

class SurfacePreset {
public:
    std::string id;
    std::string name;
    SurfaceKind kind;
    std::string surfaceTemplateJson;
};

} // namespace mapwrap
} // namespace tcx
