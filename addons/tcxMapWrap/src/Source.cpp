// =============================================================================
// tcxMapWrap — Source.cpp Implementation
// =============================================================================

#include "tcxMapWrap/Source.h"
#include "tcxMapWrap/MapWrapI18n.h"

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Color correction
// ---------------------------------------------------------------------------

ColorCorrection Source::colorCorrection() const {
    return colorCorrection_;
}

void Source::setColorCorrection(const ColorCorrection& correction) {
    colorCorrection_ = correction;
}

// ---------------------------------------------------------------------------
// kindName — localized display name for the source kind
// ---------------------------------------------------------------------------

std::string Source::kindName() const {
    switch (kind()) {
        case SourceKind::Texture:        return MapWrapI18n::instance().tr("source.texture");
        case SourceKind::Fbo:            return MapWrapI18n::instance().tr("source.fbo");
        case SourceKind::Video:          return MapWrapI18n::instance().tr("source.video");
        case SourceKind::Image:          return MapWrapI18n::instance().tr("source.image");
        case SourceKind::Generated:      return MapWrapI18n::instance().tr("source.generated");
        case SourceKind::BuiltinPattern: return MapWrapI18n::instance().tr("source.builtin_pattern");
        case SourceKind::None:           return MapWrapI18n::instance().tr("source.none");
        default:                         return "";
    }
}

} // namespace mapwrap
} // namespace tcx
