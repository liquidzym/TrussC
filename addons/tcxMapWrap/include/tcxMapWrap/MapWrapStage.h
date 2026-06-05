#pragma once
// =============================================================================
// tcxMapWrap — MapWrapStage
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapOutput.h"
#include "tcxMapWrap/MapWrapMask.h"

namespace tcx {
namespace mapwrap {

class MapWrapStage {
public:
    MapWrapStage();
    ~MapWrapStage();

    Vec2 designCanvasSize() const;
    void setDesignCanvasSize(Vec2 size);

    // Outputs
    std::vector<MapWrapOutput>& outputs();
    const std::vector<MapWrapOutput>& outputs() const;
    OutputId defaultOutputId() const;
    MapWrapOutput* getOutput(const OutputId& id);
    const MapWrapOutput* getOutput(const OutputId& id) const;
    MapWrapOutput& ensureDefaultOutput();

    // Global masks
    std::vector<MapWrapMask>& globalMasks();
    const std::vector<MapWrapMask>& globalMasks() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
