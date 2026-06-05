#pragma once
// =============================================================================
// tcxMapWrap — ProjectPackaging
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapDocument.h"

namespace tcx {
namespace mapwrap {

class ProjectPackaging {
public:
    static ProjectValidationReport validateProject(const MapWrapDocument& document);
    static Result collectMediaToFolder(const MapWrapDocument& document, const std::string& targetFolder);
    static Result relinkSource(MapWrapDocument& document, const SourceId& id, const std::string& newPath);
};

} // namespace mapwrap
} // namespace tcx
