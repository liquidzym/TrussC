#pragma once
// =============================================================================
// tcxMapWrap — MapWrapAutosave
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapDocument.h"

namespace tcx {
namespace mapwrap {

class MapWrapAutosave {
public:
    MapWrapAutosave();
    ~MapWrapAutosave();
    MapWrapAutosave(const MapWrapAutosave&) = delete;
    MapWrapAutosave& operator=(const MapWrapAutosave&) = delete;
    MapWrapAutosave(MapWrapAutosave&&) = delete;
    MapWrapAutosave& operator=(MapWrapAutosave&&) = delete;
    void setup(MapWrapDocument* document, AutosaveSettings settings);
    void update(float dt);
    Result forceSave();
    std::vector<std::string> listRecoverableFiles() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mapwrap
} // namespace tcx
