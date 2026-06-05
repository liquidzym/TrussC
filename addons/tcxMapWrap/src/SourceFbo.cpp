// =============================================================================
// tcxMapWrap — SourceFbo.cpp Implementation
// =============================================================================

#include "tcxMapWrap/SourceFbo.h"
#include "tcxMapWrap/MapWrapI18n.h"

namespace tcx {
namespace mapwrap {

// ---------------------------------------------------------------------------
// Source interface
// ---------------------------------------------------------------------------

SourceId SourceFbo::id() const {
    return id_;
}

std::string SourceFbo::name() const {
    return name_;
}

Vec2 SourceFbo::size() const {
    return size_;
}

// ---------------------------------------------------------------------------
// FBO-specific
// ---------------------------------------------------------------------------

void SourceFbo::setFbo(void* fbo, Vec2 size) {
    // The raw FBO pointer is owned externally, typically by TrussC's Fbo
    // or the host application. SourceFbo only references it.
    fbo_ = fbo;
    size_ = size;
}

void* SourceFbo::fbo() const {
    return fbo_;
}

bool SourceFbo::hasFbo() const {
    return fbo_ != nullptr;
}

std::string SourceFbo::kindName() const {
    return MapWrapI18n::instance().tr("source.fbo");
}

} // namespace mapwrap
} // namespace tcx
