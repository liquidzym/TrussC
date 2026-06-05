#pragma once
// =============================================================================
// tcxMapWrap — MapWrapSerialization
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"
#include "tcxMapWrap/MapWrapDocument.h"
#include "tcxMapWrap/SourceRegistry.h"

namespace tcx {
namespace mapwrap {

class MapWrapSerialization {
public:
    // Save / Load to string
    static std::string saveToString(const MapWrapDocument& document);
    static std::string saveToString(const MapWrapDocument& document, const SourceRegistry& sources);
    static LoadResult loadFromString(MapWrapDocument& document, const std::string& json);
    static LoadResult loadFromString(MapWrapDocument& document, SourceRegistry& sources, const std::string& json);

    // Save / Load to file
    static Result saveToFile(const MapWrapDocument& document, const std::string& path);
    static Result saveToFile(const MapWrapDocument& document, const SourceRegistry& sources, const std::string& path);
    static LoadResult loadFromFile(MapWrapDocument& document, const std::string& path);
    static LoadResult loadFromFile(MapWrapDocument& document, SourceRegistry& sources, const std::string& path);

    // Schema info
    static const char* schemaName() { return "tcxMapWrap.composition"; }
    static int schemaVersion() { return 1; }
};

} // namespace mapwrap
} // namespace tcx
