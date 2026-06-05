#pragma once
// =============================================================================
// tcxMapWrap — EditableProperty
// =============================================================================

#include "tcxMapWrap/MapWrapTypes.h"

namespace tcx {
namespace mapwrap {

struct EditableProperty {
    std::string path;
    std::string label;       // Localized display label
    PropertyKind kind;
    std::string value;       // JSON-serialized value
    std::string minValue;    // JSON-serialized, empty if not applicable
    std::string maxValue;    // JSON-serialized, empty if not applicable
    bool readOnly = false;

    // Helper to create with localized label from i18n key
    static EditableProperty fromI18n(const std::string& path,
                                      const std::string& i18nKey,
                                      PropertyKind kind,
                                      const std::string& value,
                                      bool readOnly = false);

    static EditableProperty fromI18n(const std::string& path,
                                      const std::string& i18nKey,
                                      PropertyKind kind,
                                      const std::string& value,
                                      const std::string& minVal,
                                      const std::string& maxVal,
                                      bool readOnly = false);
};

} // namespace mapwrap
} // namespace tcx
