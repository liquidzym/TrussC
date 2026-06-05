// =============================================================================
// tcxMapWrap — ProjectPackaging.cpp Implementation
// =============================================================================

#include "tcxMapWrap/ProjectPackaging.h"

namespace tcx {
namespace mapwrap {

ProjectValidationReport ProjectPackaging::validateProject(const MapWrapDocument& doc) {
    return doc.validateProject();
}

Result ProjectPackaging::collectMediaToFolder(const MapWrapDocument& doc, const std::string& targetFolder) {
    return Result::error("Not implemented");
}

Result ProjectPackaging::relinkSource(MapWrapDocument& doc, const SourceId& id, const std::string& newPath) {
    return Result::error("Not implemented");
}

} // namespace mapwrap
} // namespace tcx
