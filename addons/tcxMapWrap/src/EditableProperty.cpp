// =============================================================================
// tcxMapWrap — EditableProperty.cpp Implementation
// =============================================================================

#include "tcxMapWrap/EditableProperty.h"

#include "tcxMapWrap/MapWrapI18n.h"

namespace tcx {
namespace mapwrap {

EditableProperty EditableProperty::fromI18n(const std::string& path,
                                             const std::string& i18nKey,
                                             PropertyKind kind,
                                             const std::string& value,
                                             bool readOnly) {
    EditableProperty p;
    p.path = path;
    p.label = MapWrapI18n::instance().tr(i18nKey);
    p.kind = kind;
    p.value = value;
    p.readOnly = readOnly;
    return p;
}

EditableProperty EditableProperty::fromI18n(const std::string& path,
                                             const std::string& i18nKey,
                                             PropertyKind kind,
                                             const std::string& value,
                                             const std::string& minVal,
                                             const std::string& maxVal,
                                             bool readOnly) {
    EditableProperty p;
    p.path = path;
    p.label = MapWrapI18n::instance().tr(i18nKey);
    p.kind = kind;
    p.value = value;
    p.minValue = minVal;
    p.maxValue = maxVal;
    p.readOnly = readOnly;
    return p;
}

} // namespace mapwrap
} // namespace tcx
