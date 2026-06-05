#pragma once
// =============================================================================
// tcxMapWrap — MapWrapCue
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

struct SurfaceStatePatch {
    SurfaceId surfaceId;
    std::string patchJson;
};

struct SourceStatePatch {
    SourceId sourceId;
    std::string patchJson;
};

struct OutputStatePatch {
    OutputId outputId;
    std::string patchJson;
};

class MapWrapCue {
public:
    CueId id;
    std::string name;
    std::vector<SurfaceStatePatch> surfacePatches;
    std::vector<SourceStatePatch> sourcePatches;
    std::vector<OutputStatePatch> outputPatches;
    float transitionSeconds = 0.0f;
};

} // namespace mapwrap
} // namespace tcx
